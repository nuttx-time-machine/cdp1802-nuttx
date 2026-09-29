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

An object stays in fixed ROM when banking it would be unsafe: it takes the
address of one of its own functions (the pointer would bypass the stub), it
has code outside .text*, or it defines a symbol that is also defined
elsewhere.  A reference is a *call* when it is the 16-bit operand of
"sep 4" (the NCRT call) or of a long branch (a tail call); every other
reference to a function is an address.

Placement rules (first match wins; unmatched objects stay fixed):
    <bank>|auto|fixed   <archive-glob>:<member-glob>
Plain objects match as "-:<file name>".  "auto" packs objects, largest
first, into the first bank with room.  Object sizes come from the map of a
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


class Unit:
    """One object file of the link (an archive member or a plain object)."""

    def __init__(self, archive, member, data):
        self.archive = archive  # archive basename, or "-" for plain objects
        self.member = member
        self.elf = ElfObject(data, "%s:%s" % (archive, member))
        self.bank = None  # None = fixed
        self.why_fixed = None
        self.size = 0
        self.defined = {}  # global function name -> bind
        self.calls = set()  # names called
        self.addresses = set()  # names whose address is taken
        self.own_address = False  # takes the address of its own function
        self.foreign_code = False  # code outside .text*
        self._analyse()

    @property
    def key(self):
        return "%s:%s" % (self.archive, self.member)

    def _analyse(self):
        e = self.elf
        starts = {}  # code section index -> {function start offsets}
        for sym in e.symbols:
            if sym["shndx"] == SHN_UNDEF or sym["shndx"] >= 0xFF00:
                continue
            if not e.is_code(sym["shndx"]):
                continue
            if sym["type"] in (STT_FUNC, STT_NOTYPE) and sym["name"]:
                starts.setdefault(sym["shndx"], set()).add(sym["value"])
                if sym["bind"] in (STB_GLOBAL, STB_WEAK):
                    self.defined[sym["name"]] = sym["bind"]
        for i, s in enumerate(e.sections):
            if e.is_code(i) and s["size"]:
                self.size += s["size"]
                if not (s["name"] == ".text" or s["name"].startswith(".text.")):
                    self.foreign_code = True
        for sec, off, typ, sym, addend in e.relocations():
            call = (
                typ == R_CDP1802_16
                and e.is_code(sec)
                and off > 0
                and e.byte_at(sec, off - 1) in CALL_OPCODES
            )
            if sym["shndx"] == SHN_UNDEF:
                if sym["name"]:
                    (self.calls if call else self.addresses).add(sym["name"])
                continue
            if sym["shndx"] >= 0xFF00 or not e.is_code(sym["shndx"]):
                continue
            # A reference into this object's own code
            if sym["type"] == STT_SECTION:
                target = addend
            else:
                target = sym["value"] + addend
            if call:
                if sym["type"] != STT_SECTION and sym["bind"] != STB_LOCAL:
                    self.calls.add(sym["name"])
            elif target == 0 or target in starts.get(sym["shndx"], ()):
                self.own_address = True


def parse_rules(path):
    rules = []
    for n, line in enumerate(open(path), 1):
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        fields = line.split()
        if len(fields) != 2 or ":" not in fields[1]:
            raise SystemExit(
                "%s:%d: expected '<bank>|auto|fixed <archive>:<member>'" % (path, n)
            )
        where, pattern = fields
        if where not in ("auto", "fixed"):
            where = int(where)
        arch, member = pattern.split(":", 1)
        rules.append((where, arch, member))
    return rules


def match_rule(rules, unit):
    for where, arch, member in rules:
        if fnmatch.fnmatchcase(unit.archive, arch) and fnmatch.fnmatchcase(
            unit.member, member
        ):
            return where
    return "fixed"


MAP_SECTION = re.compile(r"^ (\.text\S*)\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S.*)$")
MAP_NAME_ONLY = re.compile(r"^ (\.text\S*)$")
MAP_CONT = re.compile(r"^\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S.*)$")


