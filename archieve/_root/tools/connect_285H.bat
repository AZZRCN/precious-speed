@echo off
REM ============================================================
REM  connect_285H.bat  —  在 285H Windows 上【以管理员运行】
REM  目的：让 WorkBuddy 机(192.168.1.x) 能直连本机(285H Windows)
REM  做法：优先复用已有的 192.168.1.x 地址；没有就给物理网卡加 192.168.1.225/23
REM        顺手开 OpenSSH 服务 + 防火墙22 + 禁止睡眠
REM  用法：右键"以管理员身份运行"，跑完把最后打出的 192.168.1.x 那行发给我
REM ============================================================

REM 若你的物理网卡不叫"以太网"，改下面这一行即可（ipconfig 里看"适配器"名）
set NIC=以太网

echo ============================================================
echo [1/5] 检查是否已存在 192.168.1.x 地址
echo ============================================================
ipconfig | findstr /i "192.168.1."
echo （上面若已经出现 192.168.1.x 的 IPv4，直接把它发给我即可，可跳过下面"加地址"步骤）

echo ============================================================
echo [2/5] 给网卡 "%NIC%" 添加 192.168.1.225 / 掩码 255.255.254.0
echo ============================================================
netsh interface ip add address "%NIC%" 192.168.1.225 255.255.254.0
echo （若提示"对象已存在/已添加"均属正常，可忽略）

echo ============================================================
echo [3/5] 安装并启动 OpenSSH 服务端 + 防火墙放行 22
echo ============================================================
powershell -NoProfile -Command "Add-WindowsCapability -Online -Name OpenSSH.Server~~~~0.0.1.0; Set-Service sshd -StartupType Automatic; Start-Service sshd"
powershell -NoProfile -Command "if(-not(Get-NetFirewallRule -Name 'OpenSSH-Server-In-TCP' -ErrorAction SilentlyContinue)){New-NetFirewallRule -Name 'OpenSSH-Server-In-TCP' -DisplayName 'OpenSSH Server' -Enabled True -Direction Inbound -Protocol TCP -Action Allow -LocalPort 22}"

echo ============================================================
echo [4/5] 禁止交流电下睡眠（避免长跑被掐断）
echo ============================================================
powercfg /change standby-timeout-ac 0

echo ============================================================
echo [5/5] 现在本机可用的 IPv4（请把 192.168.1.x 那一行发给我）
echo ============================================================
ipconfig | findstr /i "IPv4"
pause
