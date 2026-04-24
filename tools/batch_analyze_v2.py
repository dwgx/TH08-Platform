"""Batch 2: prioritize by DLL-hook relevance.
- Fix 5 near-misses from Batch 1 (GetFlag*, ScaleFloatBasedOnRank)
- Target small Player functions (for P2 implementation)
- Target small Controller/Supervisor functions (for input + frame loop hooks)
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
        with urllib.request.urlopen(req, timeout=60) as r:
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
                       "clientInfo": {"name": "batch2", "version": "0.1"}},
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


def main():
    init()
    mapping = load_rows("D:/Project/THGlobal/th08-decomp/config/mapping.csv")
    impl = set(l.strip() for l in open(
        "D:/Project/THGlobal/th08-decomp/config/implemented.csv").read().strip().split("\n"))

    def fetch(name_filter, max_size=0x80, limit=15, exclude_impl=True):
        out = []
        for r in mapping:
            if len(r) < 3: continue
            name, addr, size_hex = r[0], r[1], r[2]
            if "FUN_" in name: continue
            if exclude_impl and name in impl: continue
            if not name_filter(name): continue
            try:
                sz = int(size_hex, 16)
            except Exception:
                continue
            if sz > max_size: continue
            out.append((sz, name, addr, size_hex, r[3] if len(r) > 3 else "",
                        r[5] if len(r) > 5 else "", r[6:] if len(r) > 6 else []))
        out.sort(key=lambda x: int(x[2], 16))
        return out[:limit]

    # Category A: fix near-misses (these were in batch 1, already in src, just need re-verification)
    near_misses = ["GetFlag0", "GetFlag1", "GetFlag3", "GetFlag14", "ScaleFloatBasedOnRank"]
    near_miss_entries = []
    for r in mapping:
        if len(r) < 3: continue
        name = r[0]
        for nm in near_misses:
            if name.endswith(f"::{nm}"):
                near_miss_entries.append((int(r[2], 16), name, r[1], r[2],
                                          r[3] if len(r) > 3 else "",
                                          r[5] if len(r) > 5 else "",
                                          r[6:] if len(r) > 6 else []))

    # Category B: Player class - P0 for DLL (P2 implementation)
    player_fns = fetch(lambda n: "::Player::" in n and "EclContext" not in n, max_size=0x80, limit=8)

    # Category C: Controller - P1 for input hooks
    ctrl_fns = fetch(lambda n: "::Controller::" in n, max_size=0x80, limit=4)

    # Category D: Supervisor - P1 for frame loop
    sup_fns = fetch(lambda n: "::Supervisor::" in n, max_size=0x80, limit=4)

    # Print the worksheet
    sys.stdout.write("# Worksheet v2: DLL-oriented decomp batch\n\n")
    sys.stdout.write("**Categories**:\n")
    sys.stdout.write(f"- A. Near-miss fixes from batch 1: {len(near_miss_entries)} functions\n")
    sys.stdout.write(f"- B. Player class (for P2 impl): {len(player_fns)} functions\n")
    sys.stdout.write(f"- C. Controller class (for input hook): {len(ctrl_fns)} functions\n")
    sys.stdout.write(f"- D. Supervisor class (for frame loop hook): {len(sup_fns)} functions\n\n")
    sys.stdout.write("Context: GameManager already uses /Od /RTCu. reccmp workflow known.\n")
    sys.stdout.write("Reference byte-matched functions in same class: MidiOutput::SetFadeOut.\n\n")
    sys.stdout.write("---\n\n")

    def emit_section(header, entries):
        sys.stdout.write(f"# {header}\n\n")
        for i, (sz, name, addr, size_hex, conv, ret, args) in enumerate(entries, 1):
            sys.stdout.write(f"## {name} @ {addr} (size={size_hex})\n\n")
            args_s = ", ".join(a for a in args if a != "GameManager*" and a != "Player*" and a != "Controller*" and a != "Supervisor*")
            cls_parts = name.split("::")
            short = cls_parts[-1]
            cls = "::".join(cls_parts[1:-1])
            sys.stdout.write(f"- **Signature**: `{ret} {cls}::{short}({args_s})` [{conv}]\n\n")
            af, err = call("analyze_function", {"addr": addr})
            if err:
                sys.stdout.write(f"- **ERROR**: {err}\n\n---\n\n")
                continue
            dc = af.get("decompiled", "(no hexrays)")
            sys.stdout.write(f"- **Hex-Rays**:\n\n```c\n{dc}\n```\n\n")
            callees = af.get("callees", [])
            if callees:
                # Resolve names
                resolved = []
                for c in callees:
                    if c.startswith("sub_"):
                        ta = "0x" + c[4:].lower()
                        for r in mapping:
                            if len(r) > 1 and r[1].lower() == ta:
                                resolved.append(f"{c}→`{r[0]}`")
                                break
                        else:
                            resolved.append(f"{c}")
                    else:
                        resolved.append(c)
                sys.stdout.write(f"- **Callees**: {', '.join(resolved)}\n\n")
            gb, err = call("get_bytes", {"regions": [{"addr": addr, "size": min(sz, 64)}]})
            if gb and not err:
                try:
                    gb_list = gb if isinstance(gb, list) else [gb]
                    if gb_list:
                        data = gb_list[0].get("data", "")
                        if len(data) > 220:
                            data = data[:220] + " ..."
                        sys.stdout.write(f"- **Bytes**: `{data}`\n\n")
                except Exception:
                    pass
            sys.stdout.write("---\n\n")

    if near_miss_entries:
        emit_section("CATEGORY A: Near-miss fixes (already in src, need codegen adjustment)",
                     near_miss_entries)
    if player_fns:
        emit_section("CATEGORY B: Player class (P0 for DLL/P2 impl)", player_fns)
    if ctrl_fns:
        emit_section("CATEGORY C: Controller class (P1 for input hook)", ctrl_fns)
    if sup_fns:
        emit_section("CATEGORY D: Supervisor class (P1 for frame loop hook)", sup_fns)


if __name__ == "__main__":
    main()
