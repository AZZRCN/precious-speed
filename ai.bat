@echo off
cd /d d:\precious_speed
python verify_fix.py > verify_fix.log 2>&1
echo [DONE] >> verify_fix.log
