#!/usr/bin/env python3
"""APT ActionScript disassembler / decoder validator.

    aptdis.py <dir> <Movie>            disassemble every action block of a movie
    aptdis.py <dir> --check            decode every block of every movie; report problems

Opcode table: AVM1 + EA extensions (operand layouts as documented by OpenSAGE, GPLv3).
'A' = 4-byte aligned operand block (alignment relative to the .apt file).
"""
import os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import aptview

NAMES = {
    0x00: 'End', 0x04: 'NextFrame', 0x05: 'PrevFrame', 0x06: 'Play', 0x07: 'Stop', 0x08: 'ToggleQuality', 0x09: 'StopSounds',
    0x0A: 'Add', 0x0B: 'Subtract', 0x0C: 'Multiply', 0x0D: 'Divide', 0x0E: 'Equals', 0x0F: 'Less', 0x10: 'And', 0x11: 'Or',
    0x12: 'Not', 0x13: 'StringEquals', 0x14: 'StringLength', 0x15: 'SubString', 0x17: 'Pop', 0x18: 'ToInteger',
    0x1C: 'GetVariable', 0x1D: 'SetVariable', 0x20: 'SetTarget2', 0x21: 'StringAdd', 0x22: 'GetProperty', 0x23: 'SetProperty',
    0x24: 'CloneSprite', 0x25: 'RemoveSprite', 0x26: 'Trace', 0x27: 'StartDrag', 0x28: 'EndDrag', 0x29: 'StringLess',
    0x2A: 'Throw', 0x2B: 'CastOp', 0x2C: 'ImplementsOp', 0x30: 'Random', 0x31: 'MBStringLength', 0x32: 'Ord', 0x33: 'Chr',
    0x34: 'GetTime', 0x35: 'MBSubString', 0x36: 'MBOrd', 0x37: 'MBChr', 0x3A: 'Delete', 0x3B: 'Delete2', 0x3C: 'DefineLocal',
    0x3D: 'CallFunction', 0x3E: 'Return', 0x3F: 'Modulo', 0x40: 'NewObject', 0x41: 'DefineLocal2', 0x42: 'InitArray',
    0x43: 'InitObject', 0x44: 'TypeOf', 0x45: 'TargetPath', 0x46: 'Enumerate', 0x47: 'Add2', 0x48: 'Less2', 0x49: 'Equals2',
    0x4A: 'ToNumber', 0x4B: 'ToString', 0x4C: 'PushDuplicate', 0x4D: 'StackSwap', 0x4E: 'GetMember', 0x4F: 'SetMember',
    0x50: 'Increment', 0x51: 'Decrement', 0x52: 'CallMethod', 0x53: 'NewMethod', 0x54: 'InstanceOf', 0x55: 'Enumerate2',
    0x56: 'EA_PushThis', 0x58: 'EA_PushGlobal', 0x59: 'EA_PushZero', 0x5A: 'EA_PushOne', 0x5B: 'EA_CallFuncPop',
    0x5C: 'EA_CallFunc', 0x5D: 'EA_CallMethodPop', 0x5E: 'EA_CallMethod', 0x60: 'BitAnd', 0x61: 'BitOr', 0x62: 'BitXor',
    0x63: 'BitLShift', 0x64: 'BitRShift', 0x65: 'BitURShift', 0x66: 'StrictEquals', 0x67: 'Greater', 0x68: 'StringGreater',
    0x69: 'Extends', 0x70: 'EA_PushThisVar', 0x71: 'EA_PushGlobalVar', 0x72: 'EA_ZeroVar', 0x73: 'EA_PushTrue',
    0x74: 'EA_PushFalse', 0x75: 'EA_PushNull', 0x76: 'EA_PushUndefined', 0x77: 'TraceStart',
    0x81: 'GotoFrame', 0x83: 'GetURL', 0x87: 'SetRegister', 0x88: 'ConstantPool', 0x8A: 'WaitForFrame', 0x8B: 'SetTarget',
    0x8C: 'GotoLabel', 0x8D: 'WaitForFrame2', 0x8E: 'DefineFunction2', 0x8F: 'Try', 0x94: 'With', 0x96: 'PushData',
    0x99: 'BranchAlways', 0x9A: 'GetURL2', 0x9B: 'DefineFunction', 0x9D: 'BranchIfTrue', 0x9E: 'CallFrame', 0x9F: 'GotoFrame2',
    0xA1: 'EA_PushString', 0xA2: 'EA_PushConstantByte', 0xA3: 'EA_PushConstantWord', 0xA4: 'EA_GetStringVar',
    0xA5: 'EA_GetStringMember', 0xA6: 'EA_SetStringVar', 0xA7: 'EA_SetStringMember', 0xAE: 'EA_PushValueOfVar',
    0xAF: 'EA_GetNamedMember', 0xB0: 'EA_CallNamedFuncPop', 0xB1: 'EA_CallNamedFunc', 0xB2: 'EA_CallNamedMethodPop',
    0xB3: 'EA_CallNamedMethod', 0xB4: 'EA_PushFloat', 0xB5: 'EA_PushByte', 0xB6: 'EA_PushShort', 0xB7: 'EA_PushLong',
    0xB8: 'EA_BranchIfFalse', 0xB9: 'EA_PushRegister',
}
# operand layout: (aligned, kind)
OPS = {
    0x81: (True, 'i32'), 0x83: (True, 'str2'), 0x87: (True, 'i32'), 0x88: (True, 'consts'), 0x8B: (True, 'str'),
    0x8C: (True, 'str'), 0x8E: (True, 'func2'), 0x94: (True, 'i32'), 0x96: (True, 'consts'), 0x99: (True, 'branch'),
    0x9B: (True, 'func'), 0x9D: (True, 'branch'), 0x9F: (True, 'i32'), 0xA1: (True, 'str'), 0xA2: (False, 'u8'),
    0xA3: (False, 'u16'), 0xA4: (True, 'str'), 0xA5: (True, 'str'), 0xA6: (True, 'str'), 0xA7: (True, 'str'),
    0xAE: (False, 'u8'), 0xAF: (False, 'u8'), 0xB0: (False, 'u8'), 0xB1: (False, 'u8'), 0xB2: (False, 'u8'), 0xB3: (False, 'u8'),
    0xB4: (False, 'f32'), 0xB5: (False, 'u8'), 0xB6: (False, 'u16'), 0xB7: (False, 'u32'), 0xB8: (True, 'branch'),
    0xB9: (False, 'u8'),
}


