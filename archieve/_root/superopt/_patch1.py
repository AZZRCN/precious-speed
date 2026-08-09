import io, sys
p = "so_core.cpp"
s = io.open(p, encoding="utf-8").read()

s = s.replace("static std::vector<int>      poolIsConst;",
              "static std::vector<int>      poolKind;   // 0=var 1=num-const 2=shift-const")
s = s.replace("poolIsConst.push_back", "poolKind.push_back")
s = s.replace("poolIsConst.pop_back", "poolKind.pop_back")
s = s.replace("if (poolIsConst[i]) continue;               // P3: 不对常量做一元运算",
              "if (poolKind[i]) continue;                  // P3: 不对常量做一元运算")
s = s.replace("if (poolIsConst[i] && poolIsConst[j]) continue;",
              "if (poolKind[i] && poolKind[j]) continue;")

old = ("                    // shift/bzhi/rol \u7684\u7b2c\u4e8c\u64cd\u4f5c\u6570\u5fc5\u987b\u662f\u5e38\u91cf(\u5426\u5219\u9700\u989d\u5916\u5bc4\u5b58\u5668\u4e14\u5c11\u89c1)\n"
       "                    if ((op == OP_SHL || op == OP_SHR || op == OP_SAR ||\n"
       "                         op == OP_ROL || op == OP_BZHI) && !poolIsConst[j]) continue;")
new = ("                    // P7 \u5e38\u91cf\u5206\u7c7b: \u79fb\u4f4d\u91cf\u4e0e\u6570\u503c\u5e38\u91cf\u4e92\u4e0d\u4e32\u7528 \u2014\u2014 \u63a7\u5236\u7206\u70b8\u7684\u5173\u952e\n"
       "                    const bool isShiftOp = (op == OP_SHL || op == OP_SHR || op == OP_SAR ||\n"
       "                                            op == OP_ROL || op == OP_BZHI);\n"
       "                    if (isShiftOp) { if (poolKind[j] != 2) continue; }\n"
       "                    else           { if (poolKind[i] == 2 || poolKind[j] == 2) continue; }")
assert old in s, "shift-guard block not found"
s = s.replace(old, new)

s = s.replace("for (int i = nBase - (int)0; i < n; i++) {", "for (int i = 0; i < n; i++) {")

old2 = ('    // consts\n'
        '    std::vector<uint64_t> consts;\n'
        '    if (J.findKey("consts")) consts = J.readU64Array();\n'
        '    for (uint64_t c : consts) {\n'
        '        Vec v{}; for (int t = 0; t < K; t++) v.v[t] = c;\n'
        '        pool.push_back(v); poolKind.push_back(1);\n'
        '        char nb[32]; snprintf(nb, sizeof nb, "%llu", (unsigned long long)c);\n'
        '        poolName.push_back(nb);\n'
        '    }')
new2 = ('    // consts (\u6570\u503c) \u4e0e shifts (\u79fb\u4f4d\u91cf) \u5206\u5f00\n'
        '    std::vector<uint64_t> consts, shifts;\n'
        '    if (J.findKey("consts")) consts = J.readU64Array();\n'
        '    if (J.findKey("shifts")) shifts = J.readU64Array();\n'
        '    for (uint64_t c : consts) {\n'
        '        Vec v{}; for (int t = 0; t < K; t++) v.v[t] = c;\n'
        '        pool.push_back(v); poolKind.push_back(1);\n'
        '        char nb[32]; snprintf(nb, sizeof nb, "%llu", (unsigned long long)c);\n'
        '        poolName.push_back(nb);\n'
        '    }\n'
        '    for (uint64_t c : shifts) {\n'
        '        Vec v{}; for (int t = 0; t < K; t++) v.v[t] = c;\n'
        '        pool.push_back(v); poolKind.push_back(2);\n'
        '        char nb[32]; snprintf(nb, sizeof nb, "%llu", (unsigned long long)c);\n'
        '        poolName.push_back(nb);\n'
        '    }')
assert old2 in s, "consts block not found"
s = s.replace(old2, new2)

s = s.replace('(%d inputs + %d consts)', '(%d in + %d const + %d shift)')
s = s.replace('K, nBase, (int)inNames.size(), (int)consts.size(), (int)goals.size(),',
              'K, nBase, (int)inNames.size(), (int)consts.size(), (int)shifts.size(), (int)goals.size(),')

s = s.replace('nodes=%llu  prunedEq=%llu  prunedCost=%llu  prunedTriv=%llu  time=%.2fs%s',
              'nodes=%llu  eq=%llu cost=%llu triv=%llu dead=%llu  time=%.2fs%s')
s = s.replace('(unsigned long long)prunedCost, (unsigned long long)prunedTriv, el,',
              '(unsigned long long)prunedCost, (unsigned long long)prunedTriv,\n'
              '           (unsigned long long)prunedDead, el,')

io.open(p, "w", encoding="utf-8").write(s)
print("patched ok")
