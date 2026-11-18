import sys, zlib, struct
sys.path.insert(0, r'D:\mido\tools')
from mkbootimg import parse

d = open(r'D:\mido\boot-mido.img', 'rb').read()
i = parse(d)
k = i['kernel']
dobj = zlib.decompressobj(16 + zlib.MAX_WBITS)
payload = dobj.decompress(k)
trail = dobj.unused_data
print('kernel gzip  : %d B -> payload %d B, trailing DTB %d B' % (len(k) - len(trail), len(payload), len(trail)))

# BootShim is the first bytes; FD follows at _Payload (0x90 = 144)
shim_len = 144
fd = payload[shim_len:]
print('bootshim     : %d B, first 16: %s' % (shim_len, payload[:16].hex()))
print('FD           : %d B (0x%x)' % (len(fd), len(fd)))
print('  _FVH sig   : %s at 0x28' % fd[0x28:0x2c])
print('  FvLength   : 0x%x' % struct.unpack('<Q', fd[0x20:0x28])[0])

# search for driver names (EDK2 puts component names as UCS-2 / ASCII in FV files)
names = [b'GopDxe', b'SimpleFb', b'FrameBuffer', b'GraphicsConsole', b'SIMPLE_FB',
         b'ConPlatformDxe', b'ConSplitterDxe', b'GraphicsOutputDxe', b'BootGraphicsResourceTable',
         b'QcomDisplay', b'MdpDxe', b'DisplayDxe', b'UefiShell', b'Shell',
         b'SmemDxe', b'DALSys', b'ClockDxe', b'PmicDxe', b'SdccDxe', b'XhciDxe']
low = fd.lower()
for n in names:
    u = n.lower()
    asc = low.count(u)
    ucs2 = low.count(u.replace(b'', b'\x00')) if len(u) > 2 else 0
    print('  %-24s ascii=%d ucs2~=%d' % (n.decode(), asc, ucs2))
