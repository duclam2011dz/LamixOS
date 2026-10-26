# SPDX-License-Identifier: GPL-3.0-only
"""Tests host bằng tiến trình fixture và dữ liệu giả; không tạo bootable image."""

from concurrent.futures import ThreadPoolExecutor
from dataclasses import replace
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
from unittest.mock import patch

import pytest

from scripts.qemu_harness import Config, QMP, SerialScanner, build_command, run

FIXTURE = Path(__file__).with_name("fixture_process.py")
ROOT = Path(__file__).resolve().parents[2]


@pytest.fixture
def config(tmp_path):
    # Chỉ là dữ liệu fixture để kiểm tra validation/command; không đưa vào QEMU thật.
    for name in ("image.bin", "bios.bin", "code.bin", "vars.bin"):
        (tmp_path / name).write_bytes(b"fixture data")
    return Config(image=tmp_path / "image.bin", bios=tmp_path / "bios.bin",
                  ovmf_code=tmp_path / "code.bin", ovmf_vars=tmp_path / "vars.bin",
                  success_marker="SUCCESS", panic_marker="PANIC", timeout=3,
                  output_root=ROOT / "out/m02")


def factory(scenario):
    def command(config, directory):
        build_command(config, directory)
        return [sys.executable, str(FIXTURE), "--directory", str(directory), "--scenario", scenario] + (["--launch"] if config.launch_only else [])
    return command


def assert_reaped(result):
    with pytest.raises(ChildProcessError):
        os.waitpid(result.pid, os.WNOHANG)
    with pytest.raises(ProcessLookupError):
        os.killpg(result.pid, 0)


@pytest.mark.parametrize("scenario", ["good", "split", "crlf", "flood", "flood-on-quit", "natural-exit"])
def test_success_serial_and_output(config, scenario):
    result = run(config, factory(scenario))
    assert result.status == "PASS", result
    assert result.returncode == 0
    assert_reaped(result)
    saved = json.loads((Path(result.directory) / "result.json").read_text())
    assert saved["status"] == "PASS"
    assert saved["backend"] == "fixture"
    if scenario == "flood":
        assert (Path(result.directory) / "stderr.log").stat().st_size >= 300000


@pytest.mark.parametrize("scenario", ["panic-before", "panic-after"])
def test_panic_overrides_success_including_final_drain(config, scenario):
    result = run(config, factory(scenario))
    assert result.status == "PANIC", result
    assert result.exit_code == 1
    assert_reaped(result)


def test_natural_exit_before_host_reads_final_qmp_response(config):
    original = QMP.send

    def send(self, name, identity):
        original(self, name, identity)
        if name == "query-status":
            self.process.wait(timeout=2)  # handshake deterministic, không sleep tùy ý.

    with patch.object(QMP, "send", send):
        result = run(config, factory("natural-exit"))
    assert result.status == "PASS", result
    assert_reaped(result)


@pytest.mark.parametrize("scenario", ["exit0", "exit1"])
def test_exit_is_not_hidden_by_zero_code_or_success(config, scenario):
    result = run(config, factory(scenario))
    assert result.status == "EXIT", result
    assert_reaped(result)


@pytest.mark.parametrize("scenario", ["no-greeting", "no-response", "hang"])
def test_timeout_and_cleanup(config, scenario):
    result = run(replace(config, timeout=0.5), factory(scenario))
    assert result.status == "TIMEOUT", result
    assert result.elapsed < 5
    assert_reaped(result)


@pytest.mark.parametrize("scenario", ["bad-greeting", "bad-response"])
def test_protocol_failure(config, scenario):
    result = run(config, factory(scenario))
    assert result.status == "PROTOCOL_ERROR", result
    assert_reaped(result)


def test_launch_requires_paused_vm(config):
    result = run(replace(config, launch_only=True, image=None), factory("running"))
    assert result.status == "PROTOCOL_ERROR", result
    assert_reaped(result)


def test_forced_shutdown_cannot_pass(config):
    result = run(config, factory("ignore-quit"))
    assert result.status == "CLEANUP_ERROR", result
    assert result.cleanup_forced
    assert_reaped(result)


