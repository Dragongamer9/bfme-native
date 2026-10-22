#!/usr/bin/env python3
"""Parse a SAGE APT movie (.apt/.const/.dat/_geometry/*.ru) and render frames to PNG.

Format notes: OpenSAGE (GPLv3) Data/Apt. Pointers are file offsets in the .apt; lists are
(count, offset); character lists hold pointers. Matrices are 2x2 (M11 M12 M21 M22) + translation,
row-vector convention: x' = x*M11 + y*M21 + tx, y' = x*M12 + y*M22 + ty.

    aptview.py <dir> <Movie> [--frame N] [--out file.png] [--dump]
"""
import argparse, os, struct, re, sys

SIG = 0x09876543
SHAPE, TEXT, FONT, BUTTON, SPRITE, SOUND, IMAGE, MORPH, MOVIE, STATICTEXT, NONE, VIDEO = range(1, 13)
CHAR_NAMES = {1: 'Shape', 2: 'Text', 3: 'Font', 4: 'Button', 5: 'Sprite', 7: 'Image', 8: 'Morph', 9: 'Movie', 10: 'StaticText'}


class R:
    def __init__(self, data):
        self.d = data
    def u32(self, o): return struct.unpack_from('<I', self.d, o)[0]
    def i32(self, o): return struct.unpack_from('<i', self.d, o)[0]
    def f32(self, o): return struct.unpack_from('<f', self.d, o)[0]
    def cstr(self, o):
        if o == 0: return None
        e = self.d.index(b'\0', o)
        return self.d[o:e].decode('latin1')


