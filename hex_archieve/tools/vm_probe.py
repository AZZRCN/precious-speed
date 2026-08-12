import sys
sys.path.insert(0, 'D:/hex_precious_speed/tools')
import vmctl

for c in [
    'which valgrind callgrind cg_annotate 2>/dev/null; valgrind --version 2>/dev/null || echo NO_VALGRIND',
    'ls /usr/lib/x86_64-linux-gnu/libgmp* 2>/dev/null; echo GMP_DONE',
    'mkdir -p ~/hexbench/add ~/hexbench/mul ~/hexbench/div ~/hexbench/oracle && echo WORKSPACE_OK',
    'echo HOME=$HOME; nproc; free -m | head -2',
]:
    vmctl.run(c)
