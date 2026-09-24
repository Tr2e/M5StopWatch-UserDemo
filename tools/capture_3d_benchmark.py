#!/usr/bin/env python3
"""Capture one autorun 3D Benchmark session, ending at all_complete."""

import argparse
import sys
import time

import serial


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port")
    parser.add_argument("--timeout", type=float, default=150.0)
    args = parser.parse_args()

    with serial.Serial(args.port, 115200, timeout=0.25, exclusive=True) as device:
        device.dtr = False
        device.rts = True
        time.sleep(0.1)
        device.rts = False
        deadline = time.monotonic() + args.timeout
        pending = bytearray()
        result_count = 0
        while time.monotonic() < deadline:
            pending.extend(device.read(4096))
            while b"\n" in pending:
                line, _, rest = pending.partition(b"\n")
                pending = bytearray(rest)
                decoded = line.decode("utf-8", errors="replace").rstrip("\r")
                print(decoded, flush=True)
                if "[3DBenchResult]" in decoded:
                    result_count += 1
                if "[3DBenchAudit]" in decoded and "all_complete" in decoded:
                    if result_count != 24:
                        print(f"Expected 24 stage results, got {result_count}", file=sys.stderr)
                        return 2
                    return 0
        print(f"Timed out after {args.timeout}s; got {result_count} stage results", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
