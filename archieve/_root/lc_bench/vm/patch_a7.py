SRC="/home/azzr/addbench/src/add_A7_singlepass.cpp"
def rd(p):
    with open(p) as f: return f.read()
s=rd(SRC)
qp=rd("/home/azzr/a7_qp.inc")
fs=rd("/home/azzr/a7_fs.inc")
old=rd("/home/azzr/a7_oldmain.inc")
new=rd("/home/azzr/a7_newmain.inc")
a1="    static inline int64_t parseSIMD(const char *s, size_t &len) {"
assert s.count(a1)==1, ("a1",s.count(a1))
s=s.replace(a1, qp.rstrip("\n")+"\n\n"+a1, 1)
a2="        operator std::string() const\n        {\n            return toString();\n        }"
assert s.count(a2)==1, ("a2",s.count(a2))
s=s.replace(a2, fs.rstrip("\n")+"\n\n"+a2, 1)
assert s.count(old)==1, ("old",s.count(old))
s=s.replace(old,new,1)
with open(SRC,"w") as f: f.write(s)
print("PATCH OK")
