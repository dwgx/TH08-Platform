"""Batch 3: wider net.
- Batch 1 failures that codex partial-matched (AddLives, AddToBombCount, GetBombsRemaining, CollectExtend, InitRankParams, AddToClockTime, AddToYoukaiGauge)
- Player class small-to-medium functions (P0 for DLL)
- EnemyManager / BulletManager small functions (state capture)
- Remaining Supervisor
- Target: 40-50 functions total
"""
import csv
import json
import sys
import urllib.request
import uuid

URL = "http://127.0.0.1:13337/mcp"
SESSION_ID = None


def rpc(method, params=None, req_id=1):
    global SESSION_ID
    payload = {"jsonrpc": "2.0", "id": req_id, "method": method}
    if params is not None:
        payload["params"] = params
    headers = {"Content-Type": "application/json",
               "Accept": "application/json, text/event-stream"}
    if SESSION_ID:
        headers["Mcp-Session-Id"] = SESSION_ID
    req = urllib.request.Request(URL, data=json.dumps(payload).encode("utf-8"),
                                  headers=headers, method="POST")
    try:
        with urllib.request.urlopen(req, timeout=90) as r:
            sid = r.headers.get("Mcp-Session-Id")
            if sid:
                SESSION_ID = sid
            ctype = r.headers.get("Content-Type", "")
            raw = r.read().decode("utf-8", errors="replace")
    except Exception as e:
        return {"_err": str(e)}
    if "text/event-stream" in ctype:
        for line in raw.splitlines():
            if line.startswith("data: "):
                try:
                    return json.loads(line[6:])
                except Exception:
                    pass
    try:
        return json.loads(raw)
    except Exception:
        return {"_raw": raw}


def init():
    rpc("initialize", {"protocolVersion": "2024-11-05", "capabilities": {},
                       "clientInfo": {"name": "batch3", "version": "0.1"}},
        req_id="init")
    rpc("notifications/initialized", None, req_id="notif")


def call(name, args):
    resp = rpc("tools/call", {"name": name, "arguments": args}, req_id=str(uuid.uuid4()))
    r = resp.get("result", {})
    if r.get("isError"):
        return None, r.get("content", [{}])[0].get("text", "error")
    txt = r.get("content", [{}])[0].get("text", "")
    try:
        return json.loads(txt), None
    except Exception:
        return {"_raw": txt}, None


def load_rows(path):
    with open(path) as f:
        return list(csv.reader(f))


def get_perfect_from_report(path):
    """Return set of function names that are currently at 100% match."""
    try:
        d = json.load(open(path))
        return set(f.get('name', '') for f in d.get('data', []) if f.get('matching', 0) >= 0.9999)
    except Exception:
        return set()


