#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   "Why don't you just use kconfig"
#
#   This is a tool for CI to parse config.h and generate builds..
#
#   /* Config description */
#   #ifndef CONFIG_HOGEHOGE
#   #define CONFIG_HOGEHOGE	y
#   #endif
#

import argparse
import json
import os
import re
import sys

IFNDEF = re.compile(r'^\s*#\s*ifndef\s+(CONFIG_\w+)\s*$')
DEFINE = re.compile(r'^\s*#\s*define\s+(CONFIG_\w+)\s+(\S+)\s*$')
COMMENT = re.compile(r'^\s*/\*\s*(.*?)\s*\*/\s*$')

def parse(path):
    options = []
    comment = None

    with open(path) as f:
        lines = [l.rstrip('\n') for l in f]

    i = 0
    while i < len(lines):
        line = lines[i]

        m = COMMENT.match(line)
        if m:
            comment = m.group(1)
            i += 1
            continue

        m = IFNDEF.match(line)
        if m and i + 1 < len(lines):
            name = m.group(1)
            d = DEFINE.match(lines[i + 1])
            if d and d.group(1) == name:
                options.append({
                    'name': name,
                    'default': d.group(2),
                    'help': comment,
                })
                i += 2
                continue

        if line.strip():
            comment = None
        i += 1

    return options

def main():
    here = os.path.dirname(os.path.abspath(__file__))
    default_path = os.path.join(here, '..', 'config.h')

    ap = argparse.ArgumentParser(description='List the CONFIG_ options in config.h')
    ap.add_argument('config', nargs='?', default=default_path,
                    help='path to config.h (default: the one next to tools/)')
    ap.add_argument('--json', action='store_true', help='print json instead')
    ap.add_argument('--names', action='store_true', help='print just the names')
    args = ap.parse_args()

    try:
        options = parse(args.config)
    except OSError as e:
        sys.exit(f'{e.filename}: {e.strerror}')

    if args.json:
        json.dump(options, sys.stdout, indent=1)
        print()
    elif args.names:
        for o in options:
            print(o['name'])
    else:
        width = max(len(o['name']) for o in options) if options else 0
        for o in options:
            print(f"{o['name']:<{width}} {o['default']}  {o['help'] or ''}".rstrip())

if __name__ == '__main__':
    main()
