#!/usr/bin/env python3
import sys
sys.path.insert(0, "d:/precious_speed")
from ssh_manager import ssh

# 检查 /tmp/test_input.txt 的内容
rc, out, err = ssh.run("""
echo "=== File info ==="
wc -lc /tmp/test_input.txt
echo "=== First 20 chars ==="
head -c 20 /tmp/test_input.txt | xxd
echo "=== Line count ==="
wc -l /tmp/test_input.txt
echo "=== Run with small test ==="
printf '1\\n123456789\\n12345\\n' | /home/azzr/moptm_default
echo "=== Run with 100k/50k ==="
printf '1\\n' > /tmp/test2.txt
python3 -c "import random; random.seed(42); print(''.join([str(random.randint(0,9)) for _ in range(100000)]))" >> /tmp/test2.txt
python3 -c "import random; random.seed(42); print(str(random.randint(1,9))+''.join([str(random.randint(0,9)) for _ in range(49999)]))" >> /tmp/test2.txt
wc -c /tmp/test2.txt
/home/azzr/moptm_default < /tmp/test2.txt | head -c 50
echo ""
echo "=== Run with existing test_input.txt ==="
/home/azzr/moptm_default < /tmp/test_input.txt | head -c 50
echo ""
echo "=== Check if binary is correct ==="
file /home/azzr/moptm_default
ls -la /home/azzr/moptm_default
""", timeout=120)
print(out)
if err:
    print(f"STDERR: {err[:500]}")
