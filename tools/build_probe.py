import struct, sys
sys.path.insert(0, r'D:\mido\tools')
from mkbootimg import build, parse

# --- payload layout -------------------------------------------------------
# 0x00  b +0x40            (0x14000010)
# 0x08  0  (u64)           <- keep <= 2MB so lk2nd's text_offset hack does not write
# 0x2C  u32 = dtb offset   <- DTB_OFFSET, read by dev_tree_appended()
# 0x40  movz x0, #0x8400, lsl 16   -> PSCI 0x84000009 = SYSTEM_RESET
# 0x44  movk x0, #0x0009
# 0x48  smc #0
# 0x4C  b .                (fallback if smc returns)
# 0x60  FDT

DTB_OFF = 0x60
src = open(r'D:\mido\test\android-boot.img', 'rb').read()
k = parse(src)['kernel']
dtb_size = struct.unpack('>I', k[0xb97828 + 4:0xb97828 + 8])[0]
dtb = k[0xb97828:0xb97828 + dtb_size]

payload = bytearray(DTB_OFF + len(dtb))
struct.pack_into('<I', payload, 0x00, 0x14000010)   # b +0x40
struct.pack_into('<Q', payload, 0x08, 0)            # zeros
struct.pack_into('<I', payload, 0x2C, DTB_OFF)      # DTB offset
struct.pack_into('<I', payload, 0x40, 0xD2B08000)   # movz x0, #0x8400, lsl #16
struct.pack_into('<I', payload, 0x44, 0xF2800120)   # movk x0, #0x0009
struct.pack_into('<I', payload, 0x48, 0xD4000003)   # smc #0
struct.pack_into('<I', payload, 0x4C, 0x14000000)   # b .
payload[DTB_OFF:] = dtb

print('payload = %d bytes, dtb %d bytes at +0x%x' % (len(payload), len(dtb), DTB_OFF))

out = build(bytes(payload), b'', kernel_addr=0x80008000, ramdisk_addr=0x81000000,
            tags_addr=0x80000100, page=2048, os_version=0x1400014c,
            cmdline=b'lk2nd-test', name=b'mido-uefi-test')
open(r'D:\mido\test\probe.img', 'wb').write(out)
print('written test/probe.img: %d bytes' % len(out))

i = parse(out)
print('reparse: kernel_size=%d ramdisk=%d tags=0x%x' % (i['kernel_size'], len(i['ramdisk']), i['tags_addr']))
print('payload head:', payload[:16].hex())
print('*(u32*)(p+0x2C) =', struct.unpack('<I', payload[0x2C:0x30])[0])
