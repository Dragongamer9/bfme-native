#!/usr/bin/env python3
"""Write a minimal flat SAGE (.map, Generals chunk format) map for headless logic tests.

  mkmap.py OUT.map [--size 256] [--border 30] [--height 20]
                   [--player NAME:FACTION:human|ai] ... [--object TEMPLATE:OWNER:X:Y[:ANGLE]] ...

Writes HeightMapData (v4, flat), WorldInfo, SidesList (neutral + given players, one default
team each), ObjectsList (Player_N_Start waypoints + any --object). No textures: the engine
only reads the logical map data in -headless mode.
"""
import argparse
import struct


class Names:
    def __init__(self):
        self.ids = {}

    def id(self, name):
        if name not in self.ids:
            self.ids[name] = len(self.ids) + 1
        return self.ids[name]

    def toc(self):
        out = b'CkMp' + struct.pack('<i', len(self.ids))
        for name, i in self.ids.items():
            b = name.encode('ascii')
            out += struct.pack('<B', len(b)) + b + struct.pack('<I', i)
        return out


def astr(s):
    b = s.encode('latin1')
    return struct.pack('<H', len(b)) + b


def ustr(s):
    return struct.pack('<H', len(s)) + s.encode('utf-16-le')


def dict_(names, items):
    """items: list of (key, type, value); type in bool,int,real,ascii,unicode"""
    code = {'bool': 0, 'int': 1, 'real': 2, 'ascii': 3, 'unicode': 4}
    out = struct.pack('<H', len(items))
    for k, t, v in items:
        out += struct.pack('<i', (names.id(k) << 8) | code[t])
        if t == 'bool':
            out += struct.pack('<B', 1 if v else 0)
        elif t == 'int':
            out += struct.pack('<i', v)
        elif t == 'real':
            out += struct.pack('<f', v)
        elif t == 'ascii':
            out += astr(v)
        else:
            out += ustr(v)
    return out


def chunk(names, name, version, payload):
    return struct.pack('<IHi', names.id(name), version, len(payload)) + payload


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('out')
    ap.add_argument('--size', type=int, default=256, help='cells per side incl. border')
    ap.add_argument('--border', type=int, default=30)
    ap.add_argument('--height', type=int, default=20, help='8-bit terrain height')
    ap.add_argument('--player', action='append', default=[], help='NAME:FACTION:human|ai')
    ap.add_argument('--object', action='append', default=[], help='TEMPLATE:OWNER:X:Y[:ANGLE]')
    a = ap.parse_args()
    if not a.player:
        a.player = ['player1:FactionMen:human', 'player2:FactionMordor:ai']

    names = Names()
    w = h = a.size
    b = a.border

    # HeightMapData v4
    hm = struct.pack('<iii', w, h, b)
    hm += struct.pack('<i', 1) + struct.pack('<ii', w - 2 * b, h - 2 * b)
    hm += struct.pack('<i', w * h) + bytes([a.height]) * (w * h)
    body = chunk(names, 'HeightMapData', 4, hm)

    body += chunk(names, 'WorldInfo', 1, dict_(names, [('weather', 'int', 0)]))

    # SidesList v3: neutral side + players; one team per side
    sides = [('', 'FactionCivilian', False)] + [
        (p.split(':')[0], p.split(':')[1], p.split(':')[2] == 'human') for p in a.player]
    sl = struct.pack('<i', len(sides))
    for name, faction, human in sides:
        enemies = ' '.join(n for n, _, _ in sides[1:] if n and n != name)
        sl += dict_(names, [
            ('playerName', 'ascii', name),
            ('playerIsHuman', 'bool', human),
            ('playerDisplayName', 'unicode', name or 'Neutral'),
            ('playerFaction', 'ascii', faction if name else ''),
            ('playerEnemies', 'ascii', enemies if name else ''),
            ('playerAllies', 'ascii', ''),
        ])
        sl += struct.pack('<i', 0)  # build list
    sl += struct.pack('<i', len(sides))
    for name, _, _ in sides:
        sl += dict_(names, [
            ('teamName', 'ascii', 'team' + name),
            ('teamOwner', 'ascii', name),
            ('teamIsSingleton', 'bool', True),
        ])
    body += chunk(names, 'SidesList', 3, sl)

    # ObjectsList: start waypoints along the middle, then requested objects
    objs = b''
    inner = (w - 2 * b) * 10.0
    for i, _ in enumerate(a.player):
        x = inner * (0.25 + 0.5 * i / max(1, len(a.player) - 1))
        y = inner * 0.5
        d = dict_(names, [('waypointID', 'int', i + 1), ('waypointName', 'ascii', 'Player_%d_Start' % (i + 1))])
        objs += chunk(names, 'Object', 3, struct.pack('<ffffi', x, y, 0.0, 0.0, 0) + astr('*Waypoints/Waypoint') + d)
    for o in a.object:
        parts = o.split(':')
        tmpl, owner, x, y = parts[0], parts[1], float(parts[2]), float(parts[3])
        ang = float(parts[4]) if len(parts) > 4 else 0.0
        d = dict_(names, [('originalOwner', 'ascii', 'team' + owner)])
        objs += chunk(names, 'Object', 3, struct.pack('<ffffi', x, y, 0.0, ang, 0) + astr(tmpl) + d)
    body += chunk(names, 'ObjectsList', 3, objs)

    with open(a.out, 'wb') as f:
        f.write(names.toc() + body)
    print('wrote %s (%d bytes, %dx%d cells, %d players, %d objects)' % (a.out, len(names.toc()) + len(body), w, h, len(a.player), len(a.object)))


if __name__ == '__main__':
    main()
