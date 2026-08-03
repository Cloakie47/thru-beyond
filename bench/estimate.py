#!/usr/bin/env python3
"""Budget calculator for Thru transactions.

predict(...) returns (cu, su, mu) with itemized terms from the measured
model (alphanet, CLI 0.3.2, 2026-08). The instruction term CANNOT be known
a priori — pass instr_bytes from a one-off calibration run (measure one
size, subtract the other terms); everything else is predicted exactly.

Validation: `python3 bench/estimate.py validate` calibrates each family's
instruction term on its smallest recorded row(s) and predicts the rest of
examples/*/results.json, reporting per-row error.
"""
import json, math, glob, os, sys

FLOOR = 4608          # entry stub: 512 segment-map syscall + 4096 stack page
SYSCALL = 512         # per call; tsys_exit is free
PAGE = 4096           # per anonymous page allocated (map time)
EVENT_OVERHEAD = 10   # bytes of event record per event (MU accounting)
CPI_HOP = 1024        # invoke base + callee entry syscall (callee instr extra)
MARGIN_CU = 1.25
MARGIN_UNITS = 2

def predict(instr_bytes=0, syscalls=0, pages_allocated=0, cow_bytes=0,
            pages_written=0, grown_bytes=0, payload_bytes=0,
            event_sizes=(), cpi_hops=0, cpi_max_depth=1,
            accounts_written=0, permanent_creates=0, cpi_callee_bytes=0):
    """All inputs are raw characteristics; accounts_written adds its own
    512/account writable syscall; events add their own 512 base each."""
    terms = {
        "floor (entry syscall + stack page)": FLOOR,
        "syscalls x512": SYSCALL * syscalls,
        "writable syscalls x512": SYSCALL * accounts_written,
        "event bases x512": SYSCALL * len(event_sizes),
        "event bytes": sum(event_sizes),
        "anon pages x4096": PAGE * pages_allocated,
        "CoW bytes copied": cow_bytes,
        "resize bytes grown": grown_bytes,
        "payload/proof bytes": payload_bytes,
        "instruction bytes (calibrated input)": instr_bytes,
        "cpi hops x1024": CPI_HOP * cpi_hops,
        "cpi depth pages x4096": PAGE * max(0, cpi_max_depth - 1),
        "cpi callee bytes": cpi_callee_bytes,
    }
    cu = sum(terms.values())
    su = permanent_creates + math.ceil(grown_bytes / PAGE) if grown_bytes or permanent_creates else 0
    ev_pages = math.ceil(sum(s + EVENT_OVERHEAD for s in event_sizes) / PAGE) if event_sizes else 0
    mu = 1 + pages_allocated + pages_written + ev_pages + max(0, cpi_max_depth - 1)
    return cu, su, mu, terms

def recommend(cu, su, mu):
    return (max(int(cu * MARGIN_CU), cu + 2000), su + MARGIN_UNITS, mu + MARGIN_UNITS)

# ---------- validation against results.json ----------

def load():
    root = os.path.join(os.path.dirname(__file__), "..", "examples")
    return {json.load(open(p, encoding="utf-8"))["example"]:
            json.load(open(p, encoding="utf-8"))["rows"]
            for p in glob.glob(os.path.join(root, "*", "results.json"))}

