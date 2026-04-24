"""Compare the byte-level contents of a specific function symbol between
two COFF .obj files. Zero external deps - pure struct parsing.

Usage:
    python compare_function_bytes.py <orig.obj> <reimpl.obj> <symbol_name>

Example:
    python compare_function_bytes.py \
        build/objdiff/orig/GameManager.obj \
        build/objdiff/reimpl/GameManager.obj \
        "?SetLives@GameManager@th08@@QAEXH@Z"

Returns exit 0 and prints match info if identical.
Exit 1 with a diff if mismatch.
Exit 2 if symbol not found.
"""
import struct
import sys
from pathlib import Path


def parse_coff(data: bytes):
    """Parse COFF header, sections, symbols. Returns (header, sections, symbols, strings)."""
    # IMAGE_FILE_HEADER (20 bytes)
    fh = struct.unpack_from("<HHIIIHH", data, 0)
    machine, num_sections, timestamp, sym_ptr, num_syms, opt_size, chars = fh
    assert opt_size == 0, "COFF obj should have no optional header"

    # Sections start after file header + optional header
    sec_off = 20 + opt_size
    sections = []
    for i in range(num_sections):
        o = sec_off + i * 40
        name = data[o:o+8].rstrip(b"\x00").decode("ascii", errors="replace")
        vsize, vaddr, rsize, rptr, reloc_ptr, lineno_ptr, nrelocs, nlinenos, char = \
            struct.unpack_from("<IIIIIIHHI", data, o + 8)
        sections.append({
            "idx": i + 1,  # 1-based for symbol table
            "name": name,
            "raw_ptr": rptr,
            "raw_size": rsize,
            "chars": char,
        })

    # Symbol table: 18 bytes each
    # String table starts after symbol table
    str_tab_off = sym_ptr + num_syms * 18

    def read_name(namebytes):
        if namebytes[:4] == b"\x00\x00\x00\x00":
            off = struct.unpack("<I", namebytes[4:8])[0]
            end = data.index(b"\x00", str_tab_off + off)
            return data[str_tab_off + off:end].decode("ascii", errors="replace")
        return namebytes.rstrip(b"\x00").decode("ascii", errors="replace")

    symbols = []
    i = 0
    while i < num_syms:
        o = sym_ptr + i * 18
        nb = data[o:o+8]
        value, sec_num, typ, sclass, naux = struct.unpack_from("<iHHBB", data, o + 8)
        name = read_name(nb)
        symbols.append({
            "idx": i, "name": name, "value": value,
            "section": sec_num, "type": typ, "class": sclass,
        })
        i += 1 + naux

    return sections, symbols, str_tab_off


def find_function_bytes(path: Path, sym_predicate):
    data = path.read_bytes()
    sections, symbols, _ = parse_coff(data)

    # Find matching symbol(s)
    hits = [s for s in symbols if sym_predicate(s["name"])]
    if not hits:
        return None, None, f"no matching symbol in {path.name}"
    if len(hits) > 1:
        names = [s["name"] for s in hits]
        # Prefer ones with class 2 (external) and non-zero value
        hits = sorted(hits, key=lambda s: (s["class"] != 2, -abs(s["value"])))
    sym = hits[0]

    sec_idx = sym["section"]
    if sec_idx <= 0 or sec_idx > len(sections):
        return None, None, f"bad section {sec_idx}"
    sec = sections[sec_idx - 1]
    # Function starts at sec.raw_ptr + sym.value
    start = sec["raw_ptr"] + sym["value"]

    # Find next function symbol in same section to determine length
    next_offs = [s["value"] for s in symbols
                 if s["section"] == sec_idx and s["value"] > sym["value"]
                 and s["class"] in (2, 3) and s["type"] == 0x20]
    if next_offs:
        length = min(next_offs) - sym["value"]
    else:
        length = sec["raw_size"] - sym["value"]

    fn_bytes = data[start:start + length]
    return sym, fn_bytes, None


def match_sym_name(name_fragment: str):
    def pred(sym_name: str):
        return name_fragment in sym_name
    return pred


def hex_dump(bs: bytes, width: int = 16) -> list[str]:
    return [" ".join(f"{b:02x}" for b in bs[i:i+width]) for i in range(0, len(bs), width)]


def main():
    if len(sys.argv) < 4:
        print(__doc__)
        sys.exit(3)
    orig = Path(sys.argv[1])
    reimpl = Path(sys.argv[2])
    sym_frag = sys.argv[3]

    pred = match_sym_name(sym_frag)
    sym_o, bytes_o, err_o = find_function_bytes(orig, pred)
    sym_r, bytes_r, err_r = find_function_bytes(reimpl, pred)

    if err_o or err_r:
        print(f"ERR orig: {err_o}")
        print(f"ERR reimpl: {err_r}")
        sys.exit(2)

    print(f"orig   symbol: {sym_o['name']}  len={len(bytes_o)}")
    print(f"reimpl symbol: {sym_r['name']}  len={len(bytes_r)}")

    if bytes_o == bytes_r:
        print("")
        print(f"MATCH: {len(bytes_o)} bytes identical")
        for line in hex_dump(bytes_o):
            print(f"  {line}")
        sys.exit(0)

    # Diff
    print("")
    print(f"MISMATCH")
    print(f"orig hex:")
    for line in hex_dump(bytes_o):
        print(f"  O: {line}")
    print(f"reimpl hex:")
    for line in hex_dump(bytes_r):
        print(f"  R: {line}")
    # Byte-by-byte
    min_len = min(len(bytes_o), len(bytes_r))
    first_diff = None
    for i in range(min_len):
        if bytes_o[i] != bytes_r[i]:
            first_diff = i
            break
    if first_diff is None and len(bytes_o) != len(bytes_r):
        first_diff = min_len
    print(f"")
    print(f"first_diff_offset: {first_diff}")
    if first_diff is not None and first_diff < min_len:
        ctx_start = max(0, first_diff - 4)
        ctx_end = min(min_len, first_diff + 8)
        print(f"orig   [{ctx_start}:{ctx_end}]: {' '.join(f'{b:02x}' for b in bytes_o[ctx_start:ctx_end])}")
        print(f"reimpl [{ctx_start}:{ctx_end}]: {' '.join(f'{b:02x}' for b in bytes_r[ctx_start:ctx_end])}")
    sys.exit(1)


if __name__ == "__main__":
    main()
