#!/bin/bash
echo "=== file size ==="
ls -la /tmp/fusion/test_add_1M.txt
echo "=== first line ==="
head -1 /tmp/fusion/test_add_1M.txt
echo "=== line count ==="
wc -l /tmp/fusion/test_add_1M.txt
echo "=== first 50 chars of line 2 ==="
sed -n '2p' /tmp/fusion/test_add_1M.txt | head -c 50
echo ""
echo "=== line 2 length ==="
sed -n '2p' /tmp/fusion/test_add_1M.txt | wc -c
echo "=== line 3 length ==="
sed -n '3p' /tmp/fusion/test_add_1M.txt | wc -c
echo "=== check if line 2 has space ==="
sed -n '2p' /tmp/fusion/test_add_1M.txt | grep -c ' '
