"""HEX track VM connector. Host pinned to 10.144.33.157 (override via VM_HOST env).

Reusable by all HEX tooling. Deterministic measurement (cachegrind I refs +
Zen3 cache-sim) is the only accepted performance metric now that wall-clock is
unreliable on the host. See MEMORY.md / HANDOFF.md.
"""
import os
import paramiko

HOST = os.environ.get("VM_HOST", "10.144.33.157")
PORT = 22
USER = "azzr"
PASS = "REDACTED"


def _client():
    c = paramiko.SSHClient()
    c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    c.connect(hostname=HOST, port=PORT, username=USER, password=PASS,
              timeout=30, look_for_keys=False, allow_agent=False)
    return c


def run(cmd, timeout=300):
    c = _client()
    stdin, so, se = c.exec_command(cmd, timeout=timeout)
    out = so.read().decode("utf-8", "replace")
    err = se.read().decode("utf-8", "replace")
    rc = so.channel.recv_exit_status()
    c.close()
    return rc, out, err


def put(local, remote):
    c = _client()
    sftp = c.open_sftp()
    sftp.put(local, remote)
    sftp.close()
    c.close()


def get(remote, local):
    c = _client()
    sftp = c.open_sftp()
    sftp.get(remote, local)
    sftp.close()
    c.close()


if __name__ == "__main__":
    rc, out, err = run("uname -a; nproc; g++ --version|head -1")
    print("rc", rc)
    print(out)
    if err.strip():
        print("ERR", err[:300])
