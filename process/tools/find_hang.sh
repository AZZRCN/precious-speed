cd /home/azzr/precious_speed/hex_best
tail -n +2 molder_test.txt > /tmp/pairs.txt
i=0
while IFS= read -r line; do
  i=$((i+1))
  printf '1\n%s\n' "$line" > /tmp/c.txt
  timeout 10 ./div_molder_mtt_zn.bin < /tmp/c.txt > /tmp/cout.txt 2>/dev/null
  rc=$?
  if [ $rc -ne 0 ]; then echo "CASE $i RC=$rc (HANG/FAIL) pair=$line"; fi
done < /tmp/pairs.txt
echo "DONE scanned $i cases"