def main():
    init()
    mapping = load_rows("D:/Project/THGlobal/th08-decomp/config/mapping.csv")
    perfect = get_perfect_from_report("D:/Project/THGlobal/th08-decomp/diff_report.json")

    def fetch(name_filter, max_size=0x150, limit=30):
        """Return entries not yet at 100% in the report."""
        out = []
        for r in mapping:
            if len(r) < 3: continue
            name, addr, size_hex = r[0], r[1], r[2]
            if "FUN_" in name: continue
            if name in perfect: continue   # skip already 100% matched
            if not name_filter(name): continue
            try:
                sz = int(size_hex, 16)
            except Exception:
                continue
            if sz > max_size: continue
            out.append((sz, name, addr, size_hex, r[3] if len(r) > 3 else "",
                        r[5] if len(r) > 5 else "", r[6:] if len(r) > 6 else []))
        out.sort(key=lambda x: x[0])  # smallest first
        return out[:limit]

    # Category A: previously-failing in batch 1
    batch1_fails = [
        "AddLives", "AddToBombCount", "AddToClockTime", "AddToYoukaiGauge",
        "GetBombsRemaining", "InitRankParams", "CollectExtend",
    ]
    fail_entries = []
    for r in mapping:
        if len(r) < 3: continue
        name = r[0]
        for f in batch1_fails:
            if name.endswith(f"::{f}") and name not in perfect:
                fail_entries.append((int(r[2], 16), name, r[1], r[2],
                                     r[3] if len(r) > 3 else "",
                                     r[5] if len(r) > 5 else "",
                                     r[6:] if len(r) > 6 else []))

    # Category B: Player class (P0)
    player_fns = fetch(lambda n: "::Player::" in n and "Player::Player" not in n, max_size=0x150, limit=15)

    # Category C: EnemyManager small functions (state capture)
    em_fns = fetch(lambda n: "::EnemyManager::" in n and "EnemyManager::EnemyManager" not in n, max_size=0xc0, limit=8)

    # Category D: BulletManager small functions
    bm_fns = fetch(lambda n: "::BulletManager::" in n and "BulletManager::BulletManager" not in n, max_size=0xc0, limit=6)

    # Category E: Remaining small GameManager
    gm_fns = fetch(lambda n: "::GameManager::" in n and "GameManager::GameManager" not in n, max_size=0x80, limit=15)

    sys.stdout.write("# Worksheet v3: larger, mixed-category batch\n\n")
    sys.stdout.write("**Categories**:\n")
    sys.stdout.write(f"- A. Re-try batch-1 failures: {len(fail_entries)}\n")
    sys.stdout.write(f"- B. Player class small/medium (P0 for DLL): {len(player_fns)}\n")
    sys.stdout.write(f"- C. EnemyManager small (state capture): {len(em_fns)}\n")
    sys.stdout.write(f"- D. BulletManager small (state capture): {len(bm_fns)}\n")
    sys.stdout.write(f"- E. Remaining small GameManager: {len(gm_fns)}\n")
    sys.stdout.write(f"\n**Total**: {len(fail_entries) + len(player_fns) + len(em_fns) + len(bm_fns) + len(gm_fns)}\n\n")
    sys.stdout.write("All functions are not-yet-100% per latest reccmp report.\n\n")
    sys.stdout.write("GameManager and Player are both on /Od /RTCu (confirmed working).\n")
    sys.stdout.write("EnemyManager and BulletManager use /Od /RTCu too per scripts/configure.py.\n\n")
    sys.stdout.write("---\n\n")

    def emit_section(header, entries):
        sys.stdout.write(f"# {header}\n\n")
        for i, (sz, name, addr, size_hex, conv, ret, args) in enumerate(entries, 1):
            sys.stdout.write(f"## {name} @ {addr} (size={size_hex})\n\n")
            filtered_args = [a for a in args if not a.endswith("*")
                             or a not in ("GameManager*", "Player*", "EnemyManager*",
                                          "BulletManager*", "Supervisor*", "Controller*")]
            args_s = ", ".join(filtered_args)
            cls_parts = name.split("::")
            short = cls_parts[-1]
            cls = "::".join(cls_parts[1:-1])
            sys.stdout.write(f"- Signature: `{ret} {cls}::{short}({args_s})` [{conv}]\n\n")
            af, err = call("analyze_function", {"addr": addr})
            if err:
                sys.stdout.write(f"- ERROR: {err}\n\n---\n\n")
                continue
            dc = af.get("decompiled", "(no hexrays)")
            # Truncate very long hexrays
            if len(dc) > 2000:
                dc = dc[:2000] + "\n... [truncated]"
            sys.stdout.write(f"- Hex-Rays:\n\n```c\n{dc}\n```\n\n")
            callees = af.get("callees", [])
            if callees:
                resolved = []
                for c in callees[:10]:
                    if c.startswith("sub_"):
                        ta = "0x" + c[4:].lower()
                        for r in mapping:
                            if len(r) > 1 and r[1].lower() == ta:
                                resolved.append(f"{c}→`{r[0]}`")
                                break
                        else:
                            resolved.append(c)
                    else:
                        resolved.append(c)
                sys.stdout.write(f"- Callees: {', '.join(resolved)}\n\n")
            gb, err = call("get_bytes", {"regions": [{"addr": addr, "size": min(sz, 64)}]})
            if gb and not err:
                try:
                    gb_list = gb if isinstance(gb, list) else [gb]
                    if gb_list:
                        data = gb_list[0].get("data", "")
                        if len(data) > 220:
                            data = data[:220] + " ..."
                        sys.stdout.write(f"- Bytes: `{data}`\n\n")
                except Exception:
                    pass
            sys.stdout.write("---\n\n")

    if fail_entries:
        emit_section("A. Batch 1 retries (these are in src but only partially matching)", fail_entries)
    if gm_fns:
        emit_section("E. GameManager remaining small", gm_fns)
    if player_fns:
        emit_section("B. Player class (P0 for DLL)", player_fns)
    if em_fns:
        emit_section("C. EnemyManager (state capture)", em_fns)
    if bm_fns:
        emit_section("D. BulletManager (state capture)", bm_fns)


if __name__ == "__main__":
    main()
