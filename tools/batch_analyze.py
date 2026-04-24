"""Batch pre-analyze ~20 small GameManager functions via IDA MCP HTTP endpoint.
Emits a worksheet.md file that codex will consume.

Usage:
    python batch_analyze.py > worksheet.md
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
    headers = {
        "Content-Type": "application/json",
        "Accept": "application/json, text/event-stream",
    }
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
        return {"_raw_sse": raw}
    try:
        return json.loads(raw)
    except Exception:
        return {"_raw": raw}


def init():
    rpc("initialize", {
        "protocolVersion": "2024-11-05",
        "capabilities": {},
        "clientInfo": {"name": "batch-analyze", "version": "0.1"},
    }, req_id="init")
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


def load_candidates(mapping_path, impl_path):
    """Return list of (name, addr, size_hex, conv, ret, args_types) for small
    unimplemented GameManager functions, excluding SetLives (already done)
    and too-complex ones."""
    impl = set(l.strip() for l in open(impl_path).read().strip().split("\n"))
    out = []
    with open(mapping_path) as f:
        for row in csv.reader(f):
            if len(row) < 3: continue
            name = row[0]
            addr = row[1]
            size_hex = row[2]
            if name in impl: continue
            if "GameManager" not in name: continue
            try:
                sz = int(size_hex, 16)
            except Exception:
                continue
            # Only small functions, skip SetLives (done), skip FUN_ (no name), skip AntiTamper helpers
            if sz > 0x50: continue
            if "FUN_" in name: continue
            if "SetLives" in name: continue
            if "UpdateAntiTamper" in name: continue  # its body is 0x9a, complex
            if name.endswith("::GameManager"): continue  # constructor
            conv = row[3] if len(row) > 3 else ""
            ret = row[5] if len(row) > 5 else ""
            arg_types = row[6:] if len(row) > 6 else []
            out.append((name, addr, size_hex, conv, ret, arg_types))
    # Sort by address (binary layout order)
    out.sort(key=lambda x: int(x[1], 16))
    return out


def fmt_func_sig(name, ret, conv, args):
    short_name = name.split("::")[-1]
    cls = "::".join(name.split("::")[1:-1])  # drop th08::
    if args and args[0] == "GameManager*":
        args = args[1:]  # this pointer is implicit in __thiscall
    args_s = ", ".join(args) if args else ""
    return f"{ret} {cls}::{short_name}({args_s})  // {conv}"


def main():
    init()
    cands = load_candidates(
        "D:/Project/THGlobal/th08-decomp/config/mapping.csv",
        "D:/Project/THGlobal/th08-decomp/config/implemented.csv",
    )
    # Take up to 25 to have room
    cands = cands[:25]

    sys.stdout.write(f"# Worksheet: {len(cands)} GameManager functions to byte-match\n\n")
    sys.stdout.write("Each section has the info codex needs to implement the function.\n")
    sys.stdout.write("All functions use `/Od /RTCu` (GameManager was switched in Stage 6).\n\n")
    sys.stdout.write("---\n\n")

    for i, (name, addr, size_hex, conv, ret, args) in enumerate(cands, 1):
        sig = fmt_func_sig(name, ret, conv, args)
        sys.stdout.write(f"## {i}. `{name}` @ `{addr}` (size={size_hex})\n\n")
        sys.stdout.write(f"**Signature**: `{sig}`\n\n")

        af, err = call("analyze_function", {"addr": addr})
        if err:
            sys.stdout.write(f"**ERROR**: {err}\n\n---\n\n")
            continue

        decompiled = af.get("decompiled", "(no hexrays)")
        callees = af.get("callees", [])
        sys.stdout.write(f"**Hex-Rays**:\n\n```c\n{decompiled}\n```\n\n")
        if callees:
            sys.stdout.write(f"**Callees**: {', '.join(callees)}\n\n")

        # Get raw bytes for reference (truncate if long)
        sz_int = int(size_hex, 16)
        gb, err = call("get_bytes", {"regions": [{"addr": addr, "size": min(sz_int, 64)}]})
        if gb and not err:
            try:
                gb_list = gb if isinstance(gb, list) else [gb]
                if gb_list:
                    data = gb_list[0].get("data", "")
                    if len(data) > 200:
                        data = data[:200] + " ..."
                    sys.stdout.write(f"**Raw bytes**: `{data}`\n\n")
            except Exception:
                pass

        # Resolve callee names from mapping.csv
        if callees:
            resolved = []
            with open("D:/Project/THGlobal/th08-decomp/config/mapping.csv") as f:
                m_rows = list(csv.reader(f))
            for callee in callees:
                if callee.startswith("sub_"):
                    target_addr = "0x" + callee[4:].lower()
                    for r in m_rows:
                        if len(r) > 1 and r[1].lower() == target_addr:
                            resolved.append(f"  {callee} → `{r[0]}`")
                            break
                    else:
                        resolved.append(f"  {callee} → (not in mapping)")
            if resolved:
                sys.stdout.write("**Callee resolution**:\n")
                for r in resolved:
                    sys.stdout.write(f"{r}\n")
                sys.stdout.write("\n")

        sys.stdout.write("---\n\n")


if __name__ == "__main__":
    main()
