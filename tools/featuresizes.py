#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Create a cool markdown table of the feature sizes
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

def list_features(make):
    # list-features prints "name dep dep.." per line
    features = []
    for line in run(make + ['list-features']).splitlines():
        parts = line.split()
        if parts:
            features.append((parts[0], parts[1:]))
    return features

def closure(name, deps_of):
    # A feature's deps and their deps and so on
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
    ap = argparse.ArgumentParser(description='Measure how much each feature adds to the rootfs')
    ap.add_argument('image', help='image name the makefile uses, x86_64 for x86_64.tar and x86_64.erofs')
    ap.add_argument('--no-erofs', action='store_true', help="don't build and measure the erofs image")
    ap.add_argument('make', nargs='+', help='make command, put -- before it')
    args = ap.parse_args()

    tar, erofs = f'{args.image}.tar', f'{args.image}.erofs'
    cache = {}

    def build(features):
        key = tuple(sorted(features))
        if key not in cache:
            make = args.make + [f'FEATURES={" ".join(key)}']
            run(make + [tar])
            sizes = [rootfs_size(tar)]
            if not args.no_erofs:
                run(make + [erofs])
                sizes.append(os.path.getsize(erofs))
            cache[key] = sizes
        return cache[key]

    features = list_features(args.make)
    deps_of = dict(features)

    cols = ['rootfs'] + ([] if args.no_erofs else ['erofs'])
    rows = [('(none)', build([]))]
    for name, _ in features:
        deps = closure(name, deps_of)
        before, after = build(deps), build(deps + [name])
        label = name + (f' (on top of {" ".join(deps)})' if deps else '')
        rows.append((label, [a - b for a, b in zip(after, before)]))
    rows.append(('(all)', build([n for n, _ in features])))

    width = max(len(r[0]) for r in rows)
    print(f'| {"feature":<{width}} | ' + ' | '.join(f'{c:>10}' for c in cols) + ' |')
    print(f'|{"-" * (width + 2)}|' + '|'.join('-' * 12 for _ in cols) + '|')
    for label, sizes in rows:
        print(f'| {label:<{width}} | ' + ' | '.join(f'{s:>10}' for s in sizes) + ' |')

if __name__ == '__main__':
    main()
