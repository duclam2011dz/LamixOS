# SPDX-License-Identifier: GPL-3.0-only
"""Host harness M02: QMP/serial, deadlines và cleanup; không phải boot code."""

from dataclasses import asdict, dataclass
import json
import math
import os
from pathlib import Path
import selectors
import shutil
import signal
import stat
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]


@dataclass
class Config:
    profile: str = "bios"
    launch_only: bool = False
    image: Path | None = None
    success_marker: str | None = None
    panic_marker: str | None = None
    timeout: float | None = None
    output_root: Path = ROOT / "out/m02"
    qemu: str = "qemu-system-x86_64"
    bios: Path = Path("/usr/share/seabios/bios.bin")
    ovmf_code: Path | None = None
    ovmf_vars: Path | None = None


@dataclass
class Result:
    profile: str
    mode: str
    directory: str
    backend: str = "qemu"
    status: str = "LAUNCH_ERROR"
    detail: str = "chưa khởi chạy"
    returncode: int | None = None
    elapsed: float = 0
    pid: int | None = None
    cleanup_forced: bool = False
    cleanup_error: str | None = None
    command: list | None = None

    @property
    def exit_code(self):
        return 0 if self.status == "PASS" else 77 if self.status == "SKIPPED" else 1


class ProtocolError(ValueError):
    pass


class DeadlineExpired(TimeoutError):
    pass


def regular_file(path):
    path = Path(path).resolve(strict=True)
    if not stat.S_ISREG(path.stat().st_mode):
        raise ValueError(f"cần regular file, không dùng device: {path}")
    return path


def qemu_value(path):
    # QEMU key=value dùng dấu phẩy kép để escape dấu phẩy trong tên file.
    return str(path).replace(",", ",,")


def build_command(config, directory):
    command = [config.qemu, "-machine", "pc" if config.profile == "bios" else "q35",
               "-accel", "tcg", "-smp", "1", "-m", "256M", "-display", "none",
               "-nic", "none", "-monitor", "none", "-no-reboot", "-no-shutdown",
               "-chardev", f"file,id=serial0,path={qemu_value(directory / 'serial.log')}",
               "-serial", "chardev:serial0", "-qmp", "stdio"]
    if config.profile == "bios":
        command += ["-bios", str(regular_file(config.bios))]
    else:
        if (config.ovmf_code is None) != (config.ovmf_vars is None):
            raise ValueError("override OVMF phải có cả CODE và VARS cùng biến thể")
        code = regular_file(config.ovmf_code or "/usr/share/OVMF/OVMF_CODE.fd")
        variables = regular_file(config.ovmf_vars or "/usr/share/OVMF/OVMF_VARS.fd")
        private_vars = directory / "OVMF_VARS.fd"
        shutil.copyfile(variables, private_vars)
        command += ["-drive", f"if=pflash,format=raw,unit=0,readonly=on,file={qemu_value(code)}",
                    "-drive", f"if=pflash,format=raw,unit=1,file={qemu_value(private_vars)}"]
    if config.launch_only:
        command += ["-S"]
    else:
        image = regular_file(config.image)
        command += ["-drive", f"file={qemu_value(image)},format=raw,if=ide,snapshot=on"]
    return command


class SerialScanner:
    """Theo dõi literal line bằng bộ đệm giới hạn, raw log vẫn giữ đầy đủ."""

    def __init__(self, path, success, panic):
        self.file = path.open("rb")
        self.markers = [value.encode("utf-8") if value else None for value in (success, panic)]
        self.success = False
        self.panic = False
        self.buffer = bytearray()
        self.discard = False

    def line(self):
        if not self.discard:
            line = bytes(self.buffer).removesuffix(b"\r")
            self.success |= self.markers[0] is not None and line == self.markers[0]
            self.panic |= self.markers[1] is not None and line == self.markers[1]
        self.buffer.clear()
        self.discard = False

    def scan(self, final=False):
        # Giới hạn công việc mỗi vòng để serial flood không chiếm mất deadline/QMP.
        for _ in range(64):
            block = self.file.read(4096)
            if not block:
                break
            for value in block:
                if value == 10:
                    self.line()
                elif not self.discard:
                    self.buffer.append(value)
                    if len(self.buffer) > 65538:
                        self.buffer.clear()
                        self.discard = True
        if final:
            end = os.fstat(self.file.fileno()).st_size
            while self.file.tell() < end:
                previous = self.file.tell()
                self.scan()
                if self.file.tell() == previous:
                    break
            self.line()

    def close(self):
        self.file.close()


