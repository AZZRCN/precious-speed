@echo off
chcp 65001 >nul
REM ============================================================
REM  enable_ssh_285H.bat  —  在 285H Windows 上【以管理员运行】
REM  仅开通 OpenSSH 服务端 + 防火墙22 + 禁止睡眠。
REM  注意：285H Windows 当前在 192.168.0.x（VMware 虚拟网卡 145/152 仅 VM 内部），
REM        本机 WorkBuddy(192.168.1.x) 无法直接路由到它。
REM        真要本机直连，需另走 VM 隧道或让 Windows 也拿到 192.168.1.x 地址。
REM  用法：右键"以管理员身份运行"，看末尾是否报 sshd 已启动即可。
REM ============================================================

echo [1/3] 安装并启动 OpenSSH 服务端
powershell -NoProfile -Command "Add-WindowsCapability -Online -Name OpenSSH.Server~~~~0.0.1.0 | Out-Null; Set-Service -Name sshd -StartupType Automatic; Start-Service -Name sshd"

echo [2/3] 防火墙放行 22
powershell -NoProfile -Command "if (-not (Get-NetFirewallRule -Name 'OpenSSH-Server-In-TCP' -ErrorAction SilentlyContinue)) { New-NetFirewallRule -Name 'OpenSSH-Server-In-TCP' -DisplayName 'OpenSSH Server' -Enabled True -Direction Inbound -Protocol TCP -Action Allow -LocalPort 22 }"

echo [3/3] 禁止交流电下睡眠
powercfg /change standby-timeout-ac 0

echo.
echo 完成。本机可用 IP（仅供记录，本机可能仍路由不到）:
ipconfig | findstr /i "IPv4"
pause
