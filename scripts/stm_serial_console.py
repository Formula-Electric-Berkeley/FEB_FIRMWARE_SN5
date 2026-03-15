#!/usr/bin/env python3

import argparse
import os
import select
import subprocess
import sys
import termios
import time
import tty
from typing import Optional


DEFAULT_BAUD = 115200
RESOLVE_RETRY_S = 0.5


def print_status(message: str) -> None:
    sys.stderr.write(f"\r\n[stm-serial] {message}\r\n")
    sys.stderr.flush()


def resolve_port(script_dir: str) -> Optional[str]:
    resolver = os.path.join(script_dir, "stm_serial.sh")
    try:
        result = subprocess.run(
            [resolver, "path"],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
        )
    except subprocess.CalledProcessError:
        return None

    port = result.stdout.strip()
    return port or None


def configure_serial(fd: int, baud: int) -> None:
    attrs = termios.tcgetattr(fd)

    attrs[0] = 0
    attrs[1] = 0
    attrs[2] = attrs[2] | termios.CLOCAL | termios.CREAD | termios.CS8
    attrs[2] = attrs[2] & ~(termios.PARENB | termios.CSTOPB | termios.CSIZE | termios.CRTSCTS)
    attrs[2] = attrs[2] | termios.CS8
    attrs[3] = 0

    attrs[4] = baud
    attrs[5] = baud

    attrs[6][termios.VMIN] = 0
    attrs[6][termios.VTIME] = 0

    termios.tcsetattr(fd, termios.TCSANOW, attrs)


def open_serial_port(path: str, baud: int) -> int:
    fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    configure_serial(fd, baud)
    return fd


def main() -> int:
    parser = argparse.ArgumentParser(description="Auto-reconnecting STM serial console")
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD)
    parser.add_argument("--local-echo", action="store_true")
    args = parser.parse_args()

    stdin_fd = sys.stdin.fileno()
    script_dir = os.path.dirname(os.path.abspath(__file__))

    serial_fd: Optional[int] = None
    serial_path: Optional[str] = None
    last_resolve_attempt = 0.0
    last_error: Optional[str] = None
    original_tty = termios.tcgetattr(stdin_fd)

    print_status("Ctrl-C exits. Resolving STM serial port.")

    try:
        tty.setraw(stdin_fd)

        while True:
            now = time.monotonic()

            if serial_fd is None and now - last_resolve_attempt >= RESOLVE_RETRY_S:
                last_resolve_attempt = now
                new_path = resolve_port(script_dir)
                if new_path is not None:
                    try:
                        serial_fd = open_serial_port(new_path, args.baud)
                        serial_path = new_path
                        last_error = None
                        print_status(f"Connected to {serial_path} @ {args.baud}")
                    except OSError as exc:
                        serial_fd = None
                        serial_path = None
                        error_message = f"Open failed for {new_path}: {exc}"
                        if error_message != last_error:
                            print_status(error_message)
                            last_error = error_message

            read_fds = [stdin_fd]
            if serial_fd is not None:
                read_fds.append(serial_fd)

            ready, _, _ = select.select(read_fds, [], [], RESOLVE_RETRY_S)

            if stdin_fd in ready:
                data = os.read(stdin_fd, 1024)
                if not data:
                    break

                if b"\x03" in data:
                    break

                if args.local_echo:
                    os.write(sys.stdout.fileno(), data)

                if serial_fd is not None:
                    try:
                        os.write(serial_fd, data)
                    except OSError:
                        os.close(serial_fd)
                        serial_fd = None
                        serial_path = None
                        last_error = None
                        print_status("STM port disconnected while writing")

            if serial_fd is not None and serial_fd in ready:
                try:
                    data = os.read(serial_fd, 4096)
                    if not data:
                        raise OSError("serial port closed")
                    os.write(sys.stdout.fileno(), data)
                except OSError:
                    os.close(serial_fd)
                    serial_fd = None
                    old_path = serial_path
                    serial_path = None
                    last_error = None
                    print_status(f"STM port disconnected{f' ({old_path})' if old_path else ''}; waiting to reconnect")

    finally:
        if serial_fd is not None:
            os.close(serial_fd)
        termios.tcsetattr(stdin_fd, termios.TCSANOW, original_tty)
        print_status("Console exited")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