class Apt:
    def __init__(self, folder, name):
        self.folder, self.name = folder, name
        self.r = R(open(os.path.join(folder, name + '.apt'), 'rb').read())
        c = open(os.path.join(folder, name + '.const'), 'rb').read()
        assert c[:17] == b'Apt constant file'
        self.entry, n, hdr = struct.unpack_from('<3I', c, 20)
        self.consts = []
        for i in range(n):
            t, v = struct.unpack_from('<II', c, 32 + 8 * i)
            if t == 1:
                self.consts.append(c[v:c.index(b'\0', v)].decode('latin1'))
            else:
                self.consts.append((t, v))
        self.imagemap = {}
        dat = os.path.join(folder, name + '.dat')
        if os.path.exists(dat):
            for line in open(dat, errors='ignore'):
                line = line.split(';')[0].strip()
                if not line: continue
                img, rest = re.split(r'->|=', line, maxsplit=1)
                parts = rest.replace('(', ' ').replace(')', ' ').split()
                if len(parts) == 1:
                    self.imagemap[int(img)] = (int(parts[0]), None)
                else:
                    self.imagemap[int(img)] = (int(img), tuple(int(x) for x in parts[:4]))
        self.movie = self.char(self.entry)
        self.geom = {}
        Apt.cache[name.lower()] = self
        for mov, nm, ch in self.movie['imports']:
            try:
                other = Apt.load(folder, mov)
            except FileNotFoundError:
                continue
            for en, ech in other.movie['exports']:
                if en == nm and 0 <= ch < len(self.movie['chars']):
                    self.movie['chars'][ch] = other.movie['chars'][ech]

    cache = {}

    @staticmethod
    def load(folder, name):
        key = name.lower()
        if key in Apt.cache:
            return Apt.cache[key]
        real = None
        for fn in os.listdir(folder):
            if fn.lower() == key + '.apt':
                real = fn[:-4]
        if real is None:
            raise FileNotFoundError(name)
        return Apt(folder, real)

    def lst(self, o, item, ptr=False):
        n, off = self.r.u32(o), self.r.u32(o + 4)
        out = []
        for i in range(n):
            if ptr:
                p = self.r.u32(off + 4 * i)
                out.append(item(p) if p else None)
            else:
                out.append(item(off, i))
        return out

    def char(self, o):
        r = self.r
        t, sig = r.u32(o), r.u32(o + 4)
        assert sig == SIG, hex(sig)
        p = o + 8
        c = {'type': t, 'off': o, 'apt': self}
        if t == MOVIE:
            if getattr(self, '_movie_seen', False):
                return None	# character 0 points back at the movie
            self._movie_seen = True
        if t in (MOVIE, SPRITE):
            c['frames'] = self.lst(p, lambda off, i: self.frame(off + 8 * i))
            if t == MOVIE:
                c['chars'] = self.lst(p + 12, self.char, ptr=True)
                c['w'], c['h'], c['msPerFrame'] = r.u32(p + 20), r.u32(p + 24), r.u32(p + 28)
                c['imports'] = self.lst(p + 32, lambda off, i: (r.cstr(r.u32(off + 16 * i)), r.cstr(r.u32(off + 16 * i + 4)), r.u32(off + 16 * i + 8)))
                c['exports'] = self.lst(p + 40, lambda off, i: (r.cstr(r.u32(off + 8 * i)), r.u32(off + 8 * i + 4)))
        elif t == SHAPE:
            c['bounds'] = struct.unpack_from('<4f', r.d, p)
            c['geometry'] = r.u32(p + 16)
        elif t == IMAGE:
            c['texture'] = r.u32(p)
        elif t == TEXT:
            c['bounds'] = struct.unpack_from('<4f', r.d, p)
            c['font'], c['align'] = r.u32(p + 16), r.u32(p + 20)
            c['color'] = tuple(r.d[p + 24:p + 28])
            c['height'] = r.f32(p + 28)
            c['content'] = r.cstr(r.u32(p + 44))
            c['variable'] = r.cstr(r.u32(p + 48))
        elif t == FONT:
            c['name'] = r.cstr(r.u32(p))
        elif t == BUTTON:
            c['bounds'] = struct.unpack_from('<4f', r.d, p + 4)
            c['records'] = self.lst(p + 36, lambda off, i: self.button_record(off + 68 * i))
        return c

    def button_record(self, o):
        r = self.r
        return {'flags': r.d[o], 'char': r.u32(o + 4), 'depth': r.i32(o + 8),
                'm': struct.unpack_from('<4f', r.d, o + 12), 't': struct.unpack_from('<2f', r.d, o + 28)}

    def frame(self, o):
        return self.lst(o, self.item, ptr=True)

    def item(self, o):
        r = self.r
        t = r.u32(o)
        p = o + 4
        if t == 3:  # PlaceObject
            flags = r.u32(p)
            it = {'kind': 'place', 'flags': flags, 'depth': r.i32(p + 4)}
            if flags & 2: it['char'] = r.i32(p + 8)
            if flags & 4:
                it['m'] = struct.unpack_from('<4f', r.d, p + 12)
                it['t'] = struct.unpack_from('<2f', r.d, p + 28)
            if flags & 8:
                it['tint'] = tuple(r.d[p + 36:p + 40])
                it['add'] = tuple(r.d[p + 40:p + 44])
            if flags & 32: it['name'] = r.cstr(r.u32(p + 48))
            return it
        if t == 4: return {'kind': 'remove', 'depth': r.i32(p)}
        if t == 2: return {'kind': 'label', 'name': r.cstr(r.u32(p))}
        if t == 1: return {'kind': 'action', 'code': r.u32(p)}
        if t == 8: return {'kind': 'initaction', 'sprite': r.u32(p), 'code': r.u32(p + 4)}
        if t == 5: return {'kind': 'bgcolor'}
        return {'kind': 'unknown', 'type': t}

    def geometry(self, gid):
        if gid in self.geom: return self.geom[gid]
        path = os.path.join(self.folder, self.name + '_geometry', '%d.ru' % gid)
        entries, cur = [], None
        if os.path.exists(path):
            for line in open(path):
                line = line.strip()
                if not line: continue
                mode, rest = line[0], line[1:].strip()
                ps = [x.strip() for x in rest.split(':')]
                if mode == 'c':
                    cur = None
                elif mode == 's':
                    if ps[0] == 's': cur = {'style': 'solid', 'color': tuple(int(x) for x in ps[1:5]), 'tris': []}
                    elif ps[0] == 'l': cur = {'style': 'line', 'width': float(ps[1]), 'color': tuple(int(x) for x in ps[2:6]), 'lines': []}
                    elif ps[0] == 'tc': cur = {'style': 'tex', 'color': tuple(int(x) for x in ps[1:5]), 'image': int(ps[5]),
                                               'm': tuple(float(x) for x in ps[6:10]), 't': (float(ps[10]), float(ps[11])), 'tris': []}
                    entries.append(cur)
                elif mode == 't' and cur is not None:
                    v = [float(x) for x in ps]
                    cur['tris'].append(((v[0], v[1]), (v[2], v[3]), (v[4], v[5])))
                elif mode == 'l' and cur is not None:
                    v = [float(x) for x in ps]
                    cur['lines'].append(((v[0], v[1]), (v[2], v[3])))
        self.geom[gid] = entries
        return entries


