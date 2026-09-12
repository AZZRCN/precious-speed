# AZZRCN
# https://github.com/AZZRCN
"""vsh.py - VM shell one-liner runner.

Usage:
    python vsh.py "cmd1" ["cmd2" ...]
    python vsh.py -f script.sh          # upload & run a local shell script
Notes:
    Must be run with an interpreter that has paramiko (Python 3.11 on this box).
"""
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl  # noqa: E402


def main(argv):
    if not argv:
        print(__doc__)
        return 1
    if argv[0] == "-f":
        local = argv[1]
        remote = "/tmp/_vsh_script.sh"
        vmctl.put(local, remote, verbose=False)
        vmctl.run(f"bash {remote}")
        return 0
    for cmd in argv:
        vmctl.run(cmd)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
