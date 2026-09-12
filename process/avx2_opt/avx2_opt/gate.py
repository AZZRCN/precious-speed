"""正确性闸门：在 VM 上跑两个二进制对同一输入，逐字节比对输出。

优化器每应用一个变换都必须过此闸门——任何 Ir 下降若伴随输出不一致，
一律判 FAIL（宁可不加该变换）。这是铁律：以 callgrind Ir 为唯一收口标准，
但正确性闸门先于一切。
"""
import remote


def run_binary(remote_bin, input_file, output_file):
    """在 VM 上 `./bin < in > out`，返回 rc。"""
    cmd = f"cd {remote.VM_WORKDIR} && ./{remote_bin} < {input_file} > {output_file}"
    rc, out, err = remote.run(cmd)
    return rc, out, err


def bytecmp(ref_out, cand_out):
    """在 VM 上 cmp 两个输出文件，返回 (equal:bool, detail)。"""
    cmd = f"cd {remote.VM_WORKDIR} && cmp -s {ref_out} {cand_out} && echo EQ || echo NEQ"
    rc, out, err = remote.run(cmd)
    equal = "EQ" in out
    return equal, out.strip() + err.strip()


def gate(ref_bin, cand_bin, input_file):
    """完整闸门：ref/cand 各跑一次并比对，顺带返回 ref 输出供复用。"""
    rc1, _, _ = run_binary(ref_bin, input_file, "ref.out")
    rc2, _, _ = run_binary(cand_bin, input_file, "cand.out")
    if rc1 != 0 or rc2 != 0:
        return False, f"run rc ref={rc1} cand={rc2}"
    return bytecmp("ref.out", "cand.out")
