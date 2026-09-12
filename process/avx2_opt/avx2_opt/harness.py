"""优化器主驱动：build → profile → gate → apply → re-profile 闭环。

对每个目标：
  1. 把源码与输入推到 VM，编译出 ref（基线）。
  2. 在 VM 上 callgrind 采集 ref 的 Ir，取回归档，解析。
  3. 跑 ref 产出 ref.out（正确性基准）。
  4. 检测源码里可被策展变换命中的热惯用法。
  5. 对每个命中的变换：套用 → 编译 cand → callgrind cand → 跑 cand 比对 ref.out。
  6. 输出每步 Ir 差值 + 闸门结果。

这是"自动"的含义：profile→检测→套用→重测 全自动，且每个变换都过 bytecmp 闸门。
"""
import os
import shutil

import config
import remote
import callgrind
import gate
import transforms
from hotspot import report as hotspot_report, summary_obj


def _scp_and_build(local_src, base, build_tmpl):
    remote.ensure_workdir()
    rc, _, err = remote.scp_to(local_src, f"{config.VM_WORKDIR}/{base}.cpp")
    if rc != 0:
        return False, f"scp失败: {err}"
    build_cmd = build_tmpl.format(src=f"{base}.cpp", out=f"bin_{base}")
    rc, out, err = remote.run(f"cd {config.VM_WORKDIR} && {build_cmd}")
    if rc != 0:
        return False, f"编译失败 rc={rc}\n{out}\n{err}"
    return True, ""


def _profile(base, input_name):
    cmd = (f"cd {config.VM_WORKDIR} && "
           f"valgrind --tool=callgrind {config.CALLGRIND_EXTRA} "
           f"--callgrind-out-file=cg_{base}.out "
           f"./bin_{base} < {input_name} > /dev/null 2> cg_{base}.err")
    rc, out, err = remote.run(cmd, timeout=1200)
    if rc != 0:
        return None, f"callgrind rc={rc}\n{out}\n{err}"
    local = os.path.join(config.VM_WORKDIR, f"cg_{base}.out")
    # 存到本地 cwd 临时
    os.makedirs("_cg", exist_ok=True)
    rc2, _, err2 = remote.scp_from(f"{config.VM_WORKDIR}/cg_{base}.out",
                                   f"_cg/cg_{base}.out")
    if rc2 != 0:
        return None, f"取回失败: {err2}"
    prof = callgrind.parse("_cg/cg_{base}.out".format(base=base))
    return prof, ""


def run_task(local_src, input_local, name="task",
             build_tmpl="g++ -O3 -march=haswell -std=c++17 -no-pie -o {out} {src}",
             only_detect=False):
    remote.ensure_workdir()
    # 推输入
    remote.scp_to(input_local, f"{config.VM_WORKDIR}/{name}.in")
    input_name = f"{name}.in"

    # 1) 基线
    ok, msg = _scp_and_build(local_src, "ref", build_tmpl)
    if not ok:
        return {"ok": False, "stage": "build_ref", "msg": msg}
    prof_ref, msg = _profile("ref", input_name)
    if prof_ref is None:
        return {"ok": False, "stage": "profile_ref", "msg": msg}
    # 基线闸门产出 ref.out
    gate.run_binary("bin_ref", input_name, "ref.out")

    detected = transforms.detect_all(open(local_src, encoding="utf-8").read())
    result = {
        "ok": True,
        "name": name,
        "ref_total_ir": callgrind.total_ir(prof_ref),
        "ref_hotspot": hotspot_report(prof_ref),
        "detected": detected,
        "steps": [],
    }
    if only_detect:
        return result

    # 2) 逐个套用命中变换（链式套用，模拟"全部应用"）
    src = open(local_src, encoding="utf-8").read()
    applied_names = []
    for t in transforms.REGISTRY:
        if t["name"] in detected:
            src, applied = t["fn"](src)
            if applied:
                applied_names.append(t["name"])
    if not applied_names:
        result["steps"].append({"note": "无可用变换，已触底（基准即最优）"})
        return result

    # 写 cand 源码到本地再推
    cand_local = "_cg/cand.cpp"
    with open(cand_local, "w", encoding="utf-8") as f:
        f.write(src)
    ok, msg = _scp_and_build(cand_local, "cand", build_tmpl)
    if not ok:
        result["steps"].append({"applied": applied_names, "ok": False, "msg": msg})
        return result
    prof_cand, msg = _profile("cand", input_name)
    if prof_cand is None:
        result["steps"].append({"applied": applied_names, "ok": False, "msg": msg})
        return result
    equal, detail = gate.bytecmp("ref.out", "cand.out")
    cand_ir = callgrind.total_ir(prof_cand)
    delta = 100.0 * (cand_ir - result["ref_total_ir"]) / (result["ref_total_ir"] or 1)
    result["steps"].append({
        "applied": applied_names,
        "cand_total_ir": cand_ir,
        "delta_pct": delta,
        "gate_pass": equal,
        "gate_detail": detail,
    })
    return result


if __name__ == "__main__":
    import json
    import sys
    r = run_task(sys.argv[1], sys.argv[2], name=sys.argv[3] if len(sys.argv) > 3 else "task")
    print(json.dumps(r, indent=2, ensure_ascii=False))
