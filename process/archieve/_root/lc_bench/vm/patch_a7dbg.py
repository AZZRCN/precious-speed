SRC="/home/azzr/addbench/src/add_A7_singlepass.cpp"
DBG="/home/azzr/addbench/src/add_A7_dbg.cpp"
with open(SRC) as f:
    s=f.read()
anchor="        removeLeadingZero();\n        return r;"
dbg=(
"        removeLeadingZero();\n"
"#ifdef A7DEBUG\n"
"        {\n"
"            Integer ref;\n"
"            ref.fromCharRange(p, r);\n"
"            bool ok = (ref.sign == sign);\n"
"            if (ok && ref.data.size() == data.size()) {\n"
"                for (size_t i = 0; i < data.size(); i++) if (ref.data[i] != data[i]) { ok = false; break; }\n"
"            } else ok = false;\n"
"            if (!ok) {\n"
"                fprintf(stderr, \"A7DBG mismatch sign=%d lensingle=%zu lenref=%zu\\n\", (int)sign, data.size(), ref.data.size());\n"
"                fprintf(stderr, \"  single:\");\n"
"                for (size_t i = 0; i < data.size() && i < 40; i++) fprintf(stderr, \" %04d\", (int)data[i]);\n"
"                fprintf(stderr, \"\\n  ref:   \");\n"
"                for (size_t i = 0; i < ref.data.size() && i < 40; i++) fprintf(stderr, \" %04d\", (int)ref.data[i]);\n"
"                fprintf(stderr, \"\\n\");\n"
"                exit(1);\n"
"            }\n"
"        }\n"
"#endif\n"
"        return r;"
)
assert s.count(anchor)==1, s.count(anchor)
s=s.replace(anchor, dbg, 1)
with open(DBG,"w") as f:
    f.write(s)
print("DBG PATCH OK")
