import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import run, put
print(run("cd /home/azzr/hexbench/build && ls -l v13 v16 && size v13 v16"))
print(run("cd /home/azzr/hexbench/build && nm -C --size-sort -S v16 2>/dev/null | tail -15"))
print(run("cd /home/azzr/hexbench/build && nm -C v16 | grep -i ' [bBdD] ' | head -30"))
