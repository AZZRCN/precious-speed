p = "div_base16_rprobe.cpp"
s = open(p).read()

anchor1 = "Span window(dividend.ptr + (qn_remaining - this_in), len2 + this_in);"
assert s.count(anchor1) == 1, ("anchor1 count", s.count(anchor1))
ins1 = anchor1 + "\n#ifdef RPROBE\n            std::vector<Limb> _wsnap(window.ptr, window.ptr + window.size);\n#endif"
s = s.replace(anchor1, ins1, 1)

anchor2 = "                std::copy(tprod.data(), tprod.data() + len2, window.ptr);\n                window.size = len2;"
assert s.count(anchor2) == 1, ("anchor2 count", s.count(anchor2))
probe = (
    "#ifdef RPROBE\n"
    "                if ((qn - qn_remaining) / in == 0) {\n"
    "                    std::vector<Limb> _qref(this_in + 1, 0), _rsnap = _wsnap;\n"
    "                    absDivBasicCore(Span(_rsnap.data(), _rsnap.size()), divisor, Span(_qref.data(), this_in + 1));\n"
    "                    bool _qm=false,_rm=false;\n"
    "                    for (size_t _i=0;_i<this_in;_i++) if (_qref[_i]!=qhat_span[_i]) _qm=true;\n"
    "                    for (size_t _i=0;_i<len2;_i++) if (_rsnap[_i]!=tprod[_i]) _rm=true;\n"
    "                    fprintf(stderr,\"[rprobe] block0 qhat_mismatch=%d rp_mismatch=%d\\n\",(int)_qm,(int)_rm);\n"
    "                }\n"
    "#endif\n"
    "                std::copy(tprod.data(), tprod.data() + len2, window.ptr);\n                window.size = len2;"
)
s = s.replace(anchor2, probe, 1)
open(p, "w").write(s)
print("RPROBE PATCHED (unique anchors)")
