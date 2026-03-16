#!/usr/bin/env python3

"""
pre-commit-hook checked-in-together
    files passed as arguments either must all be checked in or none of them
"""

import sys
import hashlib
from subprocess import Popen, PIPE


def added_files():
    p = Popen(
        ['git', 'diff', '--staged', '--name-only'],
        stdout=PIPE,
        stderr=PIPE
    )
    out, err = p.communicate()
    if p.returncode != 0:
        raise RuntimeError(err.decode())

    return set(out.decode().splitlines())


def last_commited_files():
    p = Popen(
        ['git', 'diff', '--name-only', 'HEAD', 'HEAD~1'],
        stdout=PIPE,
        stderr=PIPE
    )
    out, err = p.communicate()
    if p.returncode != 0:
        # There is not always a HEAD, not considered an error
        return set()

    return set(out.decode().splitlines())



def main():
    added = added_files()

    if not added:
        # if we are in the post-added stage we verify the last commit
        # (useful for CI)
        added = last_commited_files()

    checked = [file in added for file in sys.argv[1:]]
    if any(checked) and not all(checked):
        print(f"Must check in all or none of files {' '.join(sys.argv[1:])}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
