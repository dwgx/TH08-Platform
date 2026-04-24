"""Batch 4:
1. Fix IsTampered inline→out-of-line (systemic issue; unlocks ~7 AntiTamper wrapper funcs)
2. Near-misses (<5% gap from 100%)
3. New module expansion (AsciiManager, AnmManager, GameWindow small unimpl)
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
                       "clientInfo": {"name": "batch4", "version": "0.1"}},
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


def main():
    init()
    with open("D:/Project/THGlobal/th08-decomp/config/mapping.csv") as f:
        mapping = list(csv.reader(f))
    try:
        d = json.load(open("D:/Project/THGlobal/th08-decomp/diff_report.json"))
        match_pct = {f.get('name', ''): f.get('matching', 0) for f in d.get('data', [])}
    except Exception:
        match_pct = {}

    def find_by_name_suffix(suffix, class_name=None):
        """Find entry in mapping by method name suffix."""
        for r in mapping:
            if len(r) < 3: continue
            name = r[0]
            if class_name and f"::{class_name}::" not in name: continue
            if name.endswith(f"::{suffix}"):
                return (int(r[2], 16), name, r[1], r[2],
                        r[3] if len(r) > 3 else "",
                        r[5] if len(r) > 5 else "",
                        r[6:] if len(r) > 6 else [])
        return None

    # Category A: Near-misses — functions at >= 80% match (need codegen tweaks)
    near_miss_entries = []
    for r in mapping:
        if len(r) < 3: continue
        name, addr = r[0], r[1]
        if "FUN_" in name: continue
        pct = match_pct.get(name, 0)
        if 0.80 <= pct < 0.9999:
            try:
                sz = int(r[2], 16)
            except Exception:
                continue
            if sz > 0x100: continue
            near_miss_entries.append((sz, name, addr, r[2], r[3] if len(r) > 3 else "",
                                      r[5] if len(r) > 5 else "", r[6:] if len(r) > 6 else []))
    near_miss_entries.sort(key=lambda x: -match_pct.get(x[1], 0))  # closest to 100% first
    near_miss_entries = near_miss_entries[:6]

    # Category B: Small unimplemented in AsciiManager / AnmManager / GameWindow
    def fetch_module(module, max_size=0x80, limit=8):
        out = []
        for r in mapping:
            if len(r) < 3: continue
            name = r[0]
            if "FUN_" in name: continue
            if f"::{module}::" not in name: continue
            if match_pct.get(name, 0) >= 0.9999: continue
            try:
                sz = int(r[2], 16)
            except Exception:
                continue
            if sz > max_size: continue
            out.append((sz, name, r[1], r[2], r[3] if len(r) > 3 else "",
                        r[5] if len(r) > 5 else "", r[6:] if len(r) > 6 else []))
        out.sort(key=lambda x: x[0])
        return out[:limit]

    ascii_mgr = fetch_module("AsciiManager", max_size=0x80, limit=6)
    anm_mgr = fetch_module("AnmManager", max_size=0x80, limit=6)
    game_win = fetch_module("GameWindow", max_size=0x80, limit=5)
    item_mgr = fetch_module("ItemManager", max_size=0x80, limit=4)
    score_dat = fetch_module("ScoreDat", max_size=0xa0, limit=4)

    sys.stdout.write("# Worksheet v4\n\n")
    sys.stdout.write("## Critical Systemic Fix\n\n")
    sys.stdout.write("Before implementing Category A functions, do this fix:\n\n")
    sys.stdout.write("**Move `IsTampered()` from inline in GameManager.hpp to out-of-line stub in GameManager.cpp.**\n\n")
    sys.stdout.write("Reason: Current `void IsTampered() { return FALSE; }` inline gets DCE'd, causing the `if (IsTampered()) CRASH_GAME();` branches in AddLives/AddToBombCount/etc. to be eliminated entirely. To byte-match the original (which has the branch), IsTampered needs to be a real callable function.\n\n")
    sys.stdout.write("- In src/GameManager.hpp, change:\n")
    sys.stdout.write("    ```cpp\n    ZunBool IsTampered() { return FALSE; }\n    ```\n")
    sys.stdout.write("  to:\n")
    sys.stdout.write("    ```cpp\n    ZunBool IsTampered();\n    ```\n")
    sys.stdout.write("- In src/GameManager.cpp, add (alongside UpdateAntiTamper):\n")
    sys.stdout.write("    ```cpp\n    ZunBool GameManager::IsTampered()\n    {\n        return FALSE;\n    }\n    ```\n\n")
    sys.stdout.write("After this fix, try rebuilding and see if AddLives/AddToBombCount/etc jump up naturally — they may even reach 100% if codex already implemented their bodies correctly.\n\n")
    sys.stdout.write("---\n\n")

    sys.stdout.write("# Categories\n\n")
    sys.stdout.write(f"- **A. Near-misses (>=80% match)**: {len(near_miss_entries)}\n")
    sys.stdout.write(f"- **B. AsciiManager small unimpl**: {len(ascii_mgr)}\n")
    sys.stdout.write(f"- **C. AnmManager small unimpl**: {len(anm_mgr)}\n")
    sys.stdout.write(f"- **D. GameWindow small unimpl**: {len(game_win)}\n")
    sys.stdout.write(f"- **E. ItemManager small unimpl**: {len(item_mgr)}\n")
    sys.stdout.write(f"- **F. ScoreDat small unimpl**: {len(score_dat)}\n")
    total = len(near_miss_entries) + len(ascii_mgr) + len(anm_mgr) + len(game_win) + len(item_mgr) + len(score_dat)
    sys.stdout.write(f"\n**Total (excluding the IsTampered fix)**: {total}\n\n")
    sys.stdout.write("All modules already on /Od /RTCu.\n\n")
    sys.stdout.write("---\n\n")

    def emit(header, entries):
        if not entries: return
        sys.stdout.write(f"# {header}\n\n")
        for sz, name, addr, size_hex, conv, ret, args in entries:
            pct = match_pct.get(name, 0)
            pct_s = f" (currently {pct:.1%})" if pct > 0 else ""
            sys.stdout.write(f"## {name} @ {addr} (size={size_hex}){pct_s}\n\n")
            af, err = call("analyze_function", {"addr": addr})
            if err:
                sys.stdout.write(f"- ERROR: {err}\n\n---\n\n")
                continue
            dc = af.get("decompiled", "(no hexrays)")
            if len(dc) > 1800:
                dc = dc[:1800] + "\n... [truncated]"
            sys.stdout.write(f"- Signature: `{ret} {'::'.join(name.split('::')[1:-1])}::{name.split('::')[-1]}(...)` [{conv}]\n")
            sys.stdout.write(f"- Hex-Rays:\n\n```c\n{dc}\n```\n\n")
            callees = af.get("callees", [])
            if callees:
                resolved = []
                for c in callees[:8]:
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

    emit("A. Near-misses", near_miss_entries)
    emit("B. AsciiManager small", ascii_mgr)
    emit("C. AnmManager small", anm_mgr)
    emit("D. GameWindow small", game_win)
    emit("E. ItemManager small", item_mgr)
    emit("F. ScoreDat small", score_dat)


if __name__ == "__main__":
    main()
