#!/usr/bin/env python3
"""Minimal Android boot image (v0) builder / parser for mido UEFI work.

Verified against a real Redmi Note 4X (mido) boot.img.

Layout (page aligned):
    [header page] [kernel] [ramdisk] [second] [dtb]

Header v0 (all little endian):
    0   magic[8]        'ANDROID!'
    8   kernel_size     12  kernel_addr
    16  ramdisk_size    20  ramdisk_addr
    24  second_size     28  second_addr
    32  tags_addr       36  page_size
    40  header_version  44  os_version
    48  name[16]
    64  cmdline[512]
    576 id[32]  (sha1)
    608 extra_cmdline[1024]
"""
import struct, hashlib, sys, gzip, io

PAGE = 2048


def _pad(buf, page=PAGE):
    n = len(buf) % page
    return buf if n == 0 else buf + b'\x00' * (page - n)


def _sha1_id(kernel, ramdisk, second=b'', dtb=b''):
    h = hashlib.sha1()
    for chunk in (kernel, ramdisk, second, dtb):
        if chunk:
            h.update(chunk)
            h.update(struct.pack('<I', len(chunk)))
    return h.digest() + b'\x00' * 12


def build(kernel, ramdisk=b'', second=b'', dtb=b'', *,
          kernel_addr=0x80008000, ramdisk_addr=0x81000000,
          second_addr=0x80F00000, tags_addr=0x80000100,
          page=PAGE, os_version=0, cmdline=b'', name=b'', hdr_version=0):
    assert len(kernel) <= 0xFFFFFFFF
    cmdline = cmdline[:511]
    head = struct.pack(
        '<8s10I', b'ANDROID!',
        len(kernel), kernel_addr,
        len(ramdisk), ramdisk_addr,
        len(second), second_addr,
        tags_addr, page, hdr_version, os_version,
    ) + name[:16].ljust(16, b'\x00') + cmdline.ljust(512, b'\x00')
    head += _sha1_id(kernel, ramdisk, second, dtb)
    head += b'\x00' * 1024            # extra_cmdline
    head = head.ljust(page, b'\x00')

    out = head + _pad(kernel, page) + _pad(ramdisk, page)
    if second:
        out += _pad(second, page)
    if dtb:
        out += _pad(dtb, page)
    return out


def parse(data):
    magic, ksize, kaddr, rsize, raddr, ssize, saddr, tags, page, hv, osv = \
        struct.unpack('<8s10I', data[:48])
    if magic != b'ANDROID!':
        raise ValueError('bad magic %r' % magic)
    info = dict(page=page, kernel_size=ksize, kernel_addr=kaddr,
                ramdisk_size=rsize, ramdisk_addr=raddr,
                second_size=ssize, tags_addr=tags,
                header_version=hv, os_version=osv,
                name=data[48:64].rstrip(b'\x00'),
                cmdline=data[64:576].rstrip(b'\x00'),
                id=data[576:596].hex())
    o = page
    info['kernel'] = data[o:o + ksize]; o += _r(ksize, page)
    info['ramdisk'] = data[o:o + rsize]; o += _r(rsize, page)
    info['second'] = data[o:o + ssize]; o += _r(ssize, page)
    if hv == 0 and len(data) > o:
        info['dtb'] = data[o:]
    return info


def _r(n, page=PAGE):
    return ((n + page - 1) // page) * page


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return
    cmd = sys.argv[1]
    if cmd == 'dump':
        info = parse(open(sys.argv[2], 'rb').read())
        for k, v in info.items():
            if isinstance(v, bytes):
                print('%-14s %d bytes %r' % (k, len(v), v[:24]))
            else:
                print('%-14s 0x%x (%d)' % (k, v, v) if isinstance(v, int) else '%-14s %s' % (k, v))
    elif cmd == 'selftest':
        src = open(sys.argv[2], 'rb').read()
        info = parse(src)
        rebuilt = build(info['kernel'], info['ramdisk'], info['second'],
                        kernel_addr=info['kernel_addr'], ramdisk_addr=info['ramdisk_addr'],
                        tags_addr=info['tags_addr'], page=info['page'],
                        os_version=info['os_version'], cmdline=info['cmdline'],
                        name=info['name'])
        print('original size %d, rebuilt size %d, identical=%s'
              % (len(src), len(rebuilt), src == rebuilt))
        if src != rebuilt:
            for i in range(min(len(src), len(rebuilt))):
                if src[i] != rebuilt[i]:
                    print('first diff at 0x%x: %02x vs %02x' % (i, src[i], rebuilt[i]))
                    break


if __name__ == '__main__':
    main()