def map_sizes(path):
    """Code bytes per archive member / object kept in a GNU ld map."""
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
            size, where = int(m.group(3), 16), m.group(4)
        elif pending and MAP_CONT.match(line):
            m = MAP_CONT.match(line)
            size, where = int(m.group(2), 16), m.group(3)
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
        sizes[key] = sizes.get(key, 0) + size
    return sizes


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
    if u.own_address:
        return "takes the address of its own function"
    if u.foreign_code:
        return "has code outside .text*"
    dup = [n for n in u.defined if len(definitions[n]) > 1]
    if dup:
        return "%s defined more than once" % dup[0]
    return None


def assign_banks(units, rules, sizes, args, log):
    """Set u.bank for every unit; return the bytes used per bank."""
    definitions = {}
    for u in units:
        for name in u.defined:
            definitions.setdefault(name, []).append(u)
    used = [0] * args.banks
    autos = []
    for u in units:
        if sizes is not None:
            u.size = sizes.get(u.key, 0)
        where = match_rule(rules, u)
        if where == "fixed" or u.size == 0:
            continue  # fixed by rule, no code, or not linked
        u.why_fixed = why_fixed(u, definitions)
        if u.why_fixed:
            log.append("fixed: %s (%s)" % (u.key, u.why_fixed))
        elif where == "auto":
            autos.append(u)
        elif not 0 <= where < args.banks:
            raise SystemExit("%s: bank %d out of range" % (u.key, where))
        else:
            u.bank = where
            used[where] += u.size
    capacity = args.bank_size - args.reserve
    for u in sorted(autos, key=lambda u: -u.size):
        for b in range(args.banks):
            if used[b] + u.size <= capacity:
                u.bank = b
                used[b] += u.size
                break
        else:
            log.append("fixed: %s (%d bytes: no bank has room)" % (u.key, u.size))
    return used


# Output #####################################################################


def bank_object(u, f, bank_of, objcopy):
    """Rename the code sections and the same-bank functions of object f."""
    rename = sorted(
        n
        for n in (set(u.defined) | u.calls)
        if bank_of.get(n) == u.bank and n not in u.addresses
    )
    symfile = f + ".syms"
    with open(symfile, "w") as sf:
        for n in rename:
            sf.write("%s %s%s\n" % (n, PREFIX, n))
    cmd = [objcopy, "--redefine-syms", symfile]
    for s in u.elf.sections:
        n = s["name"]
        if n == ".text" or n.startswith(".text."):
            cmd += ["--rename-section", "%s=.bank%d%s" % (n, u.bank, n)]
    subprocess.check_call(cmd + [f])


def write_libraries(inputs, units, args):
    """Write the (partly banked) copies of the inputs; return the stubs."""
    bank_of = {}
    for u in units:
        if u.bank is not None:
            for name in u.defined:
                bank_of[name] = u.bank
    if os.path.isdir(args.outdir):
        shutil.rmtree(args.outdir)
    os.makedirs(args.outdir)
    stubs = []
    for path, us in inputs:
        base = os.path.basename(path)
        work = os.path.join(args.outdir, base + ".d")
        files = []
        for i, u in enumerate(us):
            d = os.path.join(work, str(i))
            os.makedirs(d)
            f = os.path.join(d, u.member)
            open(f, "wb").write(u.elf.data)
            if u.bank is not None:
                bank_object(u, f, bank_of, args.objcopy)
                stubs += [(n, b, u.bank) for n, b in sorted(u.defined.items())]
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
        for name, bind, bank in stubs:
            f.write('\t.section .text.stub.%s, "ax"\n' % name)
            f.write("\t.%s\t%s\n" % ("weak" if bind == STB_WEAK else "global", name))
            f.write("\t.type\t%s, @function\n" % name)
            f.write("%s:\n\tsep\t4\n\t.hword\t__cosmac_farcall\n" % name)
            f.write("\t.hword\t%s%s\n\t.byte\t%d\n" % (PREFIX, name, bank))
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
        members = [u for u in units if u.bank == b]
        lines.append(
            "bank %d: %d bytes (estimate), %d objects" % (b, used[b], len(members))
        )
        for u in sorted(members, key=lambda u: -u.size):
            lines.append("    %6d  %s" % (u.size, u.key))
    lines.append("stubs: %d (%d bytes if all are kept)" % (len(stubs), 6 * len(stubs)))
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
