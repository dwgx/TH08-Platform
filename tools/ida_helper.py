"""Minimal MCP client for IDA Pro MCP HTTP endpoint.
Bypasses the Claude Code MCP proxy (which only exposes list_instances/open_file/select_instance)
and calls the full tool set directly on the IDA plugin HTTP server.
"""
import json
import sys
import urllib.request
import urllib.error
import uuid

URL = "http://127.0.0.1:13337/mcp"
SESSION_ID = None


def rpc(method: str, params: dict | None = None, req_id: int | str = 1) -> dict:
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
        with urllib.request.urlopen(req, timeout=120) as r:
            sid = r.headers.get("Mcp-Session-Id")
            if sid:
                SESSION_ID = sid
            ctype = r.headers.get("Content-Type", "")
            raw = r.read().decode("utf-8", errors="replace")
    except urllib.error.HTTPError as e:
        return {"_http_error": e.code, "_body": e.read().decode("utf-8", errors="replace")}
    # SSE stream or JSON
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
    r = rpc("initialize", {
        "protocolVersion": "2024-11-05",
        "capabilities": {},
        "clientInfo": {"name": "ida-helper", "version": "0.1"},
    }, req_id="init")
    rpc("notifications/initialized", None, req_id="notif")
    return r


def list_tools():
    return rpc("tools/list", {}, req_id="list")


def call(name: str, args: dict) -> dict:
    return rpc("tools/call", {"name": name, "arguments": args}, req_id=str(uuid.uuid4()))


if __name__ == "__main__":
    init()
    cmd = sys.argv[1] if len(sys.argv) > 1 else "tools"
    if cmd == "tools":
        resp = list_tools()
        tools = resp.get("result", {}).get("tools", [])
        print(f"{len(tools)} tools available:")
        for t in tools:
            desc = t.get("description", "").split("\n")[0][:80]
            print(f"  {t['name']:40s}  {desc}")
    elif cmd == "call":
        tool_name = sys.argv[2]
        args = json.loads(sys.argv[3]) if len(sys.argv) > 3 else {}
        result = call(tool_name, args)
        print(json.dumps(result, indent=2, ensure_ascii=False, default=str))
    else:
        print(f"Unknown cmd: {cmd}")
