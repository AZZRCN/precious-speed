#!/usr/bin/env python3
"""
持久化+非阻塞 SSH 进程管理工具
- 保持 SSH 连接 (带自动重连)
- 非阻塞执行命令 (后台运行, 定期轮询)
- 支持长时间运行的命令 (编译/测试)
- 用法: from ssh_manager import ssh
"""
import paramiko, time, threading, sys, os, base64, hashlib

HOST = "192.168.1.55"
USER = "azzr"
PWD = "1234"

class SSHManager:
    def __init__(self, host=HOST, user=USER, pwd=PWD):
        self.host = host
        self.user = user
        self.pwd = pwd
        self.client = None
        self._lock = threading.Lock()
        self._connect()

    def _connect(self):
        """建立/重建 SSH 连接"""
        if self.client:
            try:
                self.client.close()
            except:
                pass
        self.client = paramiko.SSHClient()
        self.client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        # 设置 keepalive
        self.client.connect(self.host, username=self.user, password=self.pwd, timeout=15)
        self.client.get_transport().set_keepalive(30)  # 30秒 keepalive
        print(f"[SSH] Connected to {self.host}")

    def _ensure_conn(self):
        """确保连接可用, 断则重连"""
        with self._lock:
            if self.client is None:
                self._connect()
                return
            try:
                # 测试连接是否活着
                chan = self.client.get_transport().open_session()
                chan.close()
            except Exception as e:
                print(f"[SSH] Connection lost ({e}), reconnecting...")
                self._connect()

    def upload(self, local_path, remote_path):
        """上传文件 (SFTP)"""
        self._ensure_conn()
        with self._lock:
            sftp = self.client.open_sftp()
            sftp.put(local_path, remote_path)
            sftp.close()
        print(f"[SSH] Uploaded {os.path.basename(local_path)} -> {remote_path}")

    def run(self, cmd, timeout=300):
        """同步执行命令, 返回 (exit_code, stdout, stderr)"""
        self._ensure_conn()
        with self._lock:
            try:
                stdin, stdout, stderr = self.client.exec_command(cmd, timeout=timeout)
                rc = stdout.channel.recv_exit_status()
                out = stdout.read().decode(errors='replace')
                err = stderr.read().decode(errors='replace')
                return rc, out, err
            except Exception as e:
                print(f"[SSH] run failed ({e}), retrying once...")
                self._connect()
                stdin, stdout, stderr = self.client.exec_command(cmd, timeout=timeout)
                rc = stdout.channel.recv_exit_status()
                out = stdout.read().decode(errors='replace')
                err = stderr.read().decode(errors='replace')
                return rc, out, err

    def run_bg(self, cmd, out_file="/tmp/ssh_bg_out.txt"):
        """
        非阻塞执行命令: 后台运行, 输出写到文件
        返回 job_id (PID), 用 check_bg(job_id) 轮询
        """
        self._ensure_conn()
        # 用 nohup + & 后台运行, 记录 PID
        full_cmd = f"nohup bash -c '{cmd}' > {out_file} 2>&1 & echo $!"
        with self._lock:
            try:
                stdin, stdout, stderr = self.client.exec_command(full_cmd, timeout=10)
                pid = stdout.read().decode().strip()
                return pid, out_file
            except Exception as e:
                print(f"[SSH] run_bg failed ({e}), retrying...")
                self._connect()
                stdin, stdout, stderr = self.client.exec_command(full_cmd, timeout=10)
                pid = stdout.read().decode().strip()
                return pid, out_file

    def check_bg(self, pid):
        """检查后台进程是否还在运行. 返回 (is_running, output_so_far)"""
        self._ensure_conn()
        # 检查 PID 是否存在
        rc, out, err = self.run(f"kill -0 {pid} 2>/dev/null && echo RUNNING || echo DONE", timeout=10)
        is_running = "RUNNING" in out
        return is_running

    def get_bg_output(self, out_file, tail_lines=50):
        """获取后台进程的输出"""
        self._ensure_conn()
        rc, out, err = self.run(f"tail -n {tail_lines} {out_file} 2>/dev/null", timeout=10)
        return out

    def wait_bg(self, pid, out_file, poll_interval=2, timeout=600):
        """等待后台进程完成, 返回完整输出"""
        start = time.time()
        while time.time() - start < timeout:
            if not self.check_bg(pid):
                break
            time.sleep(poll_interval)
        return self.get_bg_output(out_file, tail_lines=500)

    def upload_and_compile(self, local_cpp, remote_cpp, extra_flags=""):
        """上传源码并编译, 返回 (exe_path, rc, stderr)"""
        self.upload(local_cpp, remote_cpp)
        exe = remote_cpp.rsplit('.', 1)[0]
        cmd = f"g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV {extra_flags} {remote_cpp} -o {exe} -pthread 2>&1"
        rc, out, err = self.run(cmd, timeout=300)
        return exe, rc, out + err

    def run_test(self, exe, test_input, timeout=120):
        """运行测试, 返回 (rc, output)"""
        # 用 base64 传输输入, 避免 shell 转义问题
        b64 = base64.b64encode(test_input.encode()).decode()
        cmd = f"echo {b64} | base64 -d | {exe} 2>&1"
        rc, out, err = self.run(cmd, timeout=timeout)
        return rc, out + err

    def run_test_bg(self, exe, test_input, timeout=120):
        """非阻塞运行测试, 返回 (pid, out_file)"""
        b64 = base64.b64encode(test_input.encode()).decode()
        cmd = f"echo {b64} | base64 -d | {exe}"
        return self.run_bg(cmd, out_file=f"/tmp/test_{int(time.time())}.txt")

    def bench(self, exe, test_input, iters=3, timeout=120):
        """基准测试: 运行多次取最好时间"""
        b64 = base64.b64encode(test_input.encode()).decode()
        # 用 /usr/bin/time 计时, 运行 iters 次
        script = f"""
best=999999
for i in $(seq 1 {iters}); do
    t=$(echo {b64} | base64 -d | /usr/bin/time -f '%e' {exe} 2>&1 >/dev/null)
    python3 -c "exit(0 if float('$t') < $best else 1)" && best=$t
done
echo $best
"""
        rc, out, err = self.run(script, timeout=timeout * iters)
        try:
            return float(out.strip())
        except:
            return None

# 全局单例
ssh = SSHManager()

if __name__ == "__main__":
    # 自测
    print("=== SSH Manager Self-Test ===")
    rc, out, err = ssh.run("hostname && uname -a")
    print(f"hostname: {out.strip()} (rc={rc})")

    rc, out, err = ssh.run("ls -la /home/azzr/moptm_fusion.cpp 2>/dev/null")
    print(f"source exists: {'yes' if rc == 0 else 'no'}")

    print("\n=== BG Test ===")
    pid, outf = ssh.run_bg("sleep 2 && echo 'BG DONE'")
    print(f"Started PID {pid}")
    import time
    for i in range(5):
        running = ssh.check_bg(pid)
        print(f"  check {i}: {'RUNNING' if running else 'DONE'}")
        if not running:
            break
        time.sleep(1)
    print(f"Output: {ssh.get_bg_output(outf)}")

    print("\n=== Upload + Compile Test ===")
    exe, rc, err = ssh.upload_and_compile(
        "d:/precious_speed/moptm_fusion.cpp",
        "/home/azzr/moptm_fusion.cpp",
        "-DDISABLE_2NXN_CYCLIC"
    )
    print(f"Compile: rc={rc}, exe={exe}")
    if rc != 0:
        print(f"  stderr: {err[:500]}")
    else:
        print("  OK")

    print("\nDone!")
