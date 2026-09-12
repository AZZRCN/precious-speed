
import subprocess, glob, os
modes = {"mul": "/tmp/cand_mul", "div": "/tmp/cand_div", "add": "/tmp/cand_add"}
data = os.path.expanduser("~/hexbench/data")
for mode, binp in modes.items():
    indir = os.path.join(data, mode)
    cases = sorted(glob.glob(indir + "/*.in"))
    ok = bad = 0
    for f in cases:
        base = os.path.basename(f)[:-3]
        expf = os.path.join(indir, base + ".exp")
        explines = [l.strip() for l in open(expf).read().split("\n") if l.strip()]
        data_in = open(f).read()
        got = subprocess.run([binp], input=data_in, capture_output=True, text=True).stdout
        gotlines = [l.strip() for l in got.split("\n") if l.strip()]
        if gotlines == explines:
            ok += 1
        else:
            bad += 1
            print("BAD", mode, base, "expN", len(explines), "gotN", len(gotlines))
            for i in range(min(len(explines), len(gotlines))):
                if explines[i] != gotlines[i]:
                    print("  diff@%d exp=%s got=%s" % (i, explines[i][:40], gotlines[i][:40]))
                    break
    print("RESULT %s ok=%d bad=%d total=%d" % (mode, ok, bad, len(cases)))
