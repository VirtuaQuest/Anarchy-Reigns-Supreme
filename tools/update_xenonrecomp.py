"""Refreshes thirdparty/XenonRecomp, the copy of the recompiler this project builds with, from a XenonRecomp checkout.

Usage: python tools/update_xenonrecomp.py <XenonRecomp checkout>

It takes the files of the checkout's current commit and of its submodules' commits, as Git stores them (uncommitted
changes are left out), so one clone of this project has everything a build needs. Commit the result afterwards.
"""
import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TARGET = os.path.join(ROOT, 'thirdparty', 'XenonRecomp')
LEFT_OUT = {'.gitmodules'}


def git(repo, *args):
    return subprocess.run(['git', '-C', repo, *args], capture_output=True, check=True).stdout


def copy_tree(repo, prefix, out):
    """Every file of repo's HEAD into out/prefix; submodules are followed at the commit the parent records."""
    count = 0
    blobs = []
    for entry in git(repo, 'ls-tree', '-r', '-z', '--full-tree', 'HEAD').split(b'\0'):
        if not entry:
            continue
        meta, path = entry.split(b'\t', 1)
        mode, kind, sha = meta.split()
        path = path.decode('utf-8')
        if kind == b'commit':
            sub = os.path.join(repo, path)
            if git(sub, 'rev-parse', 'HEAD').strip() != sha:
                sys.exit(f'{prefix}{path} is checked out at a different commit from the one recorded; run git submodule update first.')
            count += copy_tree(sub, prefix + path + '/', out)
        elif kind == b'blob' and mode != b'120000' and (prefix or path not in LEFT_OUT):
            blobs.append((sha, prefix + path))
    if blobs:
        batch = subprocess.run(['git', '-C', repo, 'cat-file', '--batch'], input=b''.join(s + b'\n' for s, _ in blobs),
                               capture_output=True, check=True).stdout
        at = 0
        for _, path in blobs:
            header_end = batch.index(b'\n', at)
            size = int(batch[at:header_end].split()[2])
            data = batch[header_end + 1:header_end + 1 + size]
            at = header_end + 1 + size + 1
            if prefix and os.path.basename(path) in LEFT_OUT and '/' not in path[len(prefix):]:
                continue
            dest = os.path.join(out, *path.split('/'))
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            with open(dest, 'wb') as f:
                f.write(data)
            count += 1
    return count


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    source = os.path.abspath(sys.argv[1])
    if not os.path.exists(os.path.join(source, 'XenonUtils', 'ppc_context.h')):
        sys.exit(source + " isn't a XenonRecomp checkout.")
    if os.path.exists(TARGET):
        shutil.rmtree(TARGET)
    count = copy_tree(source, '', TARGET)
    commit = git(source, 'rev-parse', '--short', 'HEAD').decode().strip()
    print(f'Copied {count} files from XenonRecomp {commit} into thirdparty/XenonRecomp.')


main()
