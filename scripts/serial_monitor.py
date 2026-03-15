#!/usr/bin/env python3
import serial
import sys
import select
import tty
import termios

def main():
    fd = sys.stdin.fileno()
    old_settings = termios.tcgetattr(fd)
    
    try:
        tty.setraw(fd)
        s = serial.Serial('/dev/ttyACM0', 115200, timeout=0.05)
        s.dtr = True
        
        sys.stdout.write('Press any key to quit...\r\n')
        sys.stdout.flush()
        
        while True:
            # Check for keyboard input
            if select.select([sys.stdin], [], [], 0)[0]:
                break
            
            # Read from serial
            line = s.readline()
            if line:
                sys.stdout.write(line.decode('utf-8', 'ignore'))
                sys.stdout.flush()
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)
        print()

if __name__ == '__main__':
    main()