class QMP:
    def __init__(self, process, transcript):
        self.process = process
        self.transcript = transcript
        self.selector = selectors.DefaultSelector()
        os.set_blocking(process.stdout.fileno(), False)
        self.selector.register(process.stdout, selectors.EVENT_READ)
        self.buffer = bytearray()
        self.greeted = False
        self.negotiated = False
        self.ready = False
        self.running = None
        self.eof = False

    def send(self, name, identity):
        message = json.dumps({"execute": name, "id": identity}).encode() + b"\n"
        self.transcript.write(b"CLIENT " + message)
        self.transcript.flush()
        self.process.stdin.write(message)
        self.process.stdin.flush()

    def pump(self, timeout):
        if not self.selector.select(timeout):
            return False
        block = os.read(self.process.stdout.fileno(), 4096)
        if not block:
            self.eof = True
            self.selector.unregister(self.process.stdout)
            return False
        self.transcript.write(block)
        self.transcript.flush()
        self.buffer.extend(block)
        while b"\n" in self.buffer:
            line, _, remainder = self.buffer.partition(b"\n")
            self.buffer = bytearray(remainder)
            try:
                message = json.loads(line)
            except (ValueError, UnicodeError) as exc:
                raise ProtocolError("QMP JSON không hợp lệ") from exc
            if not isinstance(message, dict):
                raise ProtocolError("QMP response phải là object")
            if "event" in message:
                continue
            if not self.greeted:
                if not isinstance(message.get("QMP"), dict):
                    raise ProtocolError("thiếu QMP greeting")
                self.greeted = True
                self.send("qmp_capabilities", 1)
            elif message.get("id") in (1, 2):
                if "error" in message or "return" not in message:
                    raise ProtocolError("QMP command thất bại")
                if message["id"] == 1 and not self.negotiated:
                    self.negotiated = True
                    self.send("query-status", 2)
                elif message["id"] == 2 and self.negotiated:
                    value = message["return"]
                    if not isinstance(value, dict) or not isinstance(value.get("running"), bool):
                        raise ProtocolError("query-status thiếu running boolean")
                    self.running = value["running"]
                    self.ready = True
        if len(self.buffer) > 1024 * 1024:
            raise ProtocolError("QMP record vượt giới hạn 1 MiB")
        return True

    def close(self):
        self.selector.close()

    def drain(self, timeout=0):
        """Sau quit vẫn tiêu thụ stdout để QEMU không kẹt vì pipe đầy."""
        if self.selector.select(timeout):
            for _ in range(64):
                try:
                    block = os.read(self.process.stdout.fileno(), 65536)
                except BlockingIOError:
                    break
                if not block:
                    break
                self.transcript.write(block)
            self.transcript.flush()


def group_alive(pid):
    try:
        os.killpg(pid, 0)
        return True
    except ProcessLookupError:
        return False


def cleanup(process, qmp, grace=2):
    forced = False
    try:
        if process.poll() is None and qmp and qmp.negotiated:
            try:
                qmp.send("quit", 3)
                deadline = time.monotonic() + grace
                while process.poll() is None and time.monotonic() < deadline:
                    qmp.drain(min(0.02, max(0, deadline - time.monotonic())))
                qmp.drain()
            except (BrokenPipeError, OSError, subprocess.TimeoutExpired):
                pass
        if process.poll() is None or group_alive(process.pid):
            forced = True
            for sig in (signal.SIGTERM, signal.SIGKILL):
                try:
                    os.killpg(process.pid, sig)
                except ProcessLookupError:
                    pass
                deadline = time.monotonic() + grace
                while time.monotonic() < deadline:
                    process.poll()  # reap tiến trình trực tiếp trước khi kiểm tra group.
                    if not group_alive(process.pid):
                        break
                    time.sleep(0.01)
                if not group_alive(process.pid):
                    break
        process.wait(timeout=grace)
        if group_alive(process.pid):
            return forced, "process group chưa được thu hồi"
        return forced, None
    except (OSError, subprocess.TimeoutExpired) as exc:
        return forced, str(exc)