def mul(a, b):
    """compose affine (m11,m12,m21,m22,tx,ty): apply a then b"""
    m11 = a[0] * b[0] + a[1] * b[2]; m12 = a[0] * b[1] + a[1] * b[3]
    m21 = a[2] * b[0] + a[3] * b[2]; m22 = a[2] * b[1] + a[3] * b[3]
    tx = a[4] * b[0] + a[5] * b[2] + b[4]; ty = a[4] * b[1] + a[5] * b[3] + b[5]
    return (m11, m12, m21, m22, tx, ty)

def xf(m, p): return (p[0] * m[0] + p[1] * m[2] + m[4], p[0] * m[1] + p[1] * m[3] + m[5])

IDENT = (1, 0, 0, 1, 0, 0)


class Player:
    """minimal timeline: run frames 0..N of each playable, no ActionScript"""
    def __init__(self, apt):
        self.apt = apt

    PREFER = ('_show', '_good', '_goodSingle', '_on', '_up', '_normal')

    def preview_frame(self, sprite):
        """without ActionScript: show the end of the sprite's '_show' (or similar) segment"""
        labels = [(fi, it['name']) for fi, f in enumerate(sprite['frames']) for it in f if it and it['kind'] == 'label']
        if not labels:
            return 0
        for want in self.PREFER:
            for i, (fi, nm) in enumerate(labels):
                if nm == want:
                    end = labels[i + 1][0] - 1 if i + 1 < len(labels) else len(sprite['frames']) - 1
                    return max(fi, end)
        return 0

    def display(self, playable, upto):
        dl = {}
        for f in playable['frames'][:upto + 1]:
            for it in f:
                if not it: continue
                if it['kind'] == 'place':
                    d = it['depth']
                    old = dl.get(d)
                    if it['flags'] & 2 or old is None:
                        e = {'char': it.get('char', old['char'] if old else -1), 'm': IDENT, 'tint': None, 'name': it.get('name')}
                        if old and not (it['flags'] & 2): e = dict(old)
                    else:
                        e = dict(old)
                    if 'm' in it: e['m'] = it['m'] + it['t']
                    if 'tint' in it: e['tint'] = it['tint']
                    dl[d] = e
                elif it['kind'] == 'remove':
                    dl.pop(it['depth'], None)
        return dl

    def walk(self, playable, frame, mtx, out, depth=0):
        dl = self.display(playable, min(frame, len(playable['frames']) - 1))
        for d in sorted(dl):
            e = dl[d]
            chars = playable['apt'].movie['chars']
            ch = chars[e['char']] if 0 <= e['char'] < len(chars) else None
            if ch is None: continue
            m = mul(e['m'], mtx)
            if ch['type'] == SHAPE:
                out.append((ch, m))
            elif ch['type'] == SPRITE and depth < 30:
                self.walk(ch, self.preview_frame(ch), m, out, depth + 1)
            elif ch['type'] == BUTTON:
                for rec in ch['records']:
                    if rec['flags'] & 1:
                        sub = ch['apt'].movie['chars'][rec['char']]
                        if sub and sub['type'] == SHAPE:
                            out.append((sub, mul(rec['m'] + rec['t'], m)))


