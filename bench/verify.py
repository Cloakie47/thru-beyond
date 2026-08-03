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

print("== 08: compress = 5,853 + size + proof; instr = proof + 3 (all points) ==")
for r in [r for r in data["08-compression"] if r["instr"] in ("compress", "recompress")]:
    check(f"compress {r['size']}", r["cu"] == 5853 + r["size"] + r["proof_bytes"],
          f"{r['cu']} != 5853+{r['size']}+{r['proof_bytes']}")
    if "instr_bytes" in r:
        check(f"compress {r['size']} instr=proof+3",
              r["instr_bytes"] == r["proof_bytes"] + 3, r["instr_bytes"])

print("== 08: decompress = 5,911 + revived + proof; instr = revived + proof + 43 ==")
for r in [r for r in data["08-compression"]
          if r["instr"] == "decompress" and "proof_bytes" in r]:
    check(f"decompress {r['size']}", r["cu"] == 5911 + r["size"] + r["proof_bytes"],
          f"{r['cu']} != 5911+{r['size']}+{r['proof_bytes']}")
    check(f"decompress {r['size']} instr",
          r["instr_bytes"] == r["size"] + r["proof_bytes"] + 43, r["instr_bytes"])

print("== 10: read_many slope 18 ==")
rm = {r["n"]: r["cu"] for r in data["10-blockcontext"] if r["instr"] == "read_many"}
for a, b in [(1, 10), (10, 100), (100, 511)]:
    check(f"10 read_many {a}-{b}", (rm[b] - rm[a]) / (b - a) == 18)

print("== 09: touch flat; read slope 44; write slope 590; write pages N+1 ==")
if "09-limits" in data:
    t9 = [r for r in data["09-limits"] if r["instr"] == "touch_none" and "cu" in r]
    check("09 touch_none flat 4947", all(r["cu"] == 4947 for r in t9))
    rd = {r["n"]: r["cu"] for r in data["09-limits"] if r["instr"] == "read_all"}
    for a, b in [(1, 2), (64, 256), (512, 1000)]:
        check(f"09 read {a}-{b}", (rd[b] - rd[a]) / (b - a) == 44)
    wr = {r["n"]: r for r in data["09-limits"] if r["instr"] == "write_all"}
    for a, b in [(1, 2), (64, 256), (512, 1000)]:
        check(f"09 write {a}-{b}", (wr[b]["cu"] - wr[a]["cu"]) / (b - a) == 590)
    for n, r in wr.items():
        if "pages" in r:
            check(f"09 write pages n={n}", r["pages"] == n + 1)

print("== 11: O-level runtime — -O3 best by <0.15%, -O1 second; sizes ==")
if "11-budgets" in data:
    ol = {r["olevel"]: r for r in data["11-budgets"] if "olevel" in r}
    if "runtime_cu_1024B" in ol.get("-O3", {}):
        check("O3 reproduces 224511", ol["-O3"]["runtime_cu_1024B"] == 224511)
        check("O1 within 0.15% of O3",
              abs(ol["-O1"]["runtime_cu_1024B"] - 224511) / 224511 < 0.0015)
        check("O0 worst runtime", ol["-O0"]["runtime_cu_1024B"] ==
              max(r["runtime_cu_1024B"] for r in ol.values()))
        check("O1 cheapest upgrade", ol["-O1"]["upgrade_cu"] ==
              min(r["upgrade_cu"] for r in ol.values() if "upgrade_cu" in r))

print("== 11: pipeline stage laws (exact) + 5-txn totals + determinism ==")
if "11-budgets" in data:
    st = [r for r in data["11-budgets"] if "stage_create" in r]
    for r in st:
        b = r["binary_bytes"]
        check(f"11 {r['olevel']} create = 11,723 + B",
              r["stage_create"] == 11723 + b, r["stage_create"])
        check(f"11 {r['olevel']} chunk = 34,514 + 4.75B",
              r["stage_chunk"] == 34514 + int(4.75 * b), r["stage_chunk"])
        check(f"11 {r['olevel']} cleanup flat", r["stage_cleanup"] == 35782)
        tot5 = sum(r[k] for k in ("stage_create", "stage_chunk", "stage_finalize",
                                  "stage_upgrade", "stage_cleanup"))
        check(f"11 {r['olevel']} 5-txn total", tot5 == r["pipeline_cu"], tot5)
        if "upgrade_cu" in r:
            check(f"11 {r['olevel']} 4-txn subtotal = recorded upgrade_cu",
                  tot5 - r["stage_chunk"] == r["upgrade_cu"], tot5 - r["stage_chunk"])
    byo = {r["olevel"]: r for r in st}
    if "-Os" in byo and "-Oz" in byo:
        same = all(byo["-Os"][k] == byo["-Oz"][k] for k in
                   ("stage_create", "stage_chunk", "stage_finalize", "stage_cleanup"))
        check("11 Os/Oz identical except upgrade step", same and
              byo["-Oz"]["stage_upgrade"] - byo["-Os"]["stage_upgrade"] == 1280)
    if "-O3" in byo and "-O3-rerun" in byo:
        same = all(byo["-O3"][k] == byo["-O3-rerun"][k] for k in
                   ("stage_create", "stage_chunk", "stage_finalize", "stage_cleanup"))
        check("11 O3 rerun identical except upgrade step (grow vs overwrite)", same and
              byo["-O3-rerun"]["stage_upgrade"] - byo["-O3"]["stage_upgrade"] == 1344)

print("== 11: 4-txn subtotal law ~= 137,149 + 171.1/byte (0.5% tolerance) ==")
if "11-budgets" in data:
    pts = [(r["binary_bytes"], r["upgrade_cu"]) for r in data["11-budgets"]
           if "upgrade_cu" in r]
    if pts:
        n = len(pts)
        sx = sum(x for x, _ in pts); sy = sum(y for _, y in pts)
        sxx = sum(x * x for x, _ in pts); sxy = sum(x * y for x, y in pts)
        slope = (n * sxy - sx * sy) / (n * sxx - sx * sx)
        icept = (sy - slope * sx) / n
        check("11 upgrade slope ~171.1", abs(slope - 171.134) < 0.5, round(slope, 3))
        check("11 upgrade intercept ~137,149", abs(icept - 137149) < 500, round(icept))
        for x, y in pts:
            pred = icept + slope * x
            check(f"11 upgrade {x}B within 0.5%", abs(pred - y) / y < 0.005,
                  f"pred {round(pred)} vs {y}")

print("== 02: pay_out (owner-side tsys_account_transfer) 3x identical ==")
po = [r for r in data["02-counter"] if r["instr"] == "pay_out"]
for r in po:
    check("02 pay_out 5,453", r["cu"] == 5453 and r["runs"] == 3, r["cu"])

print("== 05 v4: inc_seg page term = 4,096/page over re-baseline ==")
v4 = {r["instr"]: r for r in data["05-events"] if r.get("binary") == "v4"}
if "inc_seg_4096" in v4:
    d1 = v4["inc_seg_4096"]["cu"] - v4["return_only"]["cu"]
    d4 = v4["inc_seg_16384"]["cu"] - v4["return_only"]["cu"]
    check("05 inc_seg +4 pages - +1 page = 3x4096",
          v4["inc_seg_16384"]["cu"] - v4["inc_seg_4096"]["cu"] == 3 * 4096,
          v4["inc_seg_16384"]["cu"] - v4["inc_seg_4096"]["cu"])
    check("05 inc_seg pages 9/12", v4["inc_seg_4096"]["pages"] == 9
          and v4["inc_seg_16384"]["pages"] == 12)

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
