#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Create a cool markdown table of the set sizes
#
import argparse
import os
import subprocess
import sys
import tarfile

def run(cmd):
    r = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if r.returncode:
        sys.stderr.write(r.stdout)
        sys.exit(f'failed: {" ".join(cmd)}')
    return r.stdout

def list_sets(make):
    # list-sets prints "name dep dep.." per line, skip anything make itself says
    sets = []
    for line in run(make + ['list-sets']).splitlines():
        parts = line.split()
        if parts and not parts[0].startswith('make'):
            sets.append((parts[0], parts[1:]))
    return sets

def closure(name, deps_of):
    # A set's deps and their deps and so on
    todo, seen = list(deps_of[name]), []
    while todo:
        d = todo.pop()
        if d not in seen:
            seen.append(d)
            todo += deps_of[d]
    return seen

def rootfs_size(path):
    # Size of the files that end up in the rootfs, not the tar itself
    with tarfile.open(path) as t:
        return sum(m.size for m in t.getmembers() if m.isreg())

def main():
    ap = argparse.ArgumentParser(description='Measure how much each set adds to the rootfs')
    ap.add_argument('image', help='image name the makefile uses, x86_64 for x86_64.tar and x86_64.erofs')
    ap.add_argument('--no-erofs', action='store_true', help="don't build and measure the erofs image")
    ap.add_argument('make', nargs='+', help='make command, put -- before it')
    args = ap.parse_args()

    tar, erofs = f'{args.image}.tar', f'{args.image}.erofs'
    # When run from another make the sub make announces directories, don't
    args.make = args.make + ['--no-print-directory']
    cache = {}

    def build(sets):
        key = tuple(sorted(sets))
        if key not in cache:
            make = args.make + [f'SETS={" ".join(key)}']
            run(make + [tar])
            sizes = [rootfs_size(tar)]
            if not args.no_erofs:
                run(make + [erofs])
                sizes.append(os.path.getsize(erofs))
            cache[key] = sizes
        return cache[key]

    sets = list_sets(args.make)
    deps_of = dict(sets)

    cols = ['rootfs'] + ([] if args.no_erofs else ['erofs'])
    rows = [('(none)', build([]))]
    for name, _ in sets:
        deps = closure(name, deps_of)
        before, after = build(deps), build(deps + [name])
        label = name + (f' (on top of {" ".join(deps)})' if deps else '')
        rows.append((label, [a - b for a, b in zip(after, before)]))
    rows.append(('(all)', build([n for n, _ in sets])))

    width = max(len(r[0]) for r in rows)
    print(f'| {"set":<{width}} | ' + ' | '.join(f'{c:>10}' for c in cols) + ' |')
    print(f'|{"-" * (width + 2)}|' + '|'.join('-' * 12 for _ in cols) + '|')
    for label, sizes in rows:
        print(f'| {label:<{width}} | ' + ' | '.join(f'{s:>10}' for s in sizes) + ' |')

if __name__ == '__main__':
    main()
