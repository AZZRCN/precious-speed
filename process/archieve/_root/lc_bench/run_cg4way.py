import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), 'vm'))
import vmctl

rc, out, err = vmctl.run('cd /home/azzr/divbench && bash _cg4way.sh 2>&1', timeout=7200)
print(out)
if err:
    print('--- STDERR ---')
    print(err)
print('rc=', rc)
