#!/bin/bash
cd /home/azzr/hexbench
BIN=${1:-/tmp/v9L11}
CASE=${2:-data/mul/max_max_01.in}
N=${3:-25}
cat > /tmp/_loop.sh <<EOF
#!/bin/bash
for i in \$(seq 1 $N); do $BIN < $CASE > /dev/null; done
EOF
chmod +x /tmp/_loop.sh
taskset -c 3 perf record -F 12000 -o /tmp/pr2.data -- /tmp/_loop.sh 2>/dev/null
echo "=== FLAT (self) ==="
perf report -i /tmp/pr2.data --stdio --no-children --sort symbol --percent-limit 0.4 2>/dev/null \
  | grep -E "^\s+[0-9]" | head -25
echo "=== total samples ==="
perf report -i /tmp/pr2.data --stdio --header-only 2>/dev/null | grep -i sample
