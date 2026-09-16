#!/usr/bin/env python3
"""Fetch Windows PE binaries + matching PDBs from Microsoft's public symbol
server, using winbindex to resolve (filename, windows-version) -> PE key.

No Windows install required.

  msfetch.py list   dxgkrnl.sys --winver 11-24H2
  msfetch.py get    dxgkrnl.sys --winver 11-24H2 --outdir re/bin --pdbdir re/bin
  msfetch.py get    dxgkrnl.sys --sha256 ab420f66... --outdir re/bin --pdbdir re/bin
"""
import argparse, gzip, hashlib, io, json, os, re, struct, sys, urllib.request

WINBINDEX = "https://winbindex.m417z.com/data/by_filename_compressed/{}.json.gz"
MSDL = "https://msdl.microsoft.com/download/symbols/{name}/{key}/{name}"
UA = {"User-Agent": "Microsoft-Symbol-Server/10.0.10036.206"}
CACHE = os.path.expanduser("~/.cache/winbindex")


def http(url, binary=True):
    req = urllib.request.Request(url, headers=UA)
    with urllib.request.urlopen(req, timeout=120) as r:
        return r.read()


def index(filename):
    os.makedirs(CACHE, exist_ok=True)
    p = os.path.join(CACHE, filename + ".json.gz")
    if not os.path.exists(p):
        open(p, "wb").write(http(WINBINDEX.format(filename)))
    return json.load(gzip.open(p))


def verkey(v):
    """sortable numeric tuple from '10.0.26100.4652'"""
    m = re.match(r"(\d+)\.(\d+)\.(\d+)\.(\d+)", v or "")
    return tuple(int(x) for x in m.groups()) if m else (0, 0, 0, 0)


def candidates(filename, winver=None, arch=34404):
    out = []
    for sha, e in index(filename).items():
        fi = e.get("fileInfo") or {}
        if "timestamp" not in fi or "virtualSize" not in fi:
            continue
        if arch and fi.get("machineType") != arch:
            continue
        wvs = list((e.get("windowsVersions") or {}).keys())
        if winver and winver not in wvs:
            continue
        out.append({
            "version": fi.get("version", "?"),
            "sha256": sha,
            "size": fi.get("size"),
            "winvers": wvs,
            "key": f"{fi['timestamp']:08X}{fi['virtualSize']:x}",
        })
    out.sort(key=lambda c: verkey(c["version"]))
    return out


# ---- PE parsing: find the RSDS CodeView record for the PDB key -------------
def pdb_key(data):
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    assert data[pe:pe + 4] == b"PE\0\0", "not a PE"
    nsec, = struct.unpack_from("<H", data, pe + 6)
    optoff = pe + 24
    magic, = struct.unpack_from("<H", data, optoff)
    ddoff = optoff + (112 if magic == 0x20B else 96)
    dbg_rva, dbg_sz = struct.unpack_from("<II", data, ddoff + 6 * 8)
    if not dbg_rva:
        return None
    secs = []
    so = optoff + struct.unpack_from("<H", data, pe + 20)[0]
    for i in range(nsec):
        s = so + i * 40
        va, vs, rawsz, rawptr = struct.unpack_from("<IIII", data, s + 12 - 4)
        va, vs = struct.unpack_from("<II", data, s + 12)[::-1][::-1]
        vs, va = struct.unpack_from("<II", data, s + 8)
        rawsz, rawptr = struct.unpack_from("<II", data, s + 16)
        secs.append((va, vs, rawsz, rawptr))

    def r2o(rva):
        for va, vs, rawsz, rawptr in secs:
            if va <= rva < va + max(vs, rawsz):
                return rawptr + (rva - va)
        return None

    off = r2o(dbg_rva)
    for i in range(dbg_sz // 28):
        e = off + i * 28
        typ, = struct.unpack_from("<I", data, e + 12)
        sz, _rva, ptr = struct.unpack_from("<III", data, e + 16)
        if typ != 2:      # IMAGE_DEBUG_TYPE_CODEVIEW
            continue
        cv = data[ptr:ptr + sz]
        if cv[:4] != b"RSDS":
            continue
        g = cv[4:20]
        d1, d2, d3 = struct.unpack_from("<IHH", g, 0)
        guid = "%08X%04X%04X%s" % (d1, d2, d3, g[8:16].hex().upper())
        age, = struct.unpack_from("<I", cv, 20)
        name = cv[24:cv.index(b"\0", 24)].decode()
        return f"{guid}{age:X}", os.path.basename(name.replace("\\", "/"))
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cmd", choices=["list", "get"])
    ap.add_argument("files", nargs="+")
    ap.add_argument("--winver", default=None)
    ap.add_argument("--pick", default="latest", help="latest|oldest|<version substring>")
    ap.add_argument("--sha256", default=None,
                    help="select exactly this build; the download must hash to it")
    ap.add_argument("--outdir", default=".")
    ap.add_argument("--pdbdir", default=None)
    a = ap.parse_args()

    for fn in a.files:
        try:
            cands = candidates(fn, a.winver)
        except Exception as e:
            print(f"!! {fn}: index failed: {e}")
            continue
        if not cands:
            print(f"!! {fn}: no candidates (winver={a.winver})")
            continue
        if a.cmd == "list":
            print(f"== {fn}  ({len(cands)} candidates)")
            for c in cands[-12:]:
                print(f"   {c['version']:<34} {c['size']:>10}  {c['key']}  {','.join(c['winvers'])}")
            continue

        if a.sha256:
            c = next((x for x in cands if x["sha256"] == a.sha256.lower()), None)
            if c is None:
                print(f"!! {fn}: no build with sha256 {a.sha256}")
                sys.exit(1)
        elif a.pick == "latest":
            c = cands[-1]
        elif a.pick == "oldest":
            c = cands[0]
        else:
            c = next(x for x in cands if a.pick in x["version"])

        os.makedirs(a.outdir, exist_ok=True)
        dst = os.path.join(a.outdir, fn)
        url = MSDL.format(name=fn, key=c["key"])
        try:
            data = http(url)
        except Exception as e:
            print(f"!! {fn} {c['version']}: PE download failed {e}")
            continue
        if data[:2] != b"MZ":
            print(f"!! {fn}: msdl returned non-PE ({len(data)}B)")
            continue
        got = hashlib.sha256(data).hexdigest()
        if a.sha256 and got != a.sha256.lower():
            print(f"!! {fn}: sha256 {got}, want {a.sha256}")
            sys.exit(1)
        open(dst, "wb").write(data)
        print(f"OK {fn:<22} {c['version']:<30} {len(data):>9}B -> {dst}")

        if a.pdbdir:
            k = pdb_key(data)
            if not k:
                print(f"   (no codeview record)")
                continue
            key, pdbname = k
            os.makedirs(a.pdbdir, exist_ok=True)
            purl = MSDL.format(name=pdbname, key=key)
            try:
                pd = http(purl)
            except Exception as e:
                print(f"   !! pdb {pdbname} {key}: {e}")
                continue
            pp = os.path.join(a.pdbdir, pdbname)
            open(pp, "wb").write(pd)
            print(f"   pdb {pdbname:<20} {key} {len(pd):>9}B -> {pp}")


if __name__ == "__main__":
    main()
