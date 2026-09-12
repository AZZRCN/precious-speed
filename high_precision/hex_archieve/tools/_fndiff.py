# AZZRCN
# 逐函数体哈希比对 v13.cpp / v17.cpp, 找出 2 幂路径上被改动的函数。
import re, hashlib, sys, os
R = r"d:\hex_precious_speed\work\mul"
def funcs(path):
    src = open(path, "r", encoding="utf-8", errors="replace").read().splitlines()
    out, i = {}, 0
    pat = re.compile(r"^(?:static\s+)?(?:inline\s+)?[A-Za-z_][\w:<>,\* ]*\s[\*&]?\s*([A-Za-z_]\w*)\s*\([^;]*\)\s*\{\s*$")
    while i < len(src):
        m = pat.match(src[i].rstrip())
        if m:
            name, depth, body, j = m.group(1), 0, [], i
            while j < len(src):
                body.append(src[j])
                depth += src[j].count("{") - src[j].count("}")
                if depth == 0 and j > i: break
                j += 1
            txt = "\n".join(l.split("//")[0].rstrip() for l in body)
            txt = re.sub(r"\s+", " ", txt).strip()
            out.setdefault(name, []).append((hashlib.md5(txt.encode()).hexdigest()[:8], i + 1, len(body)))
            i = j
        i += 1
    return out
a, b = funcs(os.path.join(R, "v13.cpp")), funcs(os.path.join(R, "v17.cpp"))
print("%-22s %-10s %-10s %s" % ("func", "v13", "v17", "verdict"))
for k in sorted(set(a) | set(b)):
    ха = a.get(k, [("-", 0, 0)])[0]
    hb = b.get(k, [("-", 0, 0)])[0]
    if ха[0] == hb[0]:
        continue
    v = "ONLY-v17" if k not in a else ("ONLY-v13" if k not in b else "CHANGED")
    print("%-22s %-10s %-10s %s  (L%d/%d vs L%d/%d)" % (k, ха[0], hb[0], v, ха[1], ха[2], hb[1], hb[2]))
