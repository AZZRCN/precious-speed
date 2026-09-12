#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
log_submit.py — LC 提交回执归档器（用户 2026-08-07 明令的强制流程，脚本化以杜绝漏记）

用法:
    python log_submit.py <版本标签> <回执文本文件>          # 例: python log_submit.py D49 recv.txt
    python log_submit.py <版本标签> -                       # 从 stdin 读回执
    python log_submit.py --index                            # 只重建 INDEX 总表

做的三件事（缺一不可）:
  1. 解析回执 -> submit_history/<ID>_<版本>_<headline>ms.md（原文 + 与上一条的逐点 Δ 对照 + 抖动判读）
  2. 更新 submit_history/INDEX.md 总表
  3. 把回执原文粘到 best/div_<版本>.cpp 文件开头 /* */（保持原换行风格）
"""
import os
import re
import sys
import json
import glob

ROOT = os.path.dirname(os.path.abspath(__file__))
SH = os.path.join(ROOT, "submit_history")
BEST = os.path.join(ROOT, "best")


def parse(text):
    """从粘贴的回执文本抽取: 提交ID / 日期 / headline / {case: ms}"""
    sub_id = None
    m = re.search(r"Submission\s*#?(\d+)", text)
    if m:
        sub_id = m.group(1)
    date = None
    headline = None
    cases = {}
    for line in text.splitlines():
        s = line.strip()
        if not s:
            continue
        # 总行: 391243  2026/8/7 21:16:02  ...  AC  35 ms  46.04 Mib
        mm = re.match(r"^(\d{6,})\s+(\d{4}/\d{1,2}/\d{1,2}[^\t]*?)\s", s)
        if mm and sub_id and mm.group(1) == sub_id:
            date = mm.group(2).strip()
            mt = re.search(r"AC\s+(\d+)\s*ms", s)
            if mt:
                headline = int(mt.group(1))
            continue
        # 用例行: name  AC  29 ms  5.62 Mib
        mc = re.match(r"^([A-Za-z_][A-Za-z0-9_]*)\s+(AC|WA|TLE|RE|MLE|CE)\s+(\d+)\s*ms", s)
        if mc:
            cases[mc.group(1)] = int(mc.group(3))
            continue
    if headline is None and cases:
        headline = max(cases.values())
    return sub_id, date, headline, cases


def load_prev():
    """读 INDEX 里最后一条已归档记录的 cases（用于 Δ 对照）"""
    recs = []
    for p in sorted(glob.glob(os.path.join(SH, "*.json"))):
        try:
            recs.append(json.load(open(p, "r", encoding="utf-8")))
        except Exception:
            pass
    recs.sort(key=lambda r: r.get("id", ""))
    return recs


def prepend_header(version, text, sub_id, headline):
    path = os.path.join(BEST, "div_%s.cpp" % version)
    if not os.path.exists(path):
        print("!! 未找到 %s，跳过文件头粘贴" % path)
        return
    raw = open(path, "r", encoding="utf-8", newline="").read()
    if re.match(r"^\s*/\*\s*[\r\n]+=== LC 提交回执", raw):
        print("!! %s 已有回执头，跳过（如需更新请手工编辑）" % path)
        return
    crlf = "\r\n" in raw
    body = "\n".join(["/*",
                      "=== LC 提交回执 (本文件即该次提交的源码) ===",
                      "Submission #%s  ==  %s ms" % (sub_id, headline),
                      ""] + text.strip().splitlines() + [
                      "",
                      "记录: submit_history/%s_%s_%sms.md" % (sub_id, version, headline),
                      "*/", ""])
    if crlf:
        body = body.replace("\n", "\r\n")
    open(path, "w", encoding="utf-8", newline="").write(body + raw)
    print("OK 回执已粘到 %s 文件头" % path)


def write_record(version, text, sub_id, date, headline, cases, prev):
    os.makedirs(SH, exist_ok=True)
    lines = ["# Submission #%s — %s ms — `best/div_%s.cpp` (%s)" % (sub_id, headline, version, date or "?"),
             "", "```", text.strip(), "```", ""]
    if prev:
        p = prev[-1]
        pc = p.get("cases", {})
        lines += ["## 与 #%s (%s, %s ms) 逐点对照" % (p["id"], p["version"], p["headline"]), "",
                  "| case | #%s | #%s | Δ |" % (p["id"], sub_id), "|---|---|---|---|"]
        big = []
        for k in cases:
            if k in pc:
                d = cases[k] - pc[k]
                mark = "  **<<<**" if abs(d) >= 5 else ""
                lines.append("| %s | %d | %d | %+d%s |" % (k, pc[k], cases[k], d, mark))
                if abs(d) >= 5:
                    big.append((k, pc[k], cases[k], d))
        lines.append("")
        up = [b for b in big if b[3] > 0]
        dn = [b for b in big if b[3] < 0]
        lines += ["## 抖动判读", ""]
        if up and dn:
            lines.append("- **双向大幅摆动**（升 %d 点 / 降 %d 点，幅度 ≥5 ms）⇒ 强烈提示**评测机抖动**，"
                         "除非改动能物理解释该方向。" % (len(up), len(dn)))
        elif up:
            lines.append("- **仅单向劣化** %s ⇒ 需与上一次同版本提交比对；若同点位复现且方向一致，判**真回归**。"
                         % ", ".join(b[0] for b in up))
        elif dn:
            lines.append("- **仅单向改善** %s ⇒ 若能与源码改动对上，判**真收益**。" % ", ".join(b[0] for b in dn))
        else:
            lines.append("- 无 ≥5 ms 点位变化。")
        lines += ["- 提醒: headline = max over cases，抖动会概率性抬高 headline。"
                  "**单次提交不能判定优劣，须重交 1~2 次看分布。**", ""]
    top = max(cases.values()) if cases else 0
    peaks = [k for k, v in cases.items() if v == top]
    lines += ["## headline 构成", "",
              "%s ms 由 %d 个点撑住: %s" % (top, len(peaks), " / ".join("`%s`" % p for p in peaks)),
              "⇒ 想降 headline 必须**同时**压这些点，只压其中一点无效。", ""]

    md = os.path.join(SH, "%s_%s_%sms.md" % (sub_id, version, headline))
    open(md, "w", encoding="utf-8", newline="\n").write("\n".join(lines))
    json.dump({"id": sub_id, "version": version, "date": date, "headline": headline, "cases": cases},
              open(os.path.join(SH, "%s.json" % sub_id), "w", encoding="utf-8"),
              ensure_ascii=False, indent=1)
    print("OK 记录已写 %s" % md)
    return md


def rebuild_index():
    recs = load_prev()
    if not recs:
        print("(无 json 记录, INDEX 未变)")
        return
    best = min(recs, key=lambda r: r["headline"])
    lines = ["# 提交记录总索引 — Division of Big Integers", "",
             "> 由 `log_submit.py` 自动维护。**用户每粘一次回执必须立即跑一次本脚本。**", "",
             "| 提交 ID | 时间 | 代码 | headline |", "|---|---|---|---|"]
    for r in recs:
        star = " ★BEST" if r is best else ""
        lines.append("| #%s | %s | %s | **%s ms**%s |" % (r["id"], r.get("date", "?"), r["version"], r["headline"], star))
    lines += ["", "**当前纪录 = #%s = %s ms = `best/div_%s.cpp`**" % (best["id"], best["headline"], best["version"]), ""]
    open(os.path.join(SH, "INDEX_auto.md"), "w", encoding="utf-8", newline="\n").write("\n".join(lines))
    print("OK INDEX_auto.md 已重建")


def main():
    a = sys.argv[1:]
    if not a or a[0] == "--index":
        rebuild_index()
        return
    if len(a) < 2:
        print(__doc__)
        return
    version, src = a[0], a[1]
    text = sys.stdin.read() if src == "-" else open(src, "r", encoding="utf-8", errors="replace").read()
    sub_id, date, headline, cases = parse(text)
    if not sub_id or not cases:
        print("!! 解析失败: id=%r cases=%d。请检查回执格式。" % (sub_id, len(cases)))
        return
    print("解析: #%s  %s  headline=%s ms  用例 %d 个" % (sub_id, date, headline, len(cases)))
    prev = load_prev()
    write_record(version, text, sub_id, date, headline, cases, prev)
    prepend_header(version, text, sub_id, headline)
    rebuild_index()


if __name__ == "__main__":
    main()
