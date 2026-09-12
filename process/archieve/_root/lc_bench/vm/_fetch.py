#!/usr/bin/env python3
"""下载 VM 上的文件到本地。用法: python _fetch.py <remote> <local>"""
import sys
import paramiko

HOST, USER, PWD = '192.168.1.55', 'azzr', '1234'


def main():
    remote, local = sys.argv[1], sys.argv[2]
    cli = paramiko.SSHClient()
    cli.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    cli.connect(HOST, username=USER, password=PWD, timeout=20)
    sftp = cli.open_sftp()
    sftp.get(remote, local)
    sftp.close()
    cli.close()
    print(f'[fetched] {remote} -> {local}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
