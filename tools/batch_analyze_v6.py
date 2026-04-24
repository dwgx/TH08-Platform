"""Batch 6: broader near-miss net (>=68%) + Player + Ending + ScoreDat fresh."""
import csv, json, sys, urllib.request, uuid
URL = "http://127.0.0.1:13337/mcp"
SESSION_ID = None

def rpc(method, params=None, req_id=1):
    global SESSION_ID
    payload = {"jsonrpc":"2.0","id":req_id,"method":method}
    if params: payload["params"] = params
    h = {"Content-Type":"application/json","Accept":"application/json, text/event-stream"}
    if SESSION_ID: h["Mcp-Session-Id"] = SESSION_ID
    req = urllib.request.Request(URL, data=json.dumps(payload).encode("utf-8"), headers=h, method="POST")
    try:
        with urllib.request.urlopen(req, timeout=90) as r:
            sid = r.headers.get("Mcp-Session-Id")
            if sid: SESSION_ID = sid
            ctype = r.headers.get("Content-Type","")
            raw = r.read().decode("utf-8", errors="replace")
    except Exception as e:
        return {"_err":str(e)}
    if "text/event-stream" in ctype:
        for line in raw.splitlines():
            if line.startswith("data: "):
                try: return json.loads(line[6:])
                except: pass
    try: return json.loads(raw)
    except: return {"_raw":raw}

def init():
    rpc("initialize",{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"b6","version":"0.1"}},"init")
    rpc("notifications/initialized",None,"notif")

def call(name, args):
    resp = rpc("tools/call",{"name":name,"arguments":args},str(uuid.uuid4()))
    r = resp.get("result",{})
    if r.get("isError"): return None, r.get("content",[{}])[0].get("text","error")
    txt = r.get("content",[{}])[0].get("text","")
    try: return json.loads(txt), None
    except: return {"_raw":txt}, None

def main():
    init()
    mapping = list(csv.reader(open("D:/Project/THGlobal/th08-decomp/config/mapping.csv")))
    try:
        d = json.load(open("D:/Project/THGlobal/th08-decomp/diff_report.json"))
        pct = {f.get('name',''): f.get('matching',0) for f in d.get('data',[])}
    except: pct = {}

    # A: broad near-misses 68-99%
    near = []
    for r in mapping:
        if len(r) < 3 or "FUN_" in r[0]: continue
        p = pct.get(r[0], 0)
        if 0.68 <= p < 0.9999:
            try: sz = int(r[2], 16)
            except: continue
            if sz > 0x150: continue
            near.append((sz, r[0], r[1], r[2], r[3] if len(r)>3 else "", r[5] if len(r)>5 else "", r[6:] if len(r)>6 else []))
    near.sort(key=lambda x: -pct.get(x[1],0))
    near = near[:12]

    # B: Player small unimpl (P0 for DLL)
    player = []
    for r in mapping:
        if len(r) < 3 or "FUN_" in r[0]: continue
        if "::Player::" not in r[0]: continue
        if pct.get(r[0], 0) >= 0.9999: continue
        try: sz = int(r[2], 16)
        except: continue
        if sz > 0xa0: continue
        player.append((sz, r[0], r[1], r[2], r[3] if len(r)>3 else "", r[5] if len(r)>5 else "", r[6:] if len(r)>6 else []))
    player.sort(key=lambda x: x[0])
    player = player[:6]

    # C: Ending (0/9 module, fresh)
    ending = []
    for r in mapping:
        if len(r) < 3 or "FUN_" in r[0]: continue
        if "::Ending::" not in r[0]: continue
        if pct.get(r[0], 0) >= 0.9999: continue
        try: sz = int(r[2], 16)
        except: continue
        if sz > 0xa0: continue
        ending.append((sz, r[0], r[1], r[2], r[3] if len(r)>3 else "", r[5] if len(r)>5 else "", r[6:] if len(r)>6 else []))
    ending.sort(key=lambda x: x[0])
    ending = ending[:5]

    # D: ScoreDat (0/11, fresh)
    scoredat = []
    for r in mapping:
        if len(r) < 3 or "FUN_" in r[0]: continue
        if "::ScoreDat::" not in r[0]: continue
        if pct.get(r[0], 0) >= 0.9999: continue
        try: sz = int(r[2], 16)
        except: continue
        if sz > 0x80: continue
        scoredat.append((sz, r[0], r[1], r[2], r[3] if len(r)>3 else "", r[5] if len(r)>5 else "", r[6:] if len(r)>6 else []))
    scoredat.sort(key=lambda x: x[0])
    scoredat = scoredat[:5]

    sys.stdout.write("# Worksheet v6\n\n")
    sys.stdout.write(f"- A. Near-misses (68%+): {len(near)}\n")
    sys.stdout.write(f"- B. Player class small: {len(player)}\n")
    sys.stdout.write(f"- C. Ending fresh: {len(ending)}\n")
    sys.stdout.write(f"- D. ScoreDat fresh: {len(scoredat)}\n\n")
    sys.stdout.write(f"**Total**: {len(near)+len(player)+len(ending)+len(scoredat)}\n\n")
    sys.stdout.write("---\n\n")

    def emit(h, entries):
        if not entries: return
        sys.stdout.write(f"# {h}\n\n")
        for sz, name, addr, size_hex, conv, ret, args in entries:
            p = pct.get(name, 0)
            ps = f" (currently {p:.1%})" if p > 0 else ""
            sys.stdout.write(f"## {name} @ {addr} (size={size_hex}){ps}\n\n")
            af, err = call("analyze_function", {"addr": addr})
            if err:
                sys.stdout.write(f"- ERROR: {err}\n\n---\n\n"); continue
            dc = af.get("decompiled","(no hexrays)")
            if len(dc) > 1500: dc = dc[:1500] + "\n... [truncated]"
            cls = "::".join(name.split("::")[1:-1])
            short = name.split("::")[-1]
            sys.stdout.write(f"- Sig: `{ret} {cls}::{short}(...)` [{conv}]\n")
            sys.stdout.write(f"- Hex-Rays:\n\n```c\n{dc}\n```\n\n")
            callees = af.get("callees", [])
            if callees:
                res = []
                for c in callees[:6]:
                    if c.startswith("sub_"):
                        ta = "0x"+c[4:].lower()
                        for r in mapping:
                            if len(r)>1 and r[1].lower()==ta:
                                res.append(f"{c}→`{r[0]}`"); break
                        else: res.append(c)
                    else: res.append(c)
                sys.stdout.write(f"- Callees: {', '.join(res)}\n\n")
            gb, err = call("get_bytes", {"regions":[{"addr":addr,"size":min(sz,48)}]})
            if gb and not err:
                try:
                    gb_list = gb if isinstance(gb, list) else [gb]
                    if gb_list:
                        data = gb_list[0].get("data","")
                        if len(data)>200: data = data[:200]+"..."
                        sys.stdout.write(f"- Bytes: `{data}`\n\n")
                except: pass
            sys.stdout.write("---\n\n")

    emit("A. Near-misses (68%-99%)", near)
    emit("B. Player class (P0 for DLL)", player)
    emit("C. Ending (fresh module)", ending)
    emit("D. ScoreDat (fresh module)", scoredat)

if __name__ == "__main__":
    main()
