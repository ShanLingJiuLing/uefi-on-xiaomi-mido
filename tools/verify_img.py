import sys, zlib, struct, os
sys.path.insert(0, r'D:\mido\tools')
from mkbootimg import parse

base = r'D:\mido'
for name in ('boot-mido-rel2.img', 'boot-mido-dbg2.img'):
    d = open(os.path.join(base, name), 'rb').read()
    i = parse(d)
    k = i['kernel']
    do = zlib.decompressobj(16 + zlib.MAX_WBITS)
    out = do.decompress(k)
    hb_mov = out.count(struct.pack('<I', 0xD2B2000A))   # movz x10,#0x9000,lsl16
    hb_end = out.count(struct.pack('<I', 0xF2B2280B))   # movk x11,#0x9140,lsl16
    print('%-18s size=%d kernel_addr=0x%x payload=%d trail=%d' %
          (name, len(d), i['kernel_addr'], len(out), len(do.unused_data)))
    print('   movz x10,#0x9000 x%d ; movk x11,#0x9140,lsl16 x%d  (expect 2 each)'
          % (hb_mov, hb_end))
