#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build cross GNU đã ghim; chỉ ghi prefix do người dùng chỉ định."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tarfile
import urllib.request


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def extract(archive, destination):
    with tarfile.open(archive) as source:
        # GNU archives chứa symlinks; chỉ cho phép links vẫn ở trong root giải nén.
        for member in source.getmembers():
            target = (destination / member.name).resolve()
            if not target.is_relative_to(destination.resolve()) or member.isdev() or member.isfifo():
                raise ValueError("Archive có đường dẫn hoặc loại file không được phép")
            if member.issym() or member.islnk():
                base = target.parent if member.issym() else destination
                if not (base / member.linkname).resolve().is_relative_to(destination.resolve()):
                    raise ValueError("Archive có link ra ngoài thư mục giải nén")
        source.extractall(destination)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prefix", type=Path, required=True)
    parser.add_argument("--work", type=Path, default=Path("out/m01/bootstrap"))
    parser.add_argument("--jobs", type=int, default=min(4, os.cpu_count() or 1))
    args = parser.parse_args()
    prefix, work = args.prefix.resolve(), args.work.resolve()
    lock = Path(__file__).with_name("toolchain-lock.json")
    manifest = json.loads(lock.read_text())
    receipt = hashlib.sha256(lock.read_bytes()).hexdigest()
    marker = prefix / ".lamix-toolchain.json"
    pending = prefix / ".lamix-toolchain.pending.json"
    if prefix in (Path("/"), Path("/usr"), Path("/usr/local"), Path.home()) or args.jobs < 1:
        parser.error("prefix hoặc số jobs không hợp lệ")
    if prefix.exists() and any(prefix.iterdir()):
        owner = marker if marker.exists() else pending
        if not owner.exists() or json.loads(owner.read_text()).get("manifest_sha256") != receipt:
            parser.error("prefix không rỗng/không có receipt phù hợp; không cài đè")
        if marker.exists():
            print("PASS: prefix có receipt phù hợp; doctor phải xác minh trước khi dùng cache")
            return 0
    prefix.mkdir(parents=True, exist_ok=True)
    pending.write_text(json.dumps({"manifest_sha256": receipt}) + "\n")
    work.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env["PATH"] = str(prefix / "bin") + os.pathsep + env["PATH"]
    env["LC_ALL"] = "C"
    with (work / "bootstrap.log").open("w") as log:
        for source in manifest["sources"]:
            archive = work / Path(source["url"]).name
            if not archive.exists():
                temporary = archive.with_suffix(".download")
                with urllib.request.urlopen(source["url"], timeout=60) as response, temporary.open("wb") as dest:
                    while block := response.read(1024 * 1024):
                        dest.write(block)
                if sha256(temporary) != source["sha256"]:
                    raise ValueError("SHA-256 download không khớp manifest")
                temporary.replace(archive)
            if sha256(archive) != source["sha256"]:
                raise ValueError("SHA-256 archive không khớp manifest")
            directory = work / f"{source['name']}-{source['version']}"
            if not directory.exists():
                extract(archive, work)
            build = work / f"build-{source['name']}"
            build.mkdir(exist_ok=True)
            command = [str(directory / "configure"), f"--target={manifest['target']}", f"--prefix={prefix}", *manifest[source['name'] + "_options"]]
            commands = [command]
            if source["name"] == "binutils":
                commands += [["make", f"-j{args.jobs}"], ["make", "install"]]
            else:
                commands += [["make", f"-j{args.jobs}", "all-gcc", "all-target-libgcc"], ["make", "install-gcc", "install-target-libgcc"]]
            for command in commands:
                print("RUN:", source["name"], command[0], flush=True)
                log.write("$ " + " ".join(command) + "\n")
                log.flush()
                subprocess.run(command, cwd=build, env=env, stdout=log, stderr=subprocess.STDOUT, check=True)
    marker.write_text(json.dumps({"manifest_sha256": receipt, "target": manifest["target"]}, indent=2) + "\n")
    pending.unlink()
    print("PASS: cross-toolchain đã cài vào prefix riêng")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
