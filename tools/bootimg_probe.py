import struct, sys

path = r"D:\mido\dumps\boot_full.img"
d = open(path, 'rb').read()
o = 0x80000
h = d[o:o+2048]

magic, ksize, kaddr, rsize, raddr, ssize, saddr, tags, page, f40, f44 = struct.unpack('<8s10I', h[:48])
name = h[48:64].rstrip(b'\0').decode(errors='replace')
print('magic=%s kernel=%d ramdisk=%d second=%d page=%d f40(dt_size/hdrver)=0x%x f44(os_version)=0x%x'
      % (magic, ksize, rsize, ssize, page, f40, f44))
print('kernel_addr=0x%x ramdisk_addr=0x%x tags_addr=0x%x name=%r' % (kaddr, raddr, tags, name))

kstart = o + page
print('kernel starts at file offset 0x%x' % kstart)

# DTB_OFFSET = 0x2C
app = struct.unpack('<I', d[kstart+0x2C:kstart+0x30])[0]
print('*(u32*)(kernel+0x2C) = 0x%x (%d)' % (app, app))

# arm64 header
text_off, img_size, flags = struct.unpack('<QQQ', d[kstart+8:kstart+8+24])
print('text_offset=0x%x image_size=0x%x flags=0x%x' % (text_off, img_size, flags))
print('arm64 magic @0x38 = %r' % d[kstart+0x38:kstart+0x3C])

def be32(buf, off):
    return struct.unpack('>I', buf[off:off+4])[0]

if 0 < app < ksize:
    pos = kstart + app
    m = be32(d, pos)
    print('at kernel+0x%x file off 0x%x: FDT magic = 0x%08x %s' % (app, pos, m, '(valid FDT)' if m == 0xd00dfeed else '(NOT fdt)'))
    if m == 0xd00dfeed:
        tot = be32(d, pos+4)
        print('  fdt totalsize = 0x%x (%d)' % (tot, tot))
        # scan for multiple DTBs
        p = pos
        n = 0
        while p < kstart + ksize:
            if be32(d, p) == 0xd00dfeed:
                t = be32(d, p+4)
                n += 1
                if n <= 5:
                    print('  dtb#%d at kernel+0x%x size 0x%x' % (n, p-kstart, t))
                p += t
            else:
                break
        print('  total dtbs = %d' % n)

# brute search for FDT magic inside kernel window
found = []
p = kstart
end = kstart + ksize
while p < end - 4:
    if d[p:p+4] == b'\xd0\x0d\xfe\xed':
        found.append(p - kstart)
    p += 4
print('FDT magic occurrences inside kernel window: %s' % [hex(x) for x in found[:10]])
