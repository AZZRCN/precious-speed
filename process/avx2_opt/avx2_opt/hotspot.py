"""hotspot 排序与报告（基于 callgrind 解析结果）。"""
from callgrind import top_functions, total_ir


def report(profile, n=12):
    tot = total_ir(profile)
    lines = []
    lines.append(f"总 Ir = {tot:,}")
    lines.append(f"{'占比':>7}  {'self_Ir':>14}  函数  (文件)")
    for r in top_functions(profile, n):
        lines.append(f"{r['pct']:6.2f}%  {r['self_ir']:>14,}  {r['fn']}  ({r['file']})")
    return "\n".join(lines)


def summary_obj(profile, n=12):
    return {
        "total_ir": total_ir(profile),
        "top": top_functions(profile, n),
    }
