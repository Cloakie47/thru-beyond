#!/usr/bin/env python3
"""Recompute every published law from examples/*/results.json and report
figures that disagree with their source. Run: python3 bench/verify.py"""
import json, math, glob, os, sys

ROOT = os.path.join(os.path.dirname(__file__), "..", "examples")
data = {}
for p in glob.glob(os.path.join(ROOT, "*", "results.json")):
    d = json.load(open(p, encoding="utf-8"))
    data[d["example"]] = d["rows"]

fails = []
def check(name, cond, detail=""):
    if not cond:
        fails.append(f"{name}: {detail}")
        print(f"  FAIL {name} {detail}")
    else:
        print(f"  ok   {name}")

print("== 01: noop floor 4,763 = 512 + 4,096 + 155 ==")
noop = [r for r in data["01-noop"] if r["instr"] == "noop"][0]
check("noop", noop["cu"] == 512 + 4096 + 155, noop["cu"])

print("== 02: create = base + 1 CU/proof byte (pairwise deltas) ==")
creates = [r for r in data["02-counter"] if r["instr"] == "create"]
base = creates[0]["cu"] - creates[0]["proof_bytes"]
for r in creates:
    check(f"create proof={r['proof_bytes']}", r["cu"] - r["proof_bytes"] == base,
          f"{r['cu']}-{r['proof_bytes']} != {base}")

print("== 02: CoW = bytes present (increments) ==")
inc = {r["instr"]: r["cu"] for r in data["02-counter"] if r["instr"].startswith("increment")}
check("inc100 = inc8 + 92", inc["increment_100B"] == inc["increment_8B"] + 92)
check("inc5000 = inc8 + 4088", inc["increment_5000B"] == inc["increment_8B"] + 4088)

print("== 02: trailing-page CoW (write_at) ==")
wa = {r.get("offset"): r["cu"] for r in data["02-counter"] if r["instr"] == "write_at"}
check("write_at(4096) = W0 - 3192", wa[4096] == wa[0] - 3192)
check("write_at(4999) = write_at(4096)", wa[4999] == wa[4096])

print("== 02: resize CU = baseline + bytes grown; SU = ceil(grown/4096) ==")
for r in [r for r in data["02-counter"] if r["instr"] == "resize"]:
    grown = r["to"] - r["from"]
    check(f"resize {r['from']}->{r['to']} CU ({r['binary']})",
          r["cu"] == r["baseline"] + grown, f"{r['cu']} != {r['baseline']}+{grown}")
    check(f"resize {r['from']}->{r['to']} SU", r["su"] == math.ceil(grown / 4096))

print("== 04: slopes 12,846 / 9,884 from big points; intercepts 6,129 ==")
for arm, slope in [("A", 12846), ("B", 9884)]:
    rows = {r["input"]: r for r in data["04-hash"] if r.get("arm") == arm}
    for a, b in [(256, 1024), (1024, 4096), (256, 4096)]:
        s = (rows[b]["cu"] - rows[a]["cu"]) / (rows[b]["blocks"] - rows[a]["blocks"])
        check(f"04{arm} slope {a}-{b}", s == slope, s)
    for L in (256, 1024, 4096):
        check(f"04{arm} intercept @{L}", rows[L]["cu"] - slope * rows[L]["blocks"] == 6129)

print("== 05 v1: emit - fill = 570 + N at all 13 points ==")
v1 = [r for r in data["05-events"] if r.get("binary") == "v1_534B"]
em = {r["n"]: r["cu"] for r in v1 if r["instr"] == "emit_n"}
fi = {r["n"]: r["cu"] for r in v1 if r["instr"] == "fill_only"}
for n in em:
    check(f"emit-fill n={n}", em[n] - fi[n] == 570 + n, em[n] - fi[n])

print("== 06: slopes 8/16/11/11; log8 = log0 + 8 ==")
for instr, slope in [("spin", 8), ("spin_wide", 16), ("spin_load", 11), ("spin_store", 11)]:
    rows = {r["n"]: r["cu"] for r in data["06-instructions"] if r["instr"] == instr}
    for a, b in [(0, 1), (1000, 10000)]:
        check(f"06 {instr} {a}-{b}", (rows[b] - rows[a]) / (b - a) == slope)
l = {r["instr"]: r["cu"] for r in data["06-instructions"] if r["instr"].startswith("log")}
check("log8 = log0 + 8", l["log8"] == l["log0"] + 8)

print("== 07: cpi_n slope 1,511; deep slope 5,607; Pages = N + 2 ==")
cn = {r["n"]: r["cu"] for r in data["07-cpi"] if r["instr"] == "cpi_n"}
for a, b in [(1, 2), (2, 4), (4, 8)]:
    check(f"07 cpi_n {a}-{b}", (cn[b] - cn[a]) / (b - a) == 1511)
dp = {r["n"]: r for r in data["07-cpi"] if r["instr"] == "cpi_deep"}
for a, b in [(1, 2), (2, 4), (4, 8), (8, 13), (13, 14)]:
    check(f"07 deep {a}-{b}", (dp[b]["cu"] - dp[a]["cu"]) / (b - a) == 5607)
for n, r in dp.items():
    check(f"07 deep pages n={n}", r["pages"] == n + 2)

print("== 08: compress = 5,853 + size + proof (all points) ==")
for r in [r for r in data["08-compression"] if r["instr"] in ("compress", "recompress")]:
    check(f"compress {r['size']}", r["cu"] == 5853 + r["size"] + r["proof_bytes"],
          f"{r['cu']} != 5853+{r['size']}+{r['proof_bytes']}")

print("== 10: read_many slope 18 ==")
rm = {r["n"]: r["cu"] for r in data["10-blockcontext"] if r["instr"] == "read_many"}
for a, b in [(1, 10), (10, 100), (100, 511)]:
    check(f"10 read_many {a}-{b}", (rm[b] - rm[a]) / (b - a) == 18)

print("== MU == Pages wherever both recorded ==")
for ex, rows in data.items():
    for r in rows:
        if "mu" in r and "pages" in r:
            check(f"{ex} {r.get('instr','?')} MU==Pages", r["mu"] == r["pages"],
                  f"mu={r['mu']} pages={r['pages']}")

print()
print(f"DISCREPANCIES: {len(fails)}")
for f in fails:
    print(" -", f)
sys.exit(1 if fails else 0)
