#!/usr/bin/env python3

import sys
import os
from datetime import datetime
import serial


def main() -> None:
    if len(sys.argv) < 2:
        sys.stderr.write(f'Usage: {sys.argv[0]} serial-device [logdir]\n')
        sys.exit(1)

    device = sys.argv[1]
    logdir = sys.argv[2] if len(sys.argv) > 2 else '.'

    ser = serial.Serial(device, 115200)

    while True:
        line = ser.readline().decode('ascii').strip()
        time = datetime.today().strftime('%H:%M:%S.%f')[:-3]
        filename = datetime.today().strftime('%Y-%m-%d') + '.log'
        with open(os.path.join(logdir, filename), 'a') as f:
            f.write(f'{time} {line}\n')
            print(f'{time} {line}')


if __name__ == '__main__':
    main()
