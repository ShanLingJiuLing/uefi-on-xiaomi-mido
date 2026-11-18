import struct, sys, json

def parse_dtb(path):
    d = open(path, 'rb').read()
    magic, totalsize, off_struct, off_strings = struct.unpack('>IIII', d[0:16])
    assert magic == 0xd00dfeed, hex(magic)
    # version fields: version, last_comp_version
    pos = off_struct
    node_stack = [('', {})]
    path_stack = ['']
    strings = d[off_strings:]
    def s(off):
        e = strings.index(b'\0', off)
        return strings[off:e].decode('utf-8', 'replace')
    nodes = []
    cur = {'path': '', 'props': {}, 'children': []}
    root = cur
    idx = 0
    pos = off_struct
    while True:
        tok = struct.unpack('>I', d[pos:pos+4])[0]; pos += 4
        if tok == 1:  # FDT_BEGIN_NODE
            e = d.index(b'\0', pos)
            name = d[pos:e].decode('utf-8', 'replace')
            pos = (e + 1 + 3) & ~3
            new = {'name': name, 'props': {}, 'children': [], 'parent': cur}
            cur['children'].append(new)
            cur = new
            idx += 1
            new['id'] = idx
        elif tok == 2:  # FDT_END_NODE
            cur = cur['parent']
        elif tok == 3:  # FDT_PROP
            ln, nameoff = struct.unpack('>II', d[pos:pos+8]); pos += 8
            val = d[pos:pos+ln]; pos = (pos + ln + 3) & ~3
            cur['props'][s(nameoff)] = val
        elif tok == 4:  # FDT_NOP
            pass
        elif tok == 9:  # FDT_END
            break
        else:
            raise SystemExit('bad token %d at %d' % (tok, pos - 4))
    return root

def fmt_val(v, name):
    if len(v) == 0:
        return '<empty>'
    # pure text (NUL separated) -> quoted
    textlike = all(32 <= c < 127 or c in (0, 9, 10) for c in v)
    if textlike and (len(v) != 4 or name in ('model', 'status', 'name', 'device_type', 'label')):
        parts = [x.decode('utf-8', 'replace') for x in v.split(b'\0') if x]
        return '"' + '", "'.join(parts) + '"'
    if len(v) == 4:
        return '0x%x' % struct.unpack('>I', v)[0]
    if len(v) == 8:
        a, b = struct.unpack('>II', v)
        return '<0x%x 0x%x>' % (a, b)
    if len(v) % 4 == 0 and len(v) <= 128:
        return '<' + ' '.join('0x%x' % x for x in struct.unpack('>%dI' % (len(v)//4), v)) + '>'
    return '<%d bytes: %s...>' % (len(v), v[:16].hex())

def walk(n, depth=0):
    out = []
    if depth:
        out.append('%s%s {' % ('    ' * (depth - 1), n['name']))
    for k, v in n['props'].items():
        out.append('%s%s = %s;' % ('    ' * depth, k, fmt_val(v, k)))
    for c in n['children']:
        out.extend(walk(c, depth + 1))
    if depth:
        out.append('%s};' % ('    ' * (depth - 1),))
    return out

root = parse_dtb(sys.argv[1])
txt = '\n'.join(walk(root))
open(sys.argv[2], 'w', encoding='utf-8').write(txt + '\n')

real_root = root['children'][0]
print('total lines:', txt.count('\n'))
print('--- root props ---')
for k, v in real_root['props'].items():
    print('  %s = %s' % (k, fmt_val(v, k)))
print('--- top level nodes ---')
for c in real_root['children']:
    print('  ', c['name'] or '<anon>')

if len(sys.argv) > 3:
    pat = sys.argv[3].lower()
    def walk2(n, depth, buf):
        hit = pat in n['name'].lower()
        if hit:
            buf.append('\n'.join(walk(n, depth)))
            return
        for c in n['children']:
            walk2(c, depth + 1, buf)
    buf = []
    walk2(real_root, 1, buf)
    print('\n'.join(buf))
