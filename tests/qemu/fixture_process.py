# SPDX-License-Identifier: GPL-3.0-only
"""Tiến trình host giả lập QMP/serial để test harness; không phải guest OS."""

import argparse
import json
import os
from pathlib import Path
import signal
import subprocess
import sys


def send(value):
    print(json.dumps(value), flush=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--directory", type=Path, required=True)
    parser.add_argument("--scenario", required=True)
    parser.add_argument("--launch", action="store_true")
    args = parser.parse_args()
    child = None

    def terminate(signum, frame):
        raise SystemExit(0)

    signal.signal(signal.SIGTERM, terminate)
    try:
        if args.scenario == "child":
            child = subprocess.Popen([sys.executable, "-c", "import sys; sys.stdin.read()"], stdin=subprocess.PIPE)
            (args.directory / "child.pid").write_text(str(child.pid))
        if args.scenario == "ignore-quit":
            signal.signal(signal.SIGTERM, signal.SIG_IGN)
        if args.scenario == "bad-greeting":
            print("not json", flush=True)
        elif args.scenario != "no-greeting":
            send({"QMP": {"version": {}, "capabilities": []}})
        for line in sys.stdin:
            command = json.loads(line)
            identity = command["id"]
            if command["execute"] == "qmp_capabilities":
                if args.scenario == "no-response":
                    continue
                if args.scenario == "bad-response":
                    send({"error": {"class": "GenericError"}, "id": identity})
                else:
                    send({"event": "FIXTURE_EVENT"})
                    send({"return": {}, "id": 999})
                    send({"return": {}, "id": identity})
            elif command["execute"] == "query-status":
                send({"return": {"running": not args.launch or args.scenario == "running", "status": "paused" if args.launch else "running"}, "id": identity})
                if args.scenario in ("hang", "exit0", "bad-greeting", "no-greeting", "no-response"):
                    if args.scenario == "exit0":
                        return 0
                    continue
                with (args.directory / "serial.log").open("ab", buffering=0) as serial:
                    if args.scenario == "panic-before":
                        serial.write(b"PANIC\nSUCCESS\n")
                    elif args.scenario == "split":
                        serial.write(b"SUCC")
                        serial.write(b"ESS\n")
                    elif args.scenario == "crlf":
                        serial.write(b"SUCCESS\r\n")
                    else:
                        if args.scenario == "flood":
                            serial.write(b"x" * 300000 + b"\n")
                            sys.stderr.buffer.write(b"e" * 300000)
                            sys.stderr.buffer.flush()
                            for _ in range(1000):
                                send({"event": "FIXTURE_EVENT"})
                        serial.write(b"SUCCESS\n")
                if args.scenario == "exit1":
                    return 1
                if args.scenario == "natural-exit":
                    return 0
                if args.scenario == "broken-stdin":
                    os.close(sys.stdin.fileno())
                    while True:
                        signal.pause()
            elif command["execute"] == "quit":
                if args.scenario == "ignore-quit":
                    continue
                if args.scenario == "flood-on-quit":
                    send({"event": "FIXTURE_EVENT", "data": {"payload": "x" * 300000}})
                if args.scenario == "panic-after":
                    with (args.directory / "serial.log").open("ab") as serial:
                        serial.write(b"PANIC\n")
                if args.scenario == "write-vars":
                    (args.directory / "OVMF_VARS.fd").write_bytes(b"private fixture write")
                send({"return": {}, "id": identity})
                return 0
        return 0
    finally:
        if child:
            child.terminate()
            child.wait(timeout=2)
            child.stdin.close()


if __name__ == "__main__":
    raise SystemExit(main())
