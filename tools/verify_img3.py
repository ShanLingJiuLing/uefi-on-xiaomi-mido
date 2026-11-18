import sys, zlib, struct, os
sys.path.insert(0, r'D:\mido\tools')
from mkbootimg import parse

PATS = {
    'movz x10,#0x9000,lsl16 (D2B2000A)': 0xD2B2000A,
    'movk x10,#0x1000      (F282000A)': 0xF282000A,   # correct start -> 0x90001000
    'movk x10,#0x1000,lsl16(F2A2000A)': 0xF2A2000A,   # BUGGY -> 0x90100000
    'movk x11,#0x9140      (F292280B)': 0xF292280B,   # end -> 0x90009140
    'str  w12,[x10],#4     (B800454C)': 0xB800454C,
}

base = r'D:\mido'
for name in ('boot-mido-rel3.img', 'boot-mido-dbg3.img'):
    d = open(os.path.join(base, name), 'rb').read()
    i = parse(d)
    do = zlib.decompressobj(16 + zlib.MAX_WBITS)
    out = do.decompress(i['kernel'])
    print('%-18s size=%d kernel_addr=0x%x payload=%d trail=%d'
          % (name, len(d), i['kernel_addr'], len(out), len(do.unused_data)))
    for label, v in PATS.items():
        print('   %-38s x%d' % (label, out.count(struct.pack('<I', v))))
