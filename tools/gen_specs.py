#!/usr/bin/env python3
"""Generate specs/NNN-*/spec.md from the feature decomposition and the WDDM DDI
table. Regenerating never overwrites hand-edited prose: everything between
AUTOGEN markers is replaced, everything else is preserved."""
import os, re, sys, datetime, textwrap

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from features import FEATURES

TABLE = os.path.join(ROOT, "docs/ref/driver_init_table.md")
BEGIN, END = "<!-- AUTOGEN:BEGIN -->", "<!-- AUTOGEN:END -->"

LEVEL_TITLES = {
    "BASE": "WDDM 1.0 (Windows Vista)", "WIN7": "WDDM 1.1 (Windows 7)",
    "WIN8": "WDDM 1.2 (Windows 8)", "WDDM1_3": "WDDM 1.3 (Windows 8.1)",
    "WDDM2_0": "WDDM 2.0 (Windows 10 1507)", "WDDM2_1": "WDDM 2.1 (1607)",
    "WDDM2_2": "WDDM 2.2 (1703)", "WDDM2_3": "WDDM 2.3 (1709)",
    "WDDM2_4": "WDDM 2.4 (1803)", "WDDM2_5": "WDDM 2.5 (1809)",
    "WDDM2_6": "WDDM 2.6 (1903)", "WDDM2_7": "WDDM 2.7 (2004)",
    "WDDM2_8": "WDDM 2.8 (20H2)", "WDDM2_9": "WDDM 2.9 (21H2)",
    "WDDM3_0": "WDDM 3.0 (22H2)", "WDDM3_1": "WDDM 3.1 (23H2)",
    "WDDM3_2": "WDDM 3.2 (24H2)",
}


def load_table():
    rows = []
    for ln in open(TABLE):
        m = re.match(r"\| *(\d+) \| `([^`]+)` \| `([^`]+)` \| (\S+) \|", ln)
        if m:
            rows.append((int(m.group(1)), m.group(2), m.group(3), m.group(4)))
    return rows


def slots_for(feat, rows):
    _id, _short, level, _status, _title, _summary, extra = feat
    extra = extra or {}
    if extra.get("slots"):
        want = set(extra["slots"])
        return [r for r in rows if r[1] in want]
    levels = set(extra.get("merge", []))
    if level and level != "BASE+" and not levels:
        levels = {level}
    if not levels:
        return []
    return [r for r in rows if r[3] in levels]


def body(feat, rows):
    fid, short, level, status, title, summary, extra = feat
    extra = extra or {}
    sl = slots_for(feat, rows)
    out = []
    a = out.append

    a(f"**Feature ID**: {fid} · **Status**: `{status}` · "
      f"**Depends on**: {', '.join(extra.get('deps', [])) or 'nothing'}")
    if level:
        a(f"· **WDDM level**: {LEVEL_TITLES.get(level, level)}")
    a("")
    if summary:
        a("\n".join(textwrap.wrap(summary, 78)))
        a("")

    a("### Status meaning")
    a("")
    a({
        "implement": "Build it, test it to the constitution's standard, and report "
                     "the capabilities it enables.",
        "partial":   "Implement only the subset listed below. Everything else in "
                     "this level is stubbed or skipped, and the stub is tested.",
        "stub":      "Provide the DDI so `dxgkrnl` finds a pointer, return the "
                     "correct not-supported status, and prove the decline with a test.",
        "skip":      "Leave the slot NULL. `dxgkrnl` handles absence. A test asserts "
                     "the slot is NULL so nobody fills it in by accident.",
    }[status])
    a("")

    if sl:
        a(f"### DDI slots in scope ({len(sl)})")
        a("")
        a("| Slot | Field | Introduced |")
        a("|--:|---|---|")
        for n, f, _t, v in sl:
            a(f"| {n} | `{f}` | {v} |")
        a("")
        a("Slot numbers are indices into `DRIVER_INITIALIZATION_DATA` as laid out "
          "by the WDDM 3.2 headers; see `docs/ref/driver_init_table.md`. They are "
          "the fastest way to navigate an unsymbolized vendor driver.")
        a("")

    if extra.get("note"):
        a("### Note")
        a("")
        a("\n".join(textwrap.wrap(extra["note"], 78)))
        a("")

    a("### Test requirements")
    a("")
    a("Constitution Principle III applies in full. Concretely, this feature is not "
      "done until:")
    a("")
    a("- every branch of every function it adds to `src/core`, `src/device` or "
      "`src/proto` is covered, or marked `AV_UNTESTABLE` with a justification;")
    a("- each stateful module it adds exports an invariant function that tests "
      "call after **every** mutation;")
    a("- every function it adds that reads bytes from outside its own address "
      "space has a hostile-input table test and a libFuzzer target with a "
      "committed corpus;")
    a("- the allocation-failure sweep passes: with each allocation site failed in "
      "turn, no crash and no leak;")
    a("- IRQL annotations are present and the mock's IRQL checker is satisfied;")
    if sl:
        a("- the DDI conformance harness (feature 003) drives every slot above, "
          "including the abusive-but-legal sequences for it;")
    a("- `tools/check_symbol_tests.py` reports no untested exported symbol.")
    a("")

    refs = extra.get("kb", [])
    if refs:
        a("### Reference material")
        a("")
        a("Knowledge base at `../claude-scratch/wddm-kb/kb/`:")
        a("")
        for r in refs:
            a(f"- `{r}`")
        a("")
        a("Normative headers: `../claude-scratch/wddm-kb/headers/wdk-26100` and "
          "`headers/sdk`. Reverse-engineering targets, symbolized, in "
          "`../claude-scratch/wddm-kb/ghidra-proj` — read `dxgkrnl.sys` when the "
          "question is \"what does the caller do with this\", and "
          "`basicrender.sys` when it is \"what is the minimum that works\".")
        a("")
    return "\n".join(out)


def main():
    rows = load_table()
    made = updated = 0
    for feat in FEATURES:
        fid, short = feat[0], feat[1]
        title = feat[4]
        d = os.path.join(ROOT, "specs", f"{fid}-{short}")
        os.makedirs(d, exist_ok=True)
        p = os.path.join(d, "spec.md")
        block = f"{BEGIN}\n\n{body(feat, rows)}\n{END}"
        if os.path.exists(p):
            s = open(p).read()
            new = re.sub(re.escape(BEGIN) + r".*?" + re.escape(END), block, s,
                         flags=re.S)
            if new != s:
                open(p, "w").write(new); updated += 1
        else:
            open(p, "w").write(
                f"# Feature Specification: {title}\n\n"
                f"**Feature Branch**: `{fid}-{short}`\n\n"
                f"**Created**: {datetime.date.today()}\n\n"
                f"**Status**: Draft\n\n"
                f"{block}\n\n"
                "## User Scenarios & Testing *(mandatory)*\n\n"
                "<!-- Written by /speckit-specify or by hand. The autogen block "
                "above is regenerated by tools/gen_specs.py; everything below is "
                "preserved. -->\n\n"
                "### Acceptance Scenarios\n\n"
                "1. **Given** [state], **When** [action], **Then** [outcome]\n\n"
                "## Requirements *(mandatory)*\n\n"
                "## Out of Scope\n\n")
            made += 1
    print(f"{made} created, {updated} updated, {len(FEATURES)} total")


if __name__ == "__main__":
    main()
