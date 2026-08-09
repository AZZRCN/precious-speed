"""Generic remote shell helper.

Usage:  python _sh.py "<command>" [timeout]
Avoids the inline-quote-eating trap by passing the command as a single argv.
"""
import sys
import vmctl

cmd = sys.argv[1]
timeout = int(sys.argv[2]) if len(sys.argv) > 2 else 900
rc, out, err = vmctl.run(cmd, timeout=timeout, verbose=False)
sys.stdout.write(out)
if err:
    sys.stderr.write("STDERR:\n" + err)
print(f"[rc={rc}]")
