        const char *sa = iCursor;
        size_t dla;
        size_t nega = size_t(*sa == '-');   // OPT-A6 branchless
        int64_t va = parseSIMD(sa + nega, dla);
        va = nega ? -va : va;
        iCursor = sa + nega + dla;
        if (iCursor < iEnd && *iCursor < 0x21) iCursor++;  // skip space

        const char *sb = iCursor;
        size_t dlb;
        size_t negb = size_t(*sb == '-');   // OPT-A6 branchless
        int64_t vb = parseSIMD(sb + negb, dlb);
        vb = negb ? -vb : vb;
        iCursor = sb + negb + dlb;
        if (iCursor < iEnd && *iCursor < 0x21) iCursor++;  // skip newline
#ifdef PROFILE_DIV
        auto _p1 = std::chrono::high_resolution_clock::now();
        t_read += std::chrono::duration<double, std::milli>(_p1 - _p0).count();
#endif
        // OPT-A1: 判定只看数字位数, 符号已折进 va/vb。
        // OPT-A2: 快路径调用 writeI64NL, 它把 '-' + 数字 + '\n' 一次性写出。
        if (dla > 0 && dla <= 18 && dlb > 0 && dlb <= 18) {
#ifdef PROFILE_DIV
            auto _p2 = std::chrono::high_resolution_clock::now();
            t_parse += std::chrono::duration<double, std::milli>(_p2 - _p1).count();
#endif
            writeI64NL(va + vb);
#ifdef PROFILE_DIV
            _p1 = std::chrono::high_resolution_clock::now();
            t_write += std::chrono::duration<double, std::milli>(_p1 - _p2).count();
#endif
        } else {
            a.fromCharRange(sa, sa + nega + dla);
            b.fromCharRange(sb, sb + negb + dlb);
            a += b;
            writeHint(a);
            *oCursor++ = '\n';
#ifdef PROFILE_DIV
            _p1 = std::chrono::high_resolution_clock::now();
            t_write += std::chrono::duration<double, std::milli>(_p1 - _p0).count();
            t_parse += std::chrono::duration<double, std::milli>(_p1 - _p0).count();
#endif
        }