def decode(f, start, end=None, out=None):
    """decode instructions from start; returns (instructions, problems). Follows the AVM1 rule that a block
    ends at End unless a forward branch jumps past it."""
    d = f.r.d
    pc = start
    insts, problems = [], []
    furthest = start
    while True:
        if end is not None and pc >= end:
            break
        if pc >= len(d):
            problems.append('ran off the file at %#x' % pc); break
        op = d[pc]; at = pc; pc += 1
        if op not in NAMES:
            problems.append('unknown opcode %#04x at %#x' % (op, at)); break
        arg = None
        if op in OPS:
            aligned, kind = OPS[op]
            if aligned: pc = (pc + 3) & ~3
            if kind in ('i32', 'branch'):
                arg = struct.unpack_from('<i', d, pc)[0]; pc += 4
                if kind == 'branch': furthest = max(furthest, pc + arg)
            elif kind == 'u32': arg = struct.unpack_from('<I', d, pc)[0]; pc += 4
            elif kind == 'u8': arg = d[pc]; pc += 1
            elif kind == 'u16': arg = struct.unpack_from('<H', d, pc)[0]; pc += 2
            elif kind == 'f32': arg = struct.unpack_from('<f', d, pc)[0]; pc += 4
            elif kind == 'str': arg = f.r.cstr(f.r.u32(pc)); pc += 4
            elif kind == 'str2': arg = (f.r.cstr(f.r.u32(pc)), f.r.cstr(f.r.u32(pc + 4))); pc += 8
            elif kind == 'consts':
                n, lo = f.r.u32(pc), f.r.u32(pc + 4); pc += 8
                arg = [f.r.u32(lo + 4 * i) for i in range(n)]
                for c in arg:
                    if c >= len(f.consts): problems.append('const %d out of range at %#x' % (c, at))
            elif kind == 'func2':
                name = f.r.cstr(f.r.u32(pc)); np_ = f.r.u32(pc + 4); nreg = d[pc + 8]
                flags = d[pc + 9] | (d[pc + 10] << 8) | (d[pc + 11] << 16)
                plist = f.r.u32(pc + 12)
                params = [(f.r.i32(plist + 8 * i), f.r.cstr(f.r.u32(plist + 8 * i + 4))) for i in range(np_)]
                size = f.r.i32(pc + 16); pc += 28
                arg = (name, params, nreg, flags, size)
                insts.append((at, op, arg))
                sub, p2 = decode(f, pc, pc + size)
                problems += p2
                if out is not None: out.append(('function2 %s' % name, pc, sub))
                pc += size
                continue
            elif kind == 'func':
                name = f.r.cstr(f.r.u32(pc)); np_, plist = f.r.u32(pc + 4), f.r.u32(pc + 8)
                params = [f.r.cstr(f.r.u32(plist + 4 * i)) for i in range(np_)]
                size = f.r.i32(pc + 12); pc += 24
                arg = (name, params, size)
                insts.append((at, op, arg))
                sub, p2 = decode(f, pc, pc + size)
                problems += p2
                if out is not None: out.append(('function %s' % name, pc, sub))
                pc += size
                continue
        insts.append((at, op, arg))
        if op == 0x00 and end is None and pc > furthest:
            break
    # branch targets must land on instruction starts
    starts = {i[0] for i in insts}
    for at, op, arg in insts:
        if op in (0x99, 0x9D, 0xB8):
            aligned_after = ((at + 1 + 3) & ~3) + 4
            tgt = aligned_after + arg
            if tgt not in starts and (end is None or tgt != end) and tgt != (insts[-1][0] + 1):
                problems.append('branch at %#x to %#x is not an instruction start' % (at, tgt))
    return insts, problems