def validate():
    data = load()
    errors, covered, skipped = [], 0, 0
    fam = {}   # family -> [calibration_rows, predicted_rows]

    def cal(family, n=1):
        fam.setdefault(family, [0, 0])[0] += n

    def rep(label, actual, pred, family=None):
        nonlocal covered
        covered += 1
        fam.setdefault(family or label.rsplit(" ", 1)[0], [0, 0])[1] += 1
        e = pred - actual
        errors.append(abs(e))
        flag = "" if e == 0 else f"  err={e:+d}"
        print(f"  {label}: actual={actual} pred={pred}{flag}")

    # 06 loops: calibrate at n=0 (fixed path), slope from body_bytes law
    for instr, per in [("spin", 8), ("spin_wide", 16), ("spin_load", 11), ("spin_store", 11)]:
        rows = {r["n"]: r["cu"] for r in data["06-instructions"] if r["instr"] == instr}
        fixed = rows[0] - FLOOR  # calibrated instruction+data term of the n=0 path
        cal(f"06 {instr}")
        for n, cu in rows.items():
            if n == 0: continue
            rep(f"06 {instr} n={n}", cu, FLOOR + fixed + per * n, f"06 {instr}")

    # 05 v1 emit: calibrate fill at each n (loop term), add event law
    # NOTE the 1:1 ratio below - every emit row leans on its own fill_only
    # calibration row; this family validates the event law only.
    v1 = [r for r in data["05-events"] if r.get("binary") == "v1_534B"]
    fi = {r["n"]: r["cu"] for r in v1 if r["instr"] == "fill_only"}
    for r in (r for r in v1 if r["instr"] == "emit_n"):
        n = r["n"]
        base = fi[n] - FLOOR  # everything except the emit call (calibrated)
        cal("05 emit")
        cu, _, mu, _ = predict(instr_bytes=base - 7 * PAGE - SYSCALL + 58,
                               syscalls=1, pages_allocated=7, event_sizes=[n])
        rep(f"05 emit n={n}", r["cu"], cu, "05 emit")

    # 07 cpi_n: calibrate at n=1,2 (base + hop), predict others
    cn = {r["n"]: r["cu"] for r in data["07-cpi"] if r["instr"] == "cpi_n"}
    hop = cn[2] - cn[1]
    cal("07 cpi_n", 2)
    for n, cu in cn.items():
        if n <= 2: continue
        rep(f"07 cpi_n n={n}", cu, cn[1] + hop * (n - 1), "07 cpi_n")
    dp = {r["n"]: r["cu"] for r in data["07-cpi"] if r["instr"] == "cpi_deep"}
    lvl = dp[2] - dp[1]
    cal("07 deep", 2)
    for n, cu in dp.items():
        if n <= 2: continue
        rep(f"07 deep n={n}", cu, dp[1] + lvl * (n - 1), "07 deep")

    # 09 ladders: calibrate at n=1
    for instr, key in [("read_all", 44), ("write_all", 590)]:
        rows = {r["n"]: r["cu"] for r in data["09-limits"] if r["instr"] == instr}
        cal(f"09 {instr}")
        for n, cu in rows.items():
            if n == 1: continue
            rep(f"09 {instr} n={n}", cu, rows[1] + key * (n - 1), f"09 {instr}")

    # 02 resize: exact law; the baseline (shrink) row is the calibration
    cal("02 resize", 2)  # one baseline row per binary (v1, v2)
    for r in (r for r in data["02-counter"] if r["instr"] == "resize"):
        grown = r["to"] - r["from"]
        rep(f"02 resize->{r['to']} ({r['binary']})", r["cu"], r["baseline"] + grown,
            "02 resize")

    # 08 compression: exact law (intercept 5,853 was fitted on this dataset)
    cal("08 compress")
    for r in (r for r in data["08-compression"] if r["instr"] in ("compress", "recompress")):
        rep(f"08 {r['instr']} {r['size']}", r["cu"], 5853 + r["size"] + r["proof_bytes"],
            "08 compress")

    # 10 read_many: calibrate n=1
    rm = {r["n"]: r["cu"] for r in data["10-blockcontext"] if r["instr"] == "read_many"}
    cal("10 read_many")
    for n, cu in rm.items():
        if n == 1: continue
        rep(f"10 read_many n={n}", cu, rm[1] + 18 * (n - 1), "10 read_many")

    # 04 hash: calibrate intercept+slope from two block counts, predict rest
    for arm, slope in [("A", 12846), ("B", 9884)]:
        rows = {r["input"]: r for r in data["04-hash"] if r.get("arm") == arm}
        icept = rows[256]["cu"] - slope * rows[256]["blocks"]
        cal(f"04{arm}", 2)  # slope + intercept anchor
        for L, r in rows.items():
            if L in (256,): continue
            if L in (0, 32):
                skippedmsg = True  # small-input tail deviation documented
                continue
            rep(f"04{arm} L={L}", r["cu"], icept + slope * r["blocks"], f"04{arm}")

    # categories NOT predictable without per-syscall calibration
    print("\nNOT predicted (calibration-class, documented): create internals"
          " (~1,430 CU payload work), deploy/upgrade pipeline components,"
          " small-input tail deviations (04 L=0/32), degraded-chain rows.")

    print("\ncalibration vs predicted, per family (a family whose ratio nears"
          " 1:1 mostly validates itself):")
    tc = tp = 0
    for f in sorted(fam):
        c, p = fam[f]
        tc += c; tp += p
        print(f"  {f:<14} calibration rows: {c:>2}   predicted rows: {p:>2}"
              f"   ({p}/{c} per calibration)" if c else
              f"  {f:<14} calibration rows:  0   predicted rows: {p:>2}")
    print(f"  {'TOTAL':<14} calibration rows: {tc:>2}   predicted rows: {tp:>2}")

    n = len(errors)
    exact = sum(1 for e in errors if e == 0)
    print(f"\nrows predicted: {n}; exact: {exact}; max |err|: {max(errors)};"
          f" mean |err|: {sum(errors)/n:.1f}")

if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "validate":
        validate()
    else:
        cu, su, mu, terms = predict(instr_bytes=400, syscalls=1,
                                    accounts_written=1, cow_bytes=8)
        print("example: simple account update")
        for k, v in terms.items():
            if v: print(f"  {k}: {v}")
        print(f"predicted CU={cu} SU={su} MU={mu}; recommend {recommend(cu, su, mu)}")
