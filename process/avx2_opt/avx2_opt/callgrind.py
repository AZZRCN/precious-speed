"""callgrind 输出解析器。

callgrind 是软件指令计数（valgrind），在 VMware 客户机无虚拟 PMU 时是
唯一可用的指令数真值来源。本模块把 callgrind.out 解析成：
  - totals: 总 Ir
  - functions: {name: {file, self_ir, lines:{lineno:ir}, callees:[(name,ir)]}}
self_ir = 该函数"自身"消耗的 Ir（不含其调用的子函数，排除 cfn 子块），
用于 hotspot 排序最直观。
"""
import re

_COST_RE = re.compile(r"^\s*(\d+)\s+([\d\s]+)$")


def parse(path):
    events = []
    totals = {}
    functions = {}
    cur_fn = None
    cur_file = None
    in_cfn = False

    with open(path, "r", errors="replace") as f:
        for raw in f:
            line = raw.rstrip("\n")
            if not line:
                continue
            if line.startswith("#") or line.startswith("*"):
                continue
            if line.startswith("events:"):
                events = line.split()[1:]
                continue
            if line.startswith("totals:"):
                parts = line.split()[1:]
                for i, name in enumerate(events):
                    if i < len(parts):
                        try:
                            totals[name] = int(parts[i])
                        except ValueError:
                            pass
                continue
            if line.startswith("fl="):
                cur_file = line[3:]
                continue
            if line.startswith("fn="):
                cur_fn = line[3:]
                in_cfn = False
                functions.setdefault(cur_fn, {"file": cur_file, "self_ir": 0,
                                             "lines": {}, "callees": []})
                if cur_file and not functions[cur_fn]["file"]:
                    functions[cur_fn]["file"] = cur_file
                continue
            if line.startswith("cfn="):
                in_cfn = True
                continue
            if line.startswith("cob=") or line.startswith("cfi=") or line.startswith("calls="):
                continue
            # cost line: "position ir..."  (cfn 子块内的属于被调者，不计入自身)
            if cur_fn and not in_cfn:
                m = _COST_RE.match(line)
                if m:
                    pos = int(m.group(1))
                    vals = m.group(2).split()
                    if events and vals:
                        try:
                            ir = int(vals[events.index("Ir")])
                        except (ValueError, IndexError):
                            ir = int(vals[0])
                        functions[cur_fn]["self_ir"] += ir
                        if ir:
                            functions[cur_fn]["lines"][pos] = (
                                functions[cur_fn]["lines"].get(pos, 0) + ir)
    return {"events": events, "totals": totals, "functions": functions}


def total_ir(profile):
    return profile["totals"].get("Ir", 0)


def top_functions(profile, n=15):
    fns = profile["functions"]
    ranked = sorted(fns.items(), key=lambda kv: kv[1]["self_ir"], reverse=True)
    tot = total_ir(profile) or 1
    out = []
    for name, info in ranked[:n]:
        out.append({
            "fn": name,
            "file": info["file"],
            "self_ir": info["self_ir"],
            "pct": 100.0 * info["self_ir"] / tot,
            "hot_lines": sorted(info["lines"].items(),
                                key=lambda kv: kv[1], reverse=True)[:5],
        })
    return out


if __name__ == "__main__":
    import sys
    p = parse(sys.argv[1])
    print("totals:", p["totals"])
    for r in top_functions(p, 10):
        print(f"{r['pct']:6.2f}%  {r['self_ir']:>12}  {r['fn']}  ({r['file']})")
