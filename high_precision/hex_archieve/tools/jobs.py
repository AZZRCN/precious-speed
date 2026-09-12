#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
jobs.py —— 统一后台任务管理器（永不阻塞调用方）

设计目标
  1. submit 立即返回 job-id, 绝不等待
  2. 任何任务都有硬超时, 到点自动杀进程树
  3. stdout/stderr 全量落盘, 随时可回收
  4. 进程与调用者解耦: 调用方退出不影响任务

API (命令行)
  python jobs.py submit  <name> -- <cmd...>      [--timeout S] [--cwd DIR]
  python jobs.py list                            列出全部任务
  python jobs.py status  <id|name>               查询状态(非阻塞)
  python jobs.py tail    <id|name> [-n 40]       看输出尾部
  python jobs.py collect <id|name> [--wait S]    回收结果(wait 有上限)
  python jobs.py kill    <id|name>               杀掉任务
  python jobs.py reap                            清理超时任务(巡检)
  python jobs.py clean   [--all]                 清理已完成记录

API (Python)
  import jobs
  jid = jobs.submit("d39_554", ["python", "x.py"], timeout=3600)
  st  = jobs.status(jid)                  # dict, 非阻塞
  out = jobs.tail(jid, 40)
"""
import os
import sys
import json
import time
import signal
import subprocess

HERE = os.path.dirname(os.path.abspath(__file__))
JOBDIR = os.path.join(HERE, ".jobs")
IS_WIN = os.name == "nt"

DEFAULT_TIMEOUT = 1800.0        # 30 min 兜底, 任何任务都不许无限跑


# ---------------------------------------------------------------- 基础设施
def _ensure():
    os.makedirs(JOBDIR, exist_ok=True)


def _meta_path(jid):
    return os.path.join(JOBDIR, f"{jid}.json")


def _out_path(jid):
    return os.path.join(JOBDIR, f"{jid}.out")


def _load(jid):
    p = _meta_path(jid)
    if not os.path.exists(p):
        return None
    try:
        with open(p, "r", encoding="utf-8") as f:
            return json.load(f)
    except Exception:
        return None


def _save(m):
    with open(_meta_path(m["id"]), "w", encoding="utf-8") as f:
        json.dump(m, f, ensure_ascii=False, indent=1)


def _all_ids():
    _ensure()
    ids = []
    for fn in os.listdir(JOBDIR):
        if fn.endswith(".json"):
            ids.append(fn[:-5])
    return sorted(ids)


def _resolve(key):
    """接受 job-id 或 name(取最新的一个)"""
    if _load(key):
        return key
    best, bts = None, -1.0
    for jid in _all_ids():
        m = _load(jid)
        if m and m.get("name") == key and m.get("started", 0) > bts:
            best, bts = jid, m.get("started", 0)
    return best


def _alive(pid):
    """进程是否存活。

    注意: 中文 Windows 的 tasklist 输出是 GBK, 绝不能用 text=True 解码,
    否则 UnicodeDecodeError 会让运行中的任务被误判为已结束。这里一律走 bytes。
    """
    if pid is None:
        return False
    if IS_WIN:
        try:
            r = subprocess.run(["tasklist", "/FI", f"PID eq {pid}", "/NH"],
                               capture_output=True, timeout=20)
            return str(pid).encode() in (r.stdout or b"")
        except Exception:
            # 探测失败时保守认为仍在运行, 避免误杀/误判完成
            return True
    else:
        try:
            os.kill(pid, 0)
            return True
        except OSError:
            return False


def _kill_tree(pid):
    if pid is None:
        return
    if IS_WIN:
        try:
            subprocess.run(["taskkill", "/T", "/F", "/PID", str(pid)],
                           capture_output=True, timeout=20)
        except Exception:
            pass
    else:
        try:
            os.killpg(os.getpgid(pid), signal.SIGKILL)
        except Exception:
            try:
                os.kill(pid, signal.SIGKILL)
            except Exception:
                pass


# ---------------------------------------------------------------- 核心 API
def submit(name, cmd, timeout=DEFAULT_TIMEOUT, cwd=None, env=None):
    """提交后台任务, 立即返回 job-id。cmd 为 list。"""
    _ensure()
    jid = f"{name}-{int(time.time() * 1000) % 100000000:08d}"
    outp = _out_path(jid)
    fout = open(outp, "wb")

    kw = {}
    if IS_WIN:
        kw["creationflags"] = (subprocess.CREATE_NEW_PROCESS_GROUP |
                               getattr(subprocess, "DETACHED_PROCESS", 0x00000008))
    else:
        kw["start_new_session"] = True

    e = dict(os.environ)
    if env:
        e.update(env)

    p = subprocess.Popen(cmd, stdout=fout, stderr=subprocess.STDOUT,
                         cwd=cwd or HERE, env=e, **kw)
    fout.close()

    m = {"id": jid, "name": name, "cmd": cmd, "pid": p.pid,
         "started": time.time(), "timeout": float(timeout),
         "cwd": cwd or HERE, "state": "running",
         "rc": None, "ended": None, "out": outp}
    _save(m)
    return jid


def status(key, reap=True):
    """非阻塞查询。reap=True 时顺带处理超时。"""
    jid = _resolve(key)
    if not jid:
        return {"state": "unknown", "id": key}
    m = _load(jid)
    if not m:
        return {"state": "unknown", "id": key}

    if m["state"] == "running":
        el = time.time() - m["started"]
        if not _alive(m["pid"]):
            m["state"] = "done"
            # 用输出文件最后写入时间近似真实结束时刻, 而不是"被发现"的时刻
            try:
                m["ended"] = max(os.path.getmtime(m["out"]), m["started"])
            except Exception:
                m["ended"] = time.time()
            _save(m)
        elif reap and el > m["timeout"]:
            _kill_tree(m["pid"])
            m["state"] = "timeout"
            m["ended"] = time.time()
            _save(m)

    m["elapsed"] = (m.get("ended") or time.time()) - m["started"]
    try:
        m["bytes"] = os.path.getsize(m["out"])
    except Exception:
        m["bytes"] = 0
    return m


def tail(key, n=40):
    jid = _resolve(key)
    if not jid:
        return f"[no such job: {key}]"
    m = _load(jid)
    try:
        with open(m["out"], "r", encoding="utf-8", errors="replace") as f:
            lines = f.read().splitlines()
        return "\n".join(lines[-n:])
    except Exception as ex:
        return f"[read fail: {ex}]"


def head(key, n=40):
    jid = _resolve(key)
    if not jid:
        return f"[no such job: {key}]"
    m = _load(jid)
    try:
        with open(m["out"], "r", encoding="utf-8", errors="replace") as f:
            lines = f.read().splitlines()
        return "\n".join(lines[:n])
    except Exception as ex:
        return f"[read fail: {ex}]"


def collect(key, wait=0.0, poll=2.0):
    """回收结果。wait 是本次调用最多等待的秒数(有上限, 不会永久阻塞)。"""
    t0 = time.time()
    while True:
        st = status(key)
        if st.get("state") != "running":
            st["output"] = tail(key, 100000)
            return st
        if time.time() - t0 >= wait:
            st["output"] = tail(key, 200)
            return st
        time.sleep(min(poll, max(0.2, wait - (time.time() - t0))))


def kill(key):
    jid = _resolve(key)
    if not jid:
        return False
    m = _load(jid)
    _kill_tree(m["pid"])
    m["state"] = "killed"
    m["ended"] = time.time()
    _save(m)
    return True


def reap():
    """巡检: 把所有超时任务杀掉。可周期调用。"""
    out = []
    for jid in _all_ids():
        st = status(jid, reap=True)
        if st.get("state") in ("timeout",):
            out.append(jid)
    return out


def ls():
    rows = []
    for jid in _all_ids():
        rows.append(status(jid))
    rows.sort(key=lambda r: r.get("started", 0), reverse=True)
    return rows


# ---------------------------------------------------------------- CLI
def _fmt(st):
    tag = {"running": "RUN ", "done": "DONE", "timeout": "TMO ",
           "killed": "KILL", "unknown": "??? "}.get(st.get("state"), "????")
    return (f"{tag} {st.get('id','?'):<28} "
            f"t={st.get('elapsed',0):7.1f}s  "
            f"lim={st.get('timeout',0):6.0f}s  "
            f"{st.get('bytes',0):>9,}B  pid={st.get('pid')}")


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 1
    act = argv[1]

    if act == "submit":
        if "--" not in argv:
            print("usage: jobs.py submit <name> [--timeout S] [--cwd D] -- <cmd...>")
            return 2
        k = argv.index("--")
        headargs, cmd = argv[2:k], argv[k + 1:]
        name = headargs[0] if headargs else "job"
        tmo, cwd = DEFAULT_TIMEOUT, None
        i = 1
        while i < len(headargs):
            if headargs[i] == "--timeout":
                tmo = float(headargs[i + 1]); i += 2
            elif headargs[i] == "--cwd":
                cwd = headargs[i + 1]; i += 2
            else:
                i += 1
        jid = submit(name, cmd, timeout=tmo, cwd=cwd)
        print(jid)
        return 0

    if act == "list":
        for st in ls():
            print(_fmt(st))
        return 0

    if act == "status":
        print(_fmt(status(argv[2])))
        return 0

    if act == "tail":
        n = 40
        if "-n" in argv:
            n = int(argv[argv.index("-n") + 1])
        print(tail(argv[2], n))
        return 0

    if act == "head":
        n = 40
        if "-n" in argv:
            n = int(argv[argv.index("-n") + 1])
        print(head(argv[2], n))
        return 0

    if act == "collect":
        w = 0.0
        if "--wait" in argv:
            w = float(argv[argv.index("--wait") + 1])
        st = collect(argv[2], wait=w)
        print(_fmt(st))
        print("--- output ---")
        print(st.get("output", ""))
        return 0 if st.get("state") == "done" else 3

    if act == "kill":
        print("killed" if kill(argv[2]) else "no such job")
        return 0

    if act == "reap":
        r = reap()
        print(f"reaped {len(r)}: {r}")
        return 0

    if act == "clean":
        allf = "--all" in argv
        n = 0
        for jid in _all_ids():
            st = status(jid)
            if allf or st.get("state") in ("done", "killed", "timeout"):
                for p in (_meta_path(jid), _out_path(jid)):
                    try:
                        os.remove(p); 
                    except Exception:
                        pass
                n += 1
        print(f"cleaned {n}")
        return 0

    print(__doc__)
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
