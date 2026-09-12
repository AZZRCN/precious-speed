        const char *sa = iCursor;
        size_t nega = size_t(*sa == '-');
        size_t dla = quickLenProbe(sa + nega);
        int64_t va = 0;
        bool a_small = (dla > 0 && dla <= 18);
        if (a_small) {
            va = parseSIMD(sa + nega, dla);
            va = nega ? -va : va;
            iCursor = sa + nega + dla;
        } else {
            iCursor = a.fromCharRangeSingle(sa, iEnd);
        }
        if (iCursor < iEnd && *iCursor < 0x21) iCursor++;

        const char *sb = iCursor;
        size_t negb = size_t(*sb == '-');
        size_t dlb = quickLenProbe(sb + negb);
        int64_t vb = 0;
        bool b_small = (dlb > 0 && dlb <= 18);
        if (b_small) {
            vb = parseSIMD(sb + negb, dlb);
            vb = negb ? -vb : vb;
            iCursor = sb + negb + dlb;
        } else {
            iCursor = b.fromCharRangeSingle(sb, iEnd);
        }
        if (iCursor < iEnd && *iCursor < 0x21) iCursor++;

        if (a_small && b_small) {
            writeI64NL(va + vb);
        } else {
            if (a_small) a.fromCharRangeSingle(sa, iEnd);
            if (b_small) b.fromCharRangeSingle(sb, iEnd);
            a += b;
            writeHint(a);
            *oCursor++ = '\n';
        }
