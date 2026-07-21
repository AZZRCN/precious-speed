#!/usr/bin/env python3
# SSH 命令执行工具（基于 paramiko）
# 用法: python ssh_exec.py "command1" "command2" ...
# 或:   python ssh_exec.py --file local_path --remote remote_path  # 上传
# 或:   python ssh_exec.py --get remote_path --local local_path    # 下载
import sys
import paramiko
import os

HOST = "192.168.1.55"
USER = "azzr"
PWD = "1234"


def get_client():
    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    client.connect(HOST, username=USER, password=PWD, timeout=10)
    return client


def run_cmd(client, cmd):
    stdin, stdout, stderr = client.exec_command(cmd, get_pty=False)
    out = stdout.read().decode("utf-8", errors="replace")
    err = stderr.read().decode("utf-8", errors="replace")
    rc = stdout.channel.recv_exit_status()
    return rc, out, err


def upload(client, local, remote):
    sftp = client.open_sftp()
    sftp.put(local, remote)
    sftp.close()


def download(client, remote, local):
    sftp = client.open_sftp()
    sftp.get(remote, local)
    sftp.close()


def main():
    if len(sys.argv) < 2:
        print("Usage: python ssh_exec.py \"cmd1\" \"cmd2\" ...")
        print("       python ssh_exec.py --file local remote")
        print("       python ssh_exec.py --get remote local")
        sys.exit(1)

    client = get_client()
    print(f"[OK] SSH connected to {USER}@{HOST}")

    if sys.argv[1] == "--file":
        upload(client, sys.argv[2], sys.argv[3])
        print(f"[OK] Uploaded {sys.argv[2]} -> {sys.argv[3]}")
    elif sys.argv[1] == "--get":
        download(client, sys.argv[2], sys.argv[3])
        print(f"[OK] Downloaded {sys.argv[2]} -> {sys.argv[3]}")
    else:
        for cmd in sys.argv[1:]:
            print(f"\n$ {cmd}")
            rc, out, err = run_cmd(client, cmd)
            if out:
                print(out, end="")
            if err:
                print(f"[STDERR] {err}", end="")
            print(f"[EXIT {rc}]")

    client.close()


if __name__ == "__main__":
    main()
