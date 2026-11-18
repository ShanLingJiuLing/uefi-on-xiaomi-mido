import sys, struct, re

def parse_dtb(path):
    d = open(path, 'rb').read()
    magic, totalsize, off_struct, off_strings = struct.unpack('>IIII', d[0:16])
    assert magic == 0xd00dfeed
    strings = d[off_strings:]
    def s(off):
        e = strings.index(b'\0', off)
        return strings[off:e].decode('utf-8', 'replace')
    pos = off_struct
    root = {'name': '', 'props': {}, 'children': [], 'parent': None}
    cur = root
    while True:
        tok = struct.unpack('>I', d[pos:pos+4])[0]; pos += 4
        if tok == 1:
            e = d.index(b'\0', pos); name = d[pos:e].decode('utf-8', 'replace')
            pos = (e + 1 + 3) & ~3
            new = {'name': name, 'props': {}, 'children': [], 'parent': cur}
            cur['children'].append(new); cur = new
        elif tok == 2:
            cur = cur['parent']
        elif tok == 3:
            ln, nameoff = struct.unpack('>II', d[pos:pos+8]); pos += 8
            val = d[pos:pos+ln]; pos = (pos + ln + 3) & ~3
            cur['props'][s(nameoff)] = val
        elif tok == 4:
            pass
        elif tok == 9:
            break
        else:
            raise SystemExit('bad token')
    return root

def text(v):
    parts = [x.decode('utf-8', 'replace') for x in v.split(b'\0') if x]
    return ' '.join(parts)

def fmt(v, name):
    if len(v) == 0: return 'true'
    tl = all(32 <= c < 127 or c in (0, 9, 10) for c in v)
    if tl and (len(v) != 4 or name in ('model', 'status', 'name', 'device_type', 'label', 'method')):
        parts = [x.decode('utf-8', 'replace') for x in v.split(b'\0') if x]
        return '"' + '", "'.join(parts) + '"'
    if len(v) == 4: return '0x%x' % struct.unpack('>I', v)[0]
    if len(v) == 8:
        a, b = struct.unpack('>II', v); return '<0x%x 0x%x>' % (a, b)
    if len(v) % 4 == 0 and len(v) <= 128:
        return '<' + ' '.join('0x%x' % x for x in struct.unpack('>%dI' % (len(v)//4), v)) + '>'
    return '<%d bytes>' % len(v)

root = parse_dtb(sys.argv[1])
pat = re.compile(sys.argv[2], re.I)
props_only = '--full' not in sys.argv

def path_of(n):
    p = []
    while n and n['parent'] is not None:
        p.append(n['name'] or '/')
        n = n['parent']
    return '/' + '/'.join(reversed(p))

def walk(n):
    if pat.search(n['name'] or ''):
        print('\n=== %s  [%s]' % (path_of(n), n['name'] or '<root>'))
        for k, v in n['props'].items():
            print('    %-34s %s' % (k, fmt(v, k)))
        if not props_only:
            for c in n['children']:
                print('    child: %s' % c['name'])
    for c in n['children']:
        walk(c)

walk(root)