def run(config, command_factory=build_command):
    started = time.monotonic()
    output = Path(config.output_root).resolve()
    output.mkdir(parents=True, exist_ok=True)
    directory = Path(tempfile.mkdtemp(prefix="run-", dir=output))
    result = Result(config.profile, "launch-only" if config.launch_only else "boot", str(directory))
    if command_factory is not build_command:
        result.backend = "fixture"
    serial = directory / "serial.log"
    serial.touch()
    (directory / "stderr.log").touch()
    (directory / "qmp.log").touch()
    process = qmp = scanner = None
    stderr = transcript = None
    try:
        if config.profile not in ("bios", "uefi"):
            raise ValueError("profile phải là bios hoặc uefi")
        timeout = config.timeout if config.timeout is not None else (10 if config.launch_only else 30)
        if not math.isfinite(timeout) or timeout <= 0:
            raise ValueError("timeout phải hữu hạn và > 0")
        if not config.launch_only and config.image is None:
            result.status, result.detail = "SKIPPED", "chưa có bootable image; không kiểm chứng OS boot"
        else:
            if config.launch_only and config.image is not None:
                raise ValueError("launch-only không nhận image")
            if not config.launch_only:
                regular_file(config.image)
                markers = (config.success_marker, config.panic_marker)
                if any(not marker or len(marker.encode()) > 65536 or "\n" in marker or "\r" in marker for marker in markers) or markers[0] == markers[1]:
                    raise ValueError("boot cần hai marker literal theo dòng, không rỗng/giống nhau")
            result.command = command_factory(config, directory)
            stderr = (directory / "stderr.log").open("wb")
            transcript = (directory / "qmp.log").open("wb")
            scanner = SerialScanner(serial, config.success_marker, config.panic_marker)
            process = subprocess.Popen(result.command, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                       stderr=stderr, start_new_session=True)
            result.pid = process.pid
            qmp = QMP(process, transcript)
            deadline = started + timeout
            while True:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    if qmp.eof:
                        raise ProtocolError("QMP EOF nhưng tiến trình không thoát")
                    raise DeadlineExpired("hết deadline monotonic")
                scanner.scan()
                if scanner.panic:
                    result.status, result.detail = "PANIC", "đã nhận panic marker"
                    break
                code = process.poll()
                if code is not None:
                    # Exit có thể xảy ra trước khi host đọc response cuối trong pipe.
                    for _ in range(64):
                        if not qmp.pump(0):
                            break
                    scanner.scan(final=True)
                    if qmp.ready and scanner.success and code == 0:
                        result.status, result.detail = "PASS", "success marker và exit sạch"
                    else:
                        result.status, result.detail = "EXIT", f"thoát trước khi đạt tiêu chí, code={code}"
                    break
                if qmp.ready and (config.launch_only or scanner.success):
                    if config.launch_only and qmp.running is not False:
                        raise ProtocolError("launch-only cần VM paused, running=false")
                    result.status, result.detail = "PASS", "emulator launch paused, không phải OS boot" if config.launch_only else "đã nhận success marker"
                    break
                qmp.pump(min(0.05, remaining))
    except DeadlineExpired as exc:
        result.status, result.detail = "TIMEOUT", str(exc)
    except (OSError, ValueError) as exc:
        result.status = "PROTOCOL_ERROR" if process is not None else "LAUNCH_ERROR"
        result.detail = f"{type(exc).__name__}: {exc}"
    except Exception as exc:
        result.status, result.detail = "PROTOCOL_ERROR", f"host exception: {type(exc).__name__}: {exc}"
    finally:
        if process is not None:
            result.cleanup_forced, result.cleanup_error = cleanup(process, qmp)
            result.returncode = process.poll()
            if scanner:
                scanner.scan(final=True)
                if scanner.panic:
                    result.status, result.detail = "PANIC", "panic marker ưu tiên sau final drain"
            if result.cleanup_error:
                result.status, result.detail = "CLEANUP_ERROR", result.cleanup_error
            elif result.status == "PASS" and result.cleanup_forced:
                result.status, result.detail = "CLEANUP_ERROR", "không đạt shutdown sạch, phải terminate/kill"
            elif result.status == "PASS" and result.returncode != 0:
                result.status, result.detail = "EXIT", f"exit lỗi không được che bởi success: {result.returncode}"
            for pipe in (process.stdin, process.stdout):
                if pipe:
                    try:
                        pipe.close()
                    except OSError:
                        # stdin có thể còn buffered write sau BrokenPipe khi guest thoát.
                        pass
        if qmp:
            qmp.close()
        if scanner:
            scanner.close()
        for file in (stderr, transcript):
            if file:
                file.close()
        result.elapsed = time.monotonic() - started
        (directory / "result.json").write_text(json.dumps(asdict(result), ensure_ascii=False, indent=2) + "\n")
    return result