def test_broken_stdin_still_writes_result_and_reaps(config):
    result = run(config, factory("broken-stdin"))
    assert result.status == "CLEANUP_ERROR", result
    assert json.loads((Path(result.directory) / "result.json").read_text())["status"] == "CLEANUP_ERROR"
    assert_reaped(result)


def test_child_cleanup_on_success(config):
    result = run(config, factory("child"))
    assert result.status == "PASS", result
    pid = int((Path(result.directory) / "child.pid").read_text())
    assert not Path(f"/proc/{pid}").exists()
    assert_reaped(result)


def test_host_exception_still_cleans_group(config):
    original = QMP.pump

    def pump(self, timeout):
        original(self, timeout)
        if self.ready:
            raise RuntimeError("injected fixture exception")

    with patch.object(QMP, "pump", pump):
        result = run(config, factory("child"))
    assert result.status == "PROTOCOL_ERROR", result
    assert "injected" in result.detail
    pid = int((Path(result.directory) / "child.pid").read_text())
    assert not Path(f"/proc/{pid}").exists()
    assert_reaped(result)


def test_launch_failure_is_reported(config):
    result = run(config, lambda c, d: ["/missing/qemu"])
    assert result.status == "LAUNCH_ERROR"


def test_no_image_skips_but_explicit_missing_image_fails(config):
    skipped = run(replace(config, image=None))
    assert skipped.status == "SKIPPED" and skipped.exit_code == 77
    failed = run(replace(config, image=Path("/missing/image")))
    assert failed.status == "LAUNCH_ERROR" and failed.exit_code == 1


@pytest.mark.parametrize("changes", [{"success_marker": None}, {"panic_marker": "SUCCESS"}, {"timeout": 0}, {"timeout": float("nan")}, {"image": Path("/dev/null")}])
def test_invalid_boot_config_fails(config, changes):
    assert run(replace(config, **changes)).status == "LAUNCH_ERROR"


def test_command_is_snapshot_and_firmware_is_readonly(config, tmp_path):
    command = build_command(config, tmp_path)
    assert "snapshot=on" in " ".join(command)
    assert "-accel tcg" in " ".join(command)
    command = build_command(replace(config, profile="uefi"), tmp_path)
    assert "unit=0,readonly=on" in " ".join(command)


def test_parallel_runs_have_private_vars_and_preserve_originals(config):
    names = [config.image, config.ovmf_code, config.ovmf_vars, config.bios]
    before = {p: hashlib.sha256(p.read_bytes()).hexdigest() for p in names}
    with ThreadPoolExecutor(max_workers=2) as pool:
        futures = [pool.submit(run, replace(config, profile="uefi"), factory("write-vars")) for _ in range(2)]
        results = [future.result(timeout=10) for future in futures]
    assert all(result.status == "PASS" for result in results), results
    assert results[0].directory != results[1].directory
    for result in results:
        assert (Path(result.directory) / "OVMF_VARS.fd").read_bytes() == b"private fixture write"
        assert_reaped(result)
    assert before == {p: hashlib.sha256(p.read_bytes()).hexdigest() for p in names}


def test_ovmf_override_requires_pair(config):
    assert run(replace(config, profile="uefi", ovmf_vars=None)).status == "LAUNCH_ERROR"


def test_serial_split_is_observed_before_second_chunk(tmp_path):
    path = tmp_path / "serial.log"
    path.write_bytes(b"SUCC")
    scanner = SerialScanner(path, "SUCCESS", "PANIC")
    try:
        scanner.scan()
        assert not scanner.success
        with path.open("ab") as output:
            output.write(b"ESS\r\n")
        scanner.scan()
        assert scanner.success and not scanner.panic
    finally:
        scanner.close()


def test_cli_boot_skip_is_77(tmp_path):
    env = os.environ.copy()
    for key in ("LAMIX_TEST_IMAGE", "LAMIX_TEST_SUCCESS_MARKER", "LAMIX_TEST_PANIC_MARKER"):
        env.pop(key, None)
    result = subprocess.run([sys.executable, str(ROOT / "scripts/run_qemu.py"), "--profile", "bios", "--output-root", str(tmp_path)], env=env, capture_output=True, text=True, timeout=5)
    assert result.returncode == 77 and "SKIPPED" in result.stdout