def blocks(apt):
    """all action entry points of a movie"""
    for fi, frame in enumerate(apt.movie['frames']):
        for it in frame:
            if it and it['kind'] in ('action', 'initaction'):
                yield ('movie frame %d %s' % (fi, it['kind']), it['code'])
    for idx, c in enumerate(apt.movie['chars']):
        if c is None or c['apt'] is not apt: continue
        if c['type'] in (aptview.MOVIE, aptview.SPRITE):
            for fi, frame in enumerate(c['frames']):
                for it in frame:
                    if it and it['kind'] in ('action', 'initaction'):
                        yield ('char %d frame %d %s' % (idx, fi, it['kind']), it['code'])
        if c['type'] == aptview.BUTTON:
            r = apt.r
            p = c['off'] + 8
            na, la = r.u32(p + 44), r.u32(p + 48)
            for i in range(na):
                yield ('button %d action %d' % (idx, i), r.u32(la + 8 * i + 4))


def fmt(f, inst):
    at, op, arg = inst
    s = '%06x  %-22s' % (at, NAMES.get(op, '?%02x' % op))
    if arg is None: return s
    if op in (0x88, 0x96):
        return s + ' ' + ', '.join(repr(f.consts[c]) if c < len(f.consts) else '?' for c in arg)
    return s + ' ' + repr(arg)


def main():
    folder = sys.argv[1]
    if sys.argv[2] == '--check':
        total = bad = 0
        from collections import Counter
        ops = Counter()
        for fn in sorted(os.listdir(folder)):
            if not fn.lower().endswith('.apt'): continue
            apt = aptview.Apt.load(folder, fn[:-4])
            for label, off in blocks(apt):
                total += 1
                insts, problems = decode(apt, off)
                for i in insts: ops[i[1]] += 1
                if problems:
                    bad += 1
                    if bad <= 20: print(fn, label, problems[:3])
        print('%d blocks, %d with problems' % (total, bad))
        print('opcodes used:', ' '.join('%s:%d' % (NAMES[o], n) for o, n in sorted(ops.items())))
        return
    apt = aptview.Apt.load(folder, sys.argv[2])
    for label, off in blocks(apt):
        funcs = []
        insts, problems = decode(apt, off, out=funcs)
        print('== %s @%#x' % (label, off), problems if problems else '')
        for i in insts: print('   ' + fmt(apt, i))
        for name, at, sub in funcs:
            print('   -- %s @%#x' % (name, at))
            for i in sub: print('      ' + fmt(apt, i))


if __name__ == '__main__':
    main()
