"""SSH / SCP 远程执行封装（依赖系统 ssh/scp，不引入第三方库）。

设计初衷：callgrind 只能在 Linux 上跑（valgrind），而开发机是 Windows。
所有"采集"动作通过 SSH 派发到 .66 VM，结果 scp 回本地解析。
"""
import os
import shlex
import subprocess
import sys

import config


def _ssh_base():
    return [
        "ssh",
        "-i", os.path.expanduser(config.VM_KEY),
        "-o", "StrictHostKeyChecking=no",
        "-o", "BatchMode=yes",
        "-o", "ConnectTimeout=15",
        f"{config.VM_USER}@{config.VM_HOST}",
    ]


def run(cmd, timeout=600):
    """在 VM 上执行一条 shell 命令，返回 (rc, stdout, stderr)。"""
    full = _ssh_base() + ["bash", "-c", cmd]
    p = subprocess.run(full, capture_output=True, text=True, timeout=timeout)
    return p.returncode, p.stdout, p.stderr


def scp_to(local, remote, timeout=300):
    """把本地文件推到 VM。"""
    target = f"{config.VM_USER}@{config.VM_HOST}:{remote}"
    p = subprocess.run(
        ["scp", "-i", os.path.expanduser(config.VM_KEY),
         "-o", "StrictHostKeyChecking=no", "-o", "BatchMode=yes",
         local, target],
        capture_output=True, text=True, timeout=timeout,
    )
    return p.returncode, p.stdout, p.stderr


def scp_from(remote, local, timeout=300):
    """从 VM 拉回文件。"""
    src = f"{config.VM_USER}@{config.VM_HOST}:{remote}"
    p = subprocess.run(
        ["scp", "-i", os.path.expanduser(config.VM_KEY),
         "-o", "StrictHostKeyChecking=no", "-o", "BatchMode=yes",
         src, local],
        capture_output=True, text=True, timeout=timeout,
    )
    return p.returncode, p.stdout, p.stderr


def ensure_workdir():
    run(f"mkdir -p {config.VM_WORKDIR}")


def check():
    """连通性自检。"""
    rc, out, err = run("echo OK; which valgrind; hostname -I")
    ok = rc == 0 and "OK" in out and "valgrind" in out
    return ok, out.strip(), err.strip()


if __name__ == "__main__":
    ok, out, err = check()
    print("reachable:", ok)
    print(out)
    if err:
        print("err:", err, file=sys.stderr)
