#!/usr/bin/env python3
# vm_ssh.py - paramiko-based SSH helper for the local Intel VM used as a
# correctness/perf-proxy rig for Library Checker (AMD EPYC 7B13 = Zen3 Milan).
#
# NOTE (cross-arch rule): this VM is INTEL. Any perf/cache numbers gathered
# here are a PROXY for tuning direction only. Final LEAF/threshold tuning MUST
# be confirmed by LC (AMD) submission receipts. Never treat Intel numbers as truth.
#
# Usage:
#   python vm_ssh.py run  "uname -a"
#   python vm_ssh.py put  local/path  /remote/path
#   python vm_ssh.py get  /remote/path local/path
#   python vm_ssh.py putdir local_dir /remote_dir
import sys, os, paramiko

# 2026-08-13(晚) 主用机迁移到 .66 (Intel 285H / 8 vCPU / 未开虚拟化 -> perf PMU 可能不可用)
# 备用: .55 (i7-11370H). 用 VM_HOST=192.168.1.55 环境变量临时切回.
PORT = int(os.environ.get("VM_PORT", "22"))
USER = os.environ.get("VM_USER", "azzr")
PASS = os.environ.get("VM_PASS", "1234")
# 自动探测：env VM_HOST 优先；否则轮流试候选 IP（.66 / .55 至少一台在线）
_ENV_HOST = os.environ.get("VM_HOST")
CANDIDATE_HOSTS = [_ENV_HOST] if _ENV_HOST else ["192.168.1.66", "192.168.1.55"]

_ssh = None
def conn():
    global _ssh
    if _ssh is not None and _ssh.get_transport().is_active():
        return _ssh
    last_err = None
    for h in CANDIDATE_HOSTS:
        try:
            c = paramiko.SSHClient()
            c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
            c.connect(h, port=PORT, username=USER, password=PASS, timeout=6,
                      look_for_keys=False, allow_agent=False)
            _ssh = c
            return c
        except Exception as ex:
            last_err = ex
    raise RuntimeError(f"no VM reachable among {CANDIDATE_HOSTS}: {last_err}")

def run(cmd, capture=True):
    c = conn()
    stdin, stdout, stderr = c.exec_command(cmd)
    out = stdout.read().decode("utf-8", "replace") if capture else ""
    err = stderr.read().decode("utf-8", "replace") if capture else ""
    rc = stdout.channel.recv_exit_status()
    return rc, out, err

def _sftp():
    return conn().open_sftp()

def put(local, remote):
    s = _sftp()
    # ensure parent dir
    parent = os.path.dirname(remote)
    run(f"mkdir -p '{parent}'")
    s.put(local, remote)
    s.close()

def get(remote, local):
    s = _sftp()
    os.makedirs(os.path.dirname(local) or ".", exist_ok=True)
    s.get(remote, local)
    s.close()

def putdir(local_dir, remote_dir):
    for root, _, files in os.walk(local_dir):
        for f in files:
            l = os.path.join(root, f)
            r = os.path.join(remote_dir, os.path.relpath(l, local_dir)).replace("\\", "/")
            put(l, r)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("usage: vm_ssh.py run|put|get|putdir ..."); sys.exit(1)
    op = sys.argv[1]
    if op == "run":
        rc, o, e = run(" ".join(sys.argv[2:]))
        sys.stdout.write(o); sys.stderr.write(e); sys.exit(rc)
    elif op == "put":
        put(sys.argv[2], sys.argv[3]); print("put ok")
    elif op == "get":
        get(sys.argv[2], sys.argv[3]); print("get ok")
    elif op == "putdir":
        putdir(sys.argv[2], sys.argv[3]); print("putdir ok")
