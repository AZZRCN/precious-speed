"""VM control helper: SSH exec + SFTP over paramiko (password auth).

Usage (as a module):
    from vmctl import run, put, get
    rc, out, err = run("uname -a")
    put(r"d:\\path\\local.cpp", "/home/azzr/bench/local.cpp")
"""
import os
import paramiko
import socket

# --- host auto-discovery -------------------------------------------------
# VM 的 IP 会随网络环境变化 (家里 / 外出)。不要再硬编码单个 IP:
#   1. 环境变量 VM_HOST 最优先 (临时覆盖)
#   2. 上次连接成功的 IP 缓存 (.vmhost, 与本文件同目录)
#   3. 内置候选列表, 依次尝试
# 连接成功后写回缓存, 下次直接命中, 无需改代码。
_CACHE = os.path.join(os.path.dirname(os.path.abspath(__file__)), ".vmhost")
CANDIDATES = ["10.144.33.157", "192.168.1.66", "192.168.1.65", "192.168.1.55", "192.168.1.64"]
VM = dict(hostname=CANDIDATES[0], port=22, username="azzr", password="REDACTED")


def _host_order():
    order = []
    env = os.environ.get("VM_HOST")
    if env:
        order.append(env)
    try:
        with open(_CACHE) as f:
            h = f.read().strip()
        if h:
            order.append(h)
    except OSError:
        pass
    order += CANDIDATES
    seen, uniq = set(), []
    for h in order:
        if h not in seen:
            seen.add(h)
            uniq.append(h)
    return uniq


def client():
    last = None
    hosts = _host_order()
    for i, h in enumerate(hosts):
        c = paramiko.SSHClient()
        c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        cfg = dict(VM)
        cfg["hostname"] = h
        try:
            # 首选主机给足超时, 后备主机快速试探
            c.connect(**cfg, timeout=30 if i == 0 else 6,
                      look_for_keys=False, allow_agent=False)
        except Exception as e:  # noqa: BLE001
            last = e
            try:
                c.close()
            except Exception:
                pass
            continue
        VM["hostname"] = h
        try:
            with open(_CACHE, "w") as f:
                f.write(h)
        except OSError:
            pass
        return c
    raise RuntimeError(f"cannot reach VM on any of {hosts}: {last}")


def run(cmd, timeout=600, verbose=True):
    """Run a shell command on the VM, return (rc, out, err)."""
    c = client()
    try:
        stdin, stdout, stderr = c.exec_command(cmd, timeout=timeout)
        try:
            out = stdout.read().decode("utf-8", "replace")
            err = stderr.read().decode("utf-8", "replace")
            rc = stdout.channel.recv_exit_status()
        except socket.timeout:
            # detached background launch: channel stays open; return what we have
            try:
                out = stdout.channel.recv(65536).decode("utf-8", "replace")
            except Exception:
                out = ""
            err = ""
            rc = None  # process still running
    finally:
        c.close()
    if verbose:
        print(f"$ {cmd}")
        if out:
            print(out.rstrip("\n"))
        if err:
            print("STDERR:", err.rstrip("\n"))
        print(f"[rc={rc}]")
    return rc, out, err


def put(local, remote, verbose=True):
    c = client()
    try:
        sftp = c.open_sftp()

        def mkdir_p(path):
            parts = [p for p in path.strip("/").split("/") if p]
            cur = ""
            for p in parts:
                cur = cur + "/" + p if cur else "/" + p
                try:
                    sftp.stat(cur)
                except IOError:
                    try:
                        sftp.mkdir(cur)
                    except IOError:
                        pass

        d = os.path.dirname(remote)
        if d:
            mkdir_p(d)
        sftp.put(local, remote)
        sftp.close()
    finally:
        c.close()
    if verbose:
        print(f"put {local} -> {remote}")


def put_tree(local_dir, remote_dir, verbose=True):
    """Recursively upload a directory (only files)."""
    c = client()
    try:
        sftp = c.open_sftp()

        def mkdir_p(path):
            parts = path.strip("/").split("/")
            cur = ""
            for p in parts:
                cur = cur + "/" + p if cur else p
                try:
                    sftp.stat("/" + cur)
                except IOError:
                    try:
                        sftp.mkdir("/" + cur)
                    except IOError:
                        pass

        for root, _dirs, files in os.walk(local_dir):
            rel = os.path.relpath(root, local_dir)
            rdir = remote_dir if rel == "." else f"{remote_dir}/{rel}"
            mkdir_p(rdir)
            for f in files:
                lf = os.path.join(root, f)
                rf = f"{rdir}/{f}"
                sftp.put(lf, rf)
                if verbose:
                    print(f"put {lf} -> {rf}")
        sftp.close()
    finally:
        c.close()


def get(remote, local, verbose=True):
    """Download a single file from the VM via SFTP."""
    c = client()
    try:
        sftp = c.open_sftp()
        d = os.path.dirname(local)
        if d and not os.path.isdir(d):
            os.makedirs(d, exist_ok=True)
        sftp.get(remote, local)
        sftp.close()
    finally:
        c.close()
    if verbose:
        print(f"get {remote} -> {local}")


if __name__ == "__main__":
    # quick connectivity + env recon when run directly
    for cmd in [
        "uname -a",
        "lscpu | egrep 'Model name|CPU\\(s\\)|Thread|Core|Socket|MHz|flags' | head -20",
        "g++ --version | head -1",
        "nproc",
        "pwd && ls -la",
    ]:
        run(cmd)