def render(apt, frame, out_png, scale=1.0):
    import numpy as np
    from PIL import Image
    W, H = int(apt.movie['w'] * scale), int(apt.movie['h'] * scale)
    canvas = np.zeros((H, W, 4), np.float32)
    texcache = {}
    texdir = os.path.join(apt.folder, 'art', 'Textures')
    texfiles = {fn.lower(): os.path.join(texdir, fn) for fn in os.listdir(texdir)} if os.path.isdir(texdir) else {}
    def tex(owner, imgid):
        tid = owner.imagemap.get(imgid, (imgid, None))[0]
        key = ('apt_%s_%d.tga' % (owner.name, tid)).lower()
        if key not in texcache:
            p = texfiles.get(key)
            texcache[key] = np.asarray(Image.open(p).convert('RGBA'), np.float32) / 255 if p else None
        return texcache[key]
    items = []
    Player(apt).walk(apt.movie, frame, (scale, 0, 0, scale, 0, 0), items)
    for shape, m in items:
        owner = shape['apt']
        for ent in owner.geometry(shape['geometry']):
            if ent['style'] not in ('solid', 'tex'): continue
            col = np.array(ent['color'], np.float32) / 255
            t = tex(owner, ent['image']) if ent['style'] == 'tex' else None
            for tri in ent['tris']:
                P = [xf(m, v) for v in tri]
                xs = [p[0] for p in P]; ys = [p[1] for p in P]
                x0, x1 = max(int(min(xs)), 0), min(int(max(xs)) + 1, W)
                y0, y1 = max(int(min(ys)), 0), min(int(max(ys)) + 1, H)
                if x0 >= x1 or y0 >= y1: continue
                gx, gy = np.meshgrid(np.arange(x0, x1) + 0.5, np.arange(y0, y1) + 0.5)
                (ax, ay), (bx, by), (cx, cy) = P
                den = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
                if abs(den) < 1e-9: continue
                l1 = ((by - cy) * (gx - cx) + (cx - bx) * (gy - cy)) / den
                l2 = ((cy - ay) * (gx - cx) + (ax - cx) * (gy - cy)) / den
                l3 = 1 - l1 - l2
                inside = (l1 >= -1e-4) & (l2 >= -1e-4) & (l3 >= -1e-4)
                if not inside.any(): continue
                if t is not None:
                    # local shape coords -> texture pixels via the tc matrix
                    lx = l1 * tri[0][0] + l2 * tri[1][0] + l3 * tri[2][0]
                    ly = l1 * tri[0][1] + l2 * tri[1][1] + l3 * tri[2][1]
                    tm = ent['m'] + ent['t']
                    u = lx * tm[0] + ly * tm[2] + tm[4]; v = lx * tm[1] + ly * tm[3] + tm[5]
                    th, tw = t.shape[:2]
                    ui = np.clip(u.astype(int), 0, tw - 1); vi = np.clip(v.astype(int), 0, th - 1)
                    src = t[vi, ui] * col
                else:
                    src = np.broadcast_to(col, gx.shape + (4,))
                region = canvas[y0:y1, x0:x1]
                a = src[..., 3:4] * inside[..., None]
                region[...] = src * a + region * (1 - a)
                region[..., 3:4] = np.maximum(region[..., 3:4], a)
    img = (np.clip(canvas, 0, 1) * 255).astype('uint8')
    Image.fromarray(img, 'RGBA').save(out_png)
    return len(items)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('dir'); ap.add_argument('movie')
    ap.add_argument('--frame', type=int, default=0)
    ap.add_argument('--out')
    ap.add_argument('--dump', action='store_true')
    a = ap.parse_args()
    apt = Apt.load(a.dir, a.movie)
    mv = apt.movie
    print('movie %s %dx%d %d frames %d chars, %d imports, %d exports, %d consts' % (a.movie, mv['w'], mv['h'], len(mv['frames']), len(mv['chars']), len(mv['imports']), len(mv['exports']), len(apt.consts)))
    if a.dump:
        from collections import Counter
        print(Counter(CHAR_NAMES.get(c['type'], c['type']) for c in mv['chars'][1:] if c))
        for i, f in enumerate(mv['frames'][:3]):
            print('frame', i, [(it['kind'], it.get('depth'), it.get('char'), it.get('name')) for it in f if it])
        print('imports', mv['imports'][:10]); print('exports', mv['exports'][:10])
    if a.out:
        n = render(apt, a.frame, a.out)
        print('rendered %d shapes -> %s' % (n, a.out))


if __name__ == '__main__':
    main()
