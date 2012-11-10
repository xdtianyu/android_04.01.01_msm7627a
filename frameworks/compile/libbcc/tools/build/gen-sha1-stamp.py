#!/usr/bin/env python

import sys

try:
    import hashlib
    sha1 = hashlib.sha1
except ImportError, e:
    import sha
    sha1 = sha.sha

def compute_sha1(h, path):
    f = open(path, 'rb')
    while True:
        buf = f.read(1024)
        h.update(buf)
        if len(buf) < 1024:
            break
    f.close()

def compute_sha1_list(path_list):
    h = sha1()
    for path in path_list:
        compute_sha1(h, path)
    return h.digest()

def main():
    if len(sys.argv) < 2:
        print 'USAGE:', sys.argv[0], '[OUTPUT] [INPUTs]'
        sys.exit(1)

    f = open(sys.argv[1], 'wb')
    f.write(compute_sha1_list(sys.argv[2:]))
    f.close()

if __name__ == '__main__':
    main()
