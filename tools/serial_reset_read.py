#!/usr/bin/env python3
import argparse
import time

import serial


def main():
    parser = argparse.ArgumentParser(description="Reset ESP32-C3 and print serial output.")
    parser.add_argument("--port", default="/dev/ttyACM0")
    parser.add_argument("--baud", default=115200, type=int)
    parser.add_argument("--seconds", default=8, type=float)
    args = parser.parse_args()

    with serial.Serial(args.port, args.baud, timeout=0.2) as ser:
        ser.dtr = False
        ser.rts = True
        time.sleep(0.1)
        ser.rts = False

        end = time.time() + args.seconds
        output = bytearray()
        while time.time() < end:
            output.extend(ser.read(4096))

    print(output.decode("utf-8", "replace"), end="")


if __name__ == "__main__":
    main()
