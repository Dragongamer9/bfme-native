"""Minimal 32-bit PE model used by the BFME analysis tools.

Wraps pefile + capstone with the handful of queries the scanners need:
section classification, VA<->bytes, C-string reads, little-endian dword reads,
and a lazily built index of every 4-byte-aligned pointer in data sections.
"""
import os
import re
import struct
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
for extra in (os.path.join(_HERE, "..", "deps", "capstone", "bindings", "python"),
              os.path.join(_HERE, "..", "deps", "pefile")):
    if os.path.isdir(extra) and extra not in sys.path:
        sys.path.insert(0, extra)

import pefile  # noqa: E402
import capstone  # noqa: E402

IDENT_RE = re.compile(rb"^[A-Za-z_][A-Za-z0-9_:\.\-]{0,95}$")

IMAGE_SCN_MEM_EXECUTE = 0x20000000
IMAGE_SCN_CNT_CODE = 0x20


class Section:
    def __init__(self, name, va, data, executable):
        self.name, self.va, self.data, self.executable = name, va, data, executable
        self.end = va + len(data)

    def __contains__(self, va):
        return self.va <= va < self.end


class PE:
    def __init__(self, path):
        self.path = path
        self.pe = pefile.PE(path, fast_load=True)
        if self.pe.FILE_HEADER.Machine != 0x14C:
            raise SystemExit(f"{path}: not a 32-bit x86 PE (machine {self.pe.FILE_HEADER.Machine:#x})")
        self.base = self.pe.OPTIONAL_HEADER.ImageBase
        self.sections = []
        for s in self.pe.sections:
            name = s.Name.rstrip(b"\0").decode("latin-1")
            size = max(s.Misc_VirtualSize, s.SizeOfRawData)
            data = s.get_data()[:size].ljust(size, b"\0")
            exe = bool(s.Characteristics & (IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_CNT_CODE))
            self.sections.append(Section(name, self.base + s.VirtualAddress, data, exe))
        self.code = [s for s in self.sections if s.executable]
        self.datasecs = [s for s in self.sections if not s.executable]
        self.md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
        self.md.detail = True
        self._ptr_index = None

    # -- address helpers -------------------------------------------------
    def section_of(self, va):
        for s in self.sections:
            if va in s:
                return s
        return None

    def is_code(self, va):
        return any(va in s for s in self.code)

    def is_data(self, va):
        return any(va in s for s in self.datasecs)

    def read(self, va, n):
        s = self.section_of(va)
        if not s:
            return None
        off = va - s.va
        return s.data[off:off + n]

    def u32(self, va):
        b = self.read(va, 4)
        return struct.unpack("<I", b)[0] if b and len(b) == 4 else None

    def cstr(self, va, limit=256):
        s = self.section_of(va)
        if not s or s.executable:
            return None
        off = va - s.va
        end = s.data.find(b"\0", off, off + limit)
        if end < 0:
            return None
        return s.data[off:end]

    def ident(self, va):
        """Return the string at va if it looks like an INI/C identifier, else None."""
        raw = self.cstr(va, 100)
        if raw and IDENT_RE.match(raw):
            return raw.decode("ascii")
        return None

    # -- pointer index ---------------------------------------------------
    def ptr_index(self):
        """{target_va: [location_va, ...]} for every aligned dword in data sections."""
        if self._ptr_index is None:
            idx = {}
            lo = min(s.va for s in self.sections)
            hi = max(s.end for s in self.sections)
            for s in self.datasecs:
                d = s.data
                for off in range(0, len(d) - 3, 4):
                    v = struct.unpack_from("<I", d, off)[0]
                    if lo <= v < hi:
                        idx.setdefault(v, []).append(s.va + off)
            self._ptr_index = idx
        return self._ptr_index

    def find_bytes(self, needle):
        """Yield VAs of every occurrence of needle in non-executable sections."""
        for s in self.datasecs:
            i = s.data.find(needle)
            while i >= 0:
                yield s.va + i
                i = s.data.find(needle, i + 1)

    def string_va(self, text):
        """VA of a NUL-terminated exact string (first match), or None."""
        needle = text.encode("latin-1") + b"\0"
        for va in self.find_bytes(needle):
            prev = self.read(va - 1, 1)
            if prev in (b"\0", None):
                return va
        return None

    # -- disassembly -----------------------------------------------------
    def disasm(self, va, max_insns=4000, stop_at_ret=True):
        s = self.section_of(va)
        if not s or not s.executable:
            return
        off = va - s.va
        n = 0
        for ins in self.md.disasm(s.data[off:], va):
            yield ins
            n += 1
            if n >= max_insns or (stop_at_ret and ins.mnemonic in ("ret", "retn")):
                return

    def code_xrefs_to(self, targets):
        """Linear-sweep every code section once; return {target: [insn_va,...]} for
        instructions with an immediate or displacement equal to a target VA."""
        targets = set(targets)
        out = {t: [] for t in targets}
        md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
        md.skipdata = True
        for s in self.code:
            for addr, size, mnem, ops in md.disasm_lite(s.data, s.va):
                if not ops or "0x" not in ops:
                    continue
                for tok in re.findall(r"0x[0-9a-f]+", ops):
                    v = int(tok, 16)
                    if v in targets:
                        out[v].append(addr)
        return out
