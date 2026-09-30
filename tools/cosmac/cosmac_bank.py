#!/usr/bin/env python3
############################################################################
# tools/cosmac/cosmac_bank.py
#
# SPDX-License-Identifier: Apache-2.0
#
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.  The
# ASF licenses this file to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance with the
# License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
# License for the specific language governing permissions and limitations
# under the License.
#
############################################################################

"""Place CDP1802 (COSMAC) objects in code banks and generate far-call stubs.

The CDP1802 port can bank code (arch/cosmac, ADR 0013 of the port): a 16 KiB
window at 0x8000 shows one bank of a larger ROM.  This tool runs at link
time.  It reads the archives and objects of the link, assigns objects to
banks by placement rules, and writes:

- copies of the archives in which every banked object has
  * its code sections renamed .text* -> .bankN.text* (the linker script puts
    them in bank N),
  * its global functions renamed foo -> __bank_foo, and the calls it makes
    to other functions of the same bank renamed the same way (direct calls);
- an assembler file with one stub per banked function, under the original
  name, which calls the far-call trampoline (cosmac_farcall.S):
        foo: sep 4 ; .hword __cosmac_farcall ; .hword __bank_foo ; .byte N
  so every other reference to foo -- calls from other regions, tail calls,
  function pointers -- goes through the stub;
- a report.

An object that takes the address of one of its own functions (a table of
static functions, a callback) would hand out a pointer into its bank; the
tool rewrites such references (ELF symbol table and relocations) to go
through a stub too: the function's own stub for a global function, so that
pointers compare equal everywhere, or a generated one (__cosmac_ptr_N ->
__cosmac_fn_N) for a static function.  An object stays in fixed ROM only
when it has code outside .text* or defines a symbol that is also defined
elsewhere.  A reference is a *call* when it is the 16-bit operand of
"sep 4" (the NCRT call) or of a long branch (a tail call); every other
reference to a function is an address.

Placement rules (first match wins; unmatched objects stay fixed):
    <bank>|auto|fixed|hot   <archive-glob>:<member-glob>
Plain objects match as "-:<file name>".  "hot" objects (small functions
called from everywhere) stay in fixed ROM while it has room, in rule order,
and are otherwise placed like "auto": the room is the fixed ROM (--window)
minus everything that is not banked in the flat link's map (--map), the
stubs and a margin (--fixed-reserve).  "auto" packs objects, largest
first, into the first bank with room; an object that does not fit any bank
as a whole is split, function (code section) by function.  Object sizes come from the map of a
flat link (--map) when given: only code that survives --gc-sections counts,
and objects that are not linked at all are left alone.
"""

import argparse
import fnmatch
import os
import re
import shutil
import struct
import subprocess
import sys

R_CDP1802_16 = 4
CALL_OPCODES = {
    0xD4,  # sep 4 (NCRT call)
    0xC0,
    0xC1,
    0xC2,
    0xC3,
    0xC9,
    0xCA,
    0xCB,
}  # long branches
SHF_ALLOC = 0x2
SHF_EXECINSTR = 0x4
STB_LOCAL, STB_GLOBAL, STB_WEAK = 0, 1, 2
STT_NOTYPE, STT_FUNC, STT_SECTION = 0, 2, 3
SHN_UNDEF = 0
SHT_SYMTAB, SHT_RELA, SHT_REL = 2, 4, 9
PREFIX = "__bank_"


# ELF32 reading ##############################################################


