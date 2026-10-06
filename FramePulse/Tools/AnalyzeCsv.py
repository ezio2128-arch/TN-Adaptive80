"""Offline optional developer utility; no Python is shipped or required by FramePulse."""
import argparse, csv, math, statistics, json
parser=argparse.ArgumentParser()
parser.add_argument('csv');args=parser.parse_args()
values=[]
with open(args.csv,encoding='utf-8-sig',newline='') as f:
    for row in csv.DictReader(f):
        try:
            n=float(row.get('MsBetweenPresents',row.get('median_ms','')))
            if math.isfinite(n) and n>0: values.append(n)
        except (ValueError,TypeError): pass
if not values: raise SystemExit('No positive frame times found')
values.sort();n=max(1,math.ceil(len(values)*.01))
def percentile(p):
    index=(len(values)-1)*p;lo=math.floor(index);hi=math.ceil(index)
    return values[lo]+(values[hi]-values[lo])*(index-lo)
print(json.dumps({'samples':len(values),'fps':1000/statistics.mean(values),
    'low1':1000/statistics.mean(values[-n:]),'median_ms':statistics.median(values),
    'p95_ms':percentile(.95),'p99_ms':percentile(.99)},indent=2))
