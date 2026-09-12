#!/usr/bin/env python3
"""通用: 上传本地脚本到 VM 并执行。用法: python _go.py <local_file> <remote_name> <cmd>"""
import sys
import paramiko

HOST, USER, PWD = '192.168.1.55', 'azzr', '1234'


def main():
    local, remote, cmd = sys.argv[1], sys.argv[2], sys.argv[3]
    cli = paramiko.SSHClient()
    cli.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    cli.connect(HOST, username=USER, password=PWD, timeout=20)
    sftp = cli.open_sftp()
    sftp.put(local, remote)
    sftp.close()
    print(f'[uploaded] {local} -> {remote}', flush=True)
    chan = cli.get_transport().open_session()
    chan.get_pty()
    chan.exec_command(cmd)
    buf = b''
    while True:
        if chan.recv_ready():
            data = chan.recv(65536)
            if not data:
                break
            buf += data
            sys.stdout.write(data.decode('utf-8', 'replace'))
            sys.stdout.flush()
        elif chan.exit_status_ready():
            while chan.recv_ready():
                data = chan.recv(65536)
                if not data:
                    break
                sys.stdout.write(data.decode('utf-8', 'replace'))
            break
    rc = chan.recv_exit_status()
    print(f'\n[rc={rc}]')
    cli.close()
    return rc


if __name__ == '__main__':
    sys.exit(main())
