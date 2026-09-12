import sys
fn = sys.argv[1]
events = None
summary = None
for line in open(fn):
    if line.startswith('events:'):
        events = line.split()[1:]
    elif line.startswith('summary:'):
        summary = [int(x) for x in line.split()[1:]]
if events and summary:
    d = dict(zip(events, summary))
    def g(k): return d.get(k, 0)
    print(f"{fn}: Ir={g('Ir'):,}  D1mr={g('D1mr'):,}  DLmr={g('DLmr'):,}  I1mr={g('I1mr'):,}  ILmr={g('ILmr'):,}")
else:
    print(f"{fn}: parse failed (events={events}, summary={'set' if summary else 'none'})")
