import struct, sys, hashlib

path = r"D:\mido\dumps\boot_full.img"
d = open(path, 'rb').read()
o = 0x80000
h = d[o:o+2048]
ksize = struct.unpack('<I', h[8:12])[0]
rstart = o + 2048 + ((ksize + 2047) // 2048) * 2048
k = d[o+2048:o+2048+ksize]
print('kernel first 16 bytes:', k[:16].hex())
print('gzip?', k[:2] == b'\x1f\x8b', ' zImage?', k[0x24:0x28].hex(), ' lz4?', k[2:4] == b'\x21\x1c', ' zstd?', k[:4] == b'\x28\xb5\x2f\xfd')
print('id field @576:', h[576:608].hex())
print('cmdline @64:', h[64:576].rstrip(b'\x00')[:200])
print('extra_cmdline @608:', h[608:1632].rstrip(b'\x00')[:200])
print('ramdisk start file off 0x%x, ramdisk_size %d' % (rstart, struct.unpack('<I', h[16:20])[0]))

# location of the FDT inside kernel window
app = 0xb97828
pos = o + 2048 + app
print('\nFDT at kernel+0x%x (file 0x%x): magic=%s totalsize=%d' % (app, pos, d[pos:pos+4].hex(), struct.unpack('>I', d[pos+4:pos+8])[0]))
# QCDT table?
print('bytes right before that FDT:', d[pos-32:pos].hex())
# dt_table magic 0xd7b7ab1e
for m in (b'\xd7\xb7\xab\x1e',):
    i = d.find(m, o, rstart)
    print('dt_table magic found at file 0x%x' % i if i >= 0 else 'dt_table magic not found')