class ElfObject:
    """The parts of an ELF32 relocatable object the tool needs."""

    def __init__(self, data, name):
        self.name = name
        self.data = data
        if data[:4] != b"\x7fELF" or data[4] != 1:
            raise ValueError("%s: not an ELF32 object" % name)
        en = ">" if data[5] == 2 else "<"
        self.en = en
        hdr = struct.unpack(en + "HHIIIIIHHHHHH", data[16:52])
        shoff, shentsize, shnum, shstrndx = hdr[5], hdr[10], hdr[11], hdr[12]
        self.sections = []
        for i in range(shnum):
            o = shoff + i * shentsize
            f = struct.unpack(en + "IIIIIIIIII", data[o : o + 40])
            self.sections.append(
                dict(
                    name_off=f[0],
                    type=f[1],
                    flags=f[2],
                    off=f[4],
                    size=f[5],
                    link=f[6],
                    info=f[7],
                )
            )
        strtab = self.sections[shstrndx]
        for s in self.sections:
            s["name"] = self._string(strtab, s["name_off"])
        self.symbols = []
        for s in self.sections:
            if s["type"] == SHT_SYMTAB:
                names = self.sections[s["link"]]
                for i in range(s["size"] // 16):
                    o = s["off"] + i * 16
                    nm, val, size, info, _, shndx = struct.unpack(
                        en + "IIIBBH", data[o : o + 16]
                    )
                    self.symbols.append(
                        dict(
                            name=self._string(names, nm),
                            value=val,
                            bind=info >> 4,
                            type=info & 15,
                            shndx=shndx,
                        )
                    )

    def _string(self, sec, off):
        start = sec["off"] + off
        return self.data[start : self.data.index(b"\0", start)].decode()

    def is_code(self, index):
        s = self.sections[index]
        return (s["flags"] & (SHF_ALLOC | SHF_EXECINSTR)) == (SHF_ALLOC | SHF_EXECINSTR)

    def relocations(self):
        """Yield (section index, offset, type, symbol, addend) for every
        relocation that applies to an allocated section."""
        for s in self.sections:
            if s["type"] == SHT_REL:
                raise ValueError("%s: REL sections are not supported" % self.name)
            if s["type"] != SHT_RELA:
                continue
            target = s["info"]
            if not self.sections[target]["flags"] & SHF_ALLOC:
                continue
            for i in range(s["size"] // 12):
                o = s["off"] + i * 12
                off, info, addend = struct.unpack(
                    self.en + "IIi", self.data[o : o + 12]
                )
                yield target, off, info & 0xFF, self.symbols[info >> 8], addend

    def byte_at(self, index, off):
        return self.data[self.sections[index]["off"] + off]

    def relocations_at(self):
        """Like relocations(), with the RELA section index and entry number
        first: (rela, entry, section, offset, type, symbol, addend)."""
        for r, s in enumerate(self.sections):
            if s["type"] != SHT_RELA:
                continue
            target = s["info"]
            if not self.sections[target]["flags"] & SHF_ALLOC:
                continue
            for i in range(s["size"] // 12):
                o = s["off"] + i * 12
                off, info, addend = struct.unpack(
                    self.en + "IIi", self.data[o : o + 12]
                )
                yield r, i, target, off, info & 0xFF, self.symbols[info >> 8], addend

    def rewrite(self, new_symbols, redirects):
        """Return a copy of the object with global symbols appended and some
        relocations pointed at them.

        new_symbols: [(name, value, section index or 0, is_function)];
        redirects: {(rela section, entry): (new symbol number, addend)},
        where new symbol number n refers to new_symbols[n].  The appended
        .symtab, .strtab and changed RELA sections go at the end of the
        file; their section headers are updated, nothing else moves.
        """
        en, d = self.en, bytearray(self.data)
        symtab = next(i for i, s in enumerate(self.sections) if s["type"] == SHT_SYMTAB)
        strtab = self.sections[symtab]["link"]
        first_new = len(self.symbols)
        strs = bytearray(self.contents_of(strtab))
        syms = bytearray(self.contents_of(symtab))
        for name, value, shndx, func in new_symbols:
            info = (STB_GLOBAL << 4) | (STT_FUNC if func else STT_NOTYPE)
            syms += struct.pack(en + "IIIBBH", len(strs), value, 0, info, 0, shndx)
            strs += name.encode() + b"\0"
        blobs = {symtab: bytes(syms), strtab: bytes(strs)}
        for (rela, entry), (n, addend) in redirects.items():
            if rela not in blobs:
                blobs[rela] = bytearray(self.contents_of(rela))
            off, info = struct.unpack_from(en + "II", blobs[rela], entry * 12)
            info = ((first_new + n) << 8) | (info & 0xFF)
            struct.pack_into(en + "IIi", blobs[rela], entry * 12, off, info, addend)
        (shoff,) = struct.unpack_from(en + "I", d, 0x20)
        (shentsize,) = struct.unpack_from(en + "H", d, 0x2E)
        for index, blob in sorted(blobs.items()):
            d += b"\0" * (-len(d) % 4)
            hdr = shoff + index * shentsize
            struct.pack_into(en + "II", d, hdr + 16, len(d), len(blob))
            d += blob
        return bytes(d)

    def contents_of(self, index):
        s = self.sections[index]
        return self.data[s["off"] : s["off"] + s["size"]]


def read_archive(path):
    """Return the members of an ar archive as a list of (name, bytes)."""
    data = open(path, "rb").read()
    if data[:8] != b"!<arch>\n":
        raise ValueError("%s: not an archive" % path)
    members = []
    pos, longnames = 8, b""
    while pos < len(data):
        hdr = data[pos : pos + 60]
        name = hdr[:16].decode().strip()
        size = int(hdr[48:58])
        body = data[pos + 60 : pos + 60 + size]
        if name in ("/", "/SYM64/"):
            pass
        elif name == "//":
            longnames = body
        else:
            if name.startswith("/"):
                o = int(name[1:])
                name = longnames[o : longnames.index(b"/\n", o)].decode()
            else:
                name = name.rstrip("/")
            members.append((name, body))
        pos += 60 + size + (size & 1)
    return members


# Analysis ##################################################################


class Ref:
    """One relocation in an allocated section of a unit."""

    __slots__ = ("rela", "entry", "sec", "call", "name", "tsec", "toff")

    def __init__(self, rela, entry, sec, call, name, tsec, toff):
        self.rela, self.entry, self.sec, self.call = rela, entry, sec, call
        self.name = name  # global name, or None for a local target
        self.tsec = tsec  # code section of a target in this unit, or None
        self.toff = toff  # offset of that target in its section


class Unit:
    """One object file of the link (an archive member or a plain object)."""

    def __init__(self, archive, member, data):
        self.archive = archive  # archive basename, or "-" for plain objects
        self.member = member
        self.elf = ElfObject(data, "%s:%s" % (archive, member))
        self.why_fixed = None
        self.split = False  # its code sections are in different regions
        self.code = {}  # code section index -> size (kept code)
        self.secbank = {}  # code section index -> bank (absent: fixed)
        self.defined = {}  # global function name -> (bind, section index)
        self.starts = {}  # code section index -> {function start offsets}
        self.refs = []
        self.foreign_code = False  # code outside .text*
        self._analyse()

    @property
    def key(self):
        return "%s:%s" % (self.archive, self.member)

    @property
    def size(self):
        return sum(self.code.values())

    @property
    def banked(self):
        return bool(self.secbank)

    def _analyse(self):
        e = self.elf
        for i, s in enumerate(e.sections):
            if e.is_code(i) and s["size"]:
                self.code[i] = s["size"]
                if not (s["name"] == ".text" or s["name"].startswith(".text.")):
                    self.foreign_code = True
        for sym in e.symbols:
            x = sym["shndx"]
            if x == SHN_UNDEF or x >= 0xFF00 or not e.is_code(x):
                continue
            if sym["type"] in (STT_FUNC, STT_NOTYPE) and sym["name"]:
                self.starts.setdefault(x, set()).add(sym["value"])
                if sym["bind"] in (STB_GLOBAL, STB_WEAK):
                    self.defined[sym["name"]] = (sym["bind"], x)
        for rela, entry, sec, off, typ, sym, addend in e.relocations_at():
            call = (
                typ == R_CDP1802_16
                and e.is_code(sec)
                and off > 0
                and e.byte_at(sec, off - 1) in CALL_OPCODES
            )
            x = sym["shndx"]
            if x == SHN_UNDEF:
                if sym["name"]:
                    self.refs.append(Ref(rela, entry, sec, call, sym["name"], None, 0))
                continue
            if x >= 0xFF00 or not e.is_code(x):
                continue  # data, absolute
            toff = addend if sym["type"] == STT_SECTION else sym["value"] + addend
            if toff != 0 and toff not in self.starts.get(x, ()):
                continue  # a label inside a function: same section only
            glob = sym["type"] != STT_SECTION and sym["bind"] != STB_LOCAL
            name = sym["name"] if glob else None
            self.refs.append(Ref(rela, entry, sec, call, name, x, toff))

    def bank_of_section(self, sec):
        return self.secbank.get(sec)

    def function_at(self, shndx, value):
        """(name, is_global) of the function symbol at a code location."""
        best = None
        for sym in self.elf.symbols:
            if (
                sym["shndx"] == shndx
                and sym["value"] == value
                and sym["type"] in (STT_FUNC, STT_NOTYPE)
                and sym["name"]
            ):
                if sym["bind"] in (STB_GLOBAL, STB_WEAK):
                    return sym["name"], True
                best = best or sym["name"]
        return best or self.elf.sections[shndx]["name"], False


def parse_rules(path):
    rules = []
    for n, line in enumerate(open(path), 1):
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        fields = line.split()
        if len(fields) != 2 or ":" not in fields[1]:
            raise SystemExit(
                "%s:%d: expected '<bank>|auto|fixed|hot <archive>:<member>'" % (path, n)
            )
        where, pattern = fields
        if where not in ("auto", "fixed", "hot"):
            where = int(where)
        arch, member = pattern.split(":", 1)
        rules.append((where, arch, member))
    return rules


def match_rule(rules, unit):
    return match_rule_index(rules, unit)[0]


def match_rule_index(rules, unit):
    """(placement, index of the matching rule) of a unit."""
    for n, (where, arch, member) in enumerate(rules):
        if fnmatch.fnmatchcase(unit.archive, arch) and fnmatch.fnmatchcase(
            unit.member, member
        ):
            return where, n
    return "fixed", len(rules)


MAP_SECTION = re.compile(r"^ (\.text\S*)\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S.*)$")
MAP_NAME_ONLY = re.compile(r"^ (\.text\S*)$")
MAP_CONT = re.compile(r"^\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S.*)$")


def map_sizes(path):
    """{unit key: {code section name: bytes}} kept in a GNU ld map."""
    sizes = {}
    lines = open(path).read().split("\n")
    try:
        start = lines.index("Linker script and memory map")
    except ValueError:
        raise SystemExit("%s: not a GNU ld map" % path)
    pending = None
    for line in lines[start:]:
        m = MAP_SECTION.match(line)
        if m:
            name, size, where = m.group(1), int(m.group(3), 16), m.group(4)
        elif pending and MAP_CONT.match(line):
            c = MAP_CONT.match(line)
            name, size, where = pending.group(1), int(c.group(2), 16), c.group(3)
        else:
            pending = MAP_NAME_ONLY.match(line)
            continue
        pending = None
        m = re.match(r"^(.*)\((.*)\)$", where)
        key = (
            "%s:%s" % (os.path.basename(m.group(1)), m.group(2))
            if m
            else "-:%s" % os.path.basename(where)
        )
        secs = sizes.setdefault(key, {})
        secs[name] = secs.get(name, 0) + size
    return sizes


MAP_OUTPUT = re.compile(r"^(\.\S+)\s+0x[0-9a-f]+\s+0x([0-9a-f]+)")


def map_rom_total(path):
    """Bytes of ROM (code, read-only data, initialized data) in a map of the
    measuring link (cosmac_measure.ld)."""
    total = 0
    for line in open(path):
        m = MAP_OUTPUT.match(line)
        if m and m.group(1) in (".text", ".init_section", ".data"):
            total += int(m.group(2), 16)
    return total


# Placement ##################################################################


def read_inputs(paths):
    """Return [(path, [Unit])] in link order."""
    inputs = []
    for path in paths:
        base = os.path.basename(path)
        if path.endswith(".a"):
            units = [Unit(base, m, d) for m, d in read_archive(path)]
        else:
            units = [Unit("-", base, open(path, "rb").read())]
        inputs.append((path, units))
    return inputs


def why_fixed(u, definitions):
    """The reason a unit cannot be banked, or None."""
    if u.foreign_code:
        return "has code outside .text*"
    dup = [n for n in u.defined if len(definitions[n]) > 1]
    if dup:
        return "%s defined more than once" % dup[0]
    return None


def apply_sizes(units, sizes):
    """Keep only the code a flat link keeps, with its sizes."""
    for u in units:
        if sizes is None:
            continue
        kept = sizes.get(u.key, {})
        names = {i: u.elf.sections[i]["name"] for i in u.code}
        u.code = {i: kept[n] for i, n in names.items() if kept.get(n)}


def place(u, bank, used):
    for sec in u.code:
        u.secbank[sec] = bank
    used[bank] += u.size


def place_split(u, used, capacity, log):
    """Place the code sections of u one by one (first fit, largest first);
    what does not fit stays fixed."""
    u.split = True
    for sec, size in sorted(u.code.items(), key=lambda kv: -kv[1]):
        for b in range(len(used)):
            if used[b] + size <= capacity:
                u.secbank[sec] = b
                used[b] += size
                break
        else:
            log.append(
                "fixed: %s %s (%d bytes: no bank has room)"
                % (u.key, u.elf.sections[sec]["name"], size)
            )


def assign_banks(units, rules, sizes, args, log):
    """Place every unit's code sections; return the bytes used per bank."""
    apply_sizes(units, sizes)
    definitions = {}
    for u in units:
        for name in u.defined:
            definitions.setdefault(name, []).append(u)
    used = [0] * args.banks
    autos = []
    hots = []
    for u in units:
        where = match_rule(rules, u)
        if where == "fixed" or u.size == 0:
            continue  # fixed by rule, no code, or not linked
        u.why_fixed = why_fixed(u, definitions)
        if u.why_fixed:
            log.append("fixed: %s (%s)" % (u.key, u.why_fixed))
        elif where == "hot":
            hots.append(u)
        elif where == "auto":
            autos.append(u)
        elif not 0 <= where < args.banks:
            raise SystemExit("%s: bank %d out of range" % (u.key, where))
        else:
            place(u, where, used)
    if hots:
        if args.map is None:
            raise SystemExit("'hot' rules need --map")
        # Fixed ROM if every hot object stayed there: everything except the
        # code that will be banked, plus at most one stub per global
        # function of a banked object.
        banked = [u for u in autos] + [u for u in units if u.secbank]
        stub_bytes = 6 * sum(len(u.defined) for u in banked)
        room = (
            args.window
            - map_rom_total(args.map)
            + sum(u.size for u in banked)
            - stub_bytes
            - args.fixed_reserve
        )
        for u in sorted(hots, key=lambda u: match_rule_index(rules, u)[1]):
            if u.size <= room:
                room -= u.size
                log.append("hot, fixed: %s (%d bytes)" % (u.key, u.size))
            else:
                autos.append(u)
                log.append(
                    "hot, banked: %s (%d bytes, fixed ROM full)" % (u.key, u.size)
                )
    capacity = args.bank_size - args.reserve
    split = []
    for u in sorted(autos, key=lambda u: -u.size):
        b = next((b for b in range(args.banks) if used[b] + u.size <= capacity), None)
        if b is None:
            split.append(u)  # too big for any bank's free space as a whole
        else:
            place(u, b, used)
    for u in split:
        place_split(u, used, capacity, log)
    return used


# Output #####################################################################


def plan_references(u, bank_of):
    """Decide how each reference of a banked unit reaches its target.

    Returns (renamed, stubbed): renamed is the set of names the object
    refers to directly under __bank_<name> (its banked definitions and the
    banked functions it calls from the same bank); stubbed lists the
    references that must go through a stub although the name they use is
    renamed in this object, or that point at a static function.
    """
    renamed = {n for n, (b, sec) in u.defined.items() if sec in u.secbank}
    decisions = []
    for r in u.refs:
        tb = u.secbank.get(r.tsec) if r.tsec is not None else bank_of.get(r.name)
        if tb is None:
            continue  # fixed target: always direct
        fb = u.secbank.get(r.sec) if u.elf.is_code(r.sec) else "data"
        direct = r.call and fb == tb
        if direct and r.name is not None:
            renamed.add(r.name)
        decisions.append((r, direct, tb))
    stubbed = [
        (r, tb)
        for r, direct, tb in decisions
        if not direct and (r.name is None or r.name in renamed)
    ]
    return renamed, stubbed


def rewrite_references(u, f, stubbed, counter):
    """Point the references in stubbed at stubs: a global function's own
    stub (via a placeholder renamed back to its name, so that pointers
    compare equal everywhere) or a generated one for a static function.
    Rewrites file f; returns the placeholder renames and extra stubs."""
    new_symbols, redirects, renames, stubs = [], {}, [], []
    targets = {}
    for r, tb in stubbed:
        key = (r.tsec, r.toff) if r.name is None else r.name
        if key not in targets:
            if r.name is not None:
                ref = "__cosmac_ref_%s" % r.name
                renames.append((ref, r.name))
            else:
                counter[0] += 1
                fn = "__cosmac_fn_%d" % counter[0]
                ref = "__cosmac_ptr_%d" % counter[0]
                new_symbols.append((fn, r.toff, r.tsec, True))
                stubs.append((ref, fn, STB_GLOBAL, tb))
            targets[key] = len(new_symbols)
            new_symbols.append((ref, 0, SHN_UNDEF, False))
        redirects[(r.rela, r.entry)] = (targets[key], 0)
    if redirects:
        open(f, "wb").write(u.elf.rewrite(new_symbols, redirects))
    return renames, stubs


def bank_object(u, f, bank_of, objcopy, counter):
    """Write the banked form of object f; return the stubs it needs."""
    renamed, stubbed = plan_references(u, bank_of)
    renames, stubs = rewrite_references(u, f, stubbed, counter)
    symfile = f + ".syms"
    with open(symfile, "w") as sf:
        for n in sorted(renamed):
            sf.write("%s %s%s\n" % (n, PREFIX, n))
        for old_name, new_name in renames:
            sf.write("%s %s\n" % (old_name, new_name))
    cmd = [objcopy, "--redefine-syms", symfile]
    for sec, bank in sorted(u.secbank.items()):
        n = u.elf.sections[sec]["name"]
        cmd += ["--rename-section", "%s=.bank%d%s" % (n, bank, n)]
    subprocess.check_call(cmd + [f])
    for n, (bind, sec) in sorted(u.defined.items()):
        if sec in u.secbank:
            stubs.append((n, PREFIX + n, bind, u.secbank[sec]))
    u.redirected = len(stubbed)
    return stubs


def write_libraries(inputs, units, args):
    """Write the (partly banked) copies of the inputs; return the stubs."""
    bank_of = {}
    for u in units:
        for name, (bind, sec) in u.defined.items():
            if sec in u.secbank:
                bank_of[name] = u.secbank[sec]
    if os.path.isdir(args.outdir):
        shutil.rmtree(args.outdir)
    os.makedirs(args.outdir)
    stubs = []
    counter = [0]
    for path, us in inputs:
        base = os.path.basename(path)
        work = os.path.join(args.outdir, base + ".d")
        files = []
        for i, u in enumerate(us):
            d = os.path.join(work, str(i))
            os.makedirs(d)
            f = os.path.join(d, u.member)
            open(f, "wb").write(u.elf.data)
            if u.banked:
                stubs += bank_object(u, f, bank_of, args.objcopy, counter)
            files.append(f)
        out = os.path.join(args.outdir, base)
        if path.endswith(".a"):
            subprocess.check_call([args.ar, "qc", out] + files)
            subprocess.check_call([args.ar, "s", out])
        else:
            shutil.copy(files[0], out)
    return stubs


def write_stubs(path, stubs):
    with open(path, "w") as f:
        f.write(
            "/* Generated by tools/cosmac/cosmac_bank.py: far-call "
            "stubs (do not edit) */\n\n"
        )
        for name, target, bind, bank in stubs:
            f.write('\t.section .text.stub.%s, "ax"\n' % name)
            f.write("\t.%s\t%s\n" % ("weak" if bind == STB_WEAK else "global", name))
            f.write("\t.type\t%s, @function\n" % name)
            f.write("%s:\n\tsep\t4\n\t.hword\t__cosmac_farcall\n" % name)
            f.write("\t.hword\t%s\n\t.byte\t%d\n" % (target, bank))
            f.write("\t.size\t%s, .-%s\n\n" % (name, name))


def write_ldscript(path, args):
    """The OVERLAY statement for the banks.  Every bank is padded to its
    full size, so the ROM image is the fixed part followed by bank 0, 1,
    ...  NOCROSSREFS makes the linker reject any direct reference between
    two banks.  __cosmac_banks tells the board script the bank count."""
    with open(path, "w") as f:
        f.write("/* Generated by tools/cosmac/cosmac_bank.py */\n\n")
        f.write(
            "OVERLAY 0x%04x : NOCROSSREFS AT (0x%04x)\n{\n" % (args.window, args.window)
        )
        for b in range(args.banks):
            f.write(
                "  .bank%d { *(.bank%d.text .bank%d.text.*) . = 0x%x; }\n"
                % (b, b, b, args.bank_size)
            )
        f.write("}\n\n__cosmac_banks = %d;\n" % args.banks)


def report(units, used, stubs, log, args):
    lines = [
        "cosmac_bank: %d banks of %d bytes (auto placement leaves %d free)"
        % (args.banks, args.bank_size, args.reserve)
    ]
    for b in range(args.banks):
        members = []
        for u in units:
            size = sum(n for sec, n in u.code.items() if u.secbank.get(sec) == b)
            if size:
                members.append((size, u.key + (" (split)" if u.split else "")))
        lines.append(
            "bank %d: %d bytes (estimate), %d objects" % (b, used[b], len(members))
        )
        for size, key in sorted(members, reverse=True):
            lines.append("    %6d  %s" % (size, key))
    lines.append("stubs: %d (%d bytes if all are kept)" % (len(stubs), 6 * len(stubs)))
    redirected = [u for u in units if getattr(u, "redirected", 0)]
    lines.append(
        "references rewritten to go through stubs: %d in %d objects"
        % (sum(u.redirected for u in redirected), len(redirected))
    )
    return "\n".join(lines + log) + "\n"


# Main #######################################################################


def parse_args():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--rules", required=True)
    ap.add_argument("--banks", type=int, required=True)
    ap.add_argument("--bank-size", type=int, default=0x4000)
    ap.add_argument(
        "--reserve",
        type=int,
        default=256,
        help="bytes kept free in each bank for auto placement",
    )
    ap.add_argument("--map", help="map of a flat link, for object sizes")
    ap.add_argument(
        "--fixed-reserve",
        type=int,
        default=1024,
        help="bytes of fixed ROM kept free when placing 'hot' objects",
    )
    ap.add_argument("--outdir", required=True)
    ap.add_argument("--stubs", required=True)
    ap.add_argument("--report")
    ap.add_argument("--ldscript", help="write the OVERLAY statement here")
    ap.add_argument("--window", type=lambda x: int(x, 0), default=0x8000)
    ap.add_argument("--objcopy", default="objcopy")
    ap.add_argument("--ar", default="ar")
    ap.add_argument("inputs", nargs="+")
    return ap.parse_args()


def main():
    args = parse_args()
    rules = parse_rules(args.rules)
    sizes = map_sizes(args.map) if args.map else None
    inputs = read_inputs(args.inputs)
    units = [u for _, us in inputs for u in us]
    log = []
    used = assign_banks(units, rules, sizes, args, log)
    stubs = write_libraries(inputs, units, args)
    write_stubs(args.stubs, stubs)
    if args.ldscript:
        write_ldscript(args.ldscript, args)
    text = report(units, used, stubs, log, args)
    if args.report:
        open(args.report, "w").write(text)
    else:
        sys.stdout.write(text)


if __name__ == "__main__":
    main()
