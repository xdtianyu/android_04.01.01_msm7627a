#!/usr/bin/env python

import re
import sys

def extract_config(f):
    conf_patt = re.compile('# Configurations')
    split_patt = re.compile('#={69}')
    var_patt = re.compile('libbcc_([A-Z_]+)\\s*:=\\s*([01])')

    STATE_PRE_CONFIG = 0
    STATE_FOUND_CONFIG = 1
    STATE_IN_CONFIG = 2

    state = STATE_PRE_CONFIG

    for line in f:
        if state == STATE_PRE_CONFIG:
            if conf_patt.match(line.strip()):
                state = STATE_FOUND_CONFIG

        elif state == STATE_FOUND_CONFIG:
            if split_patt.match(line.strip()):
                # Start reading the configuration
                print '/* BEGIN USER CONFIG */'
                state = STATE_IN_CONFIG

        elif state == STATE_IN_CONFIG:
            match = var_patt.match(line.strip())
            if match:
                print '#define', match.group(1), match.group(2)

            elif split_patt.match(line.strip()):
                # Stop reading the configuration
                print '/* END USER CONFIG */'
                break

def main():
    if len(sys.argv) != 1:
        print >> sys.stderr, 'USAGE:', sys.argv[0]
        sys.exit(1)

    extract_config(sys.stdin)


if __name__ == '__main__':
    main()
