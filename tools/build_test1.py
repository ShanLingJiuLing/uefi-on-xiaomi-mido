import struct, gzip, io, sys
sys.path.insert(0, r'D:\mido\tools')
from mkbootimg import parse, build

src = r'D:\mido\test\android-boot.img'
data = open(src, 'rb').read()
info = parse(data)
k = info['kernel']

# The gzip stream holds the raw arm64 Image; the DTB sits after the stream,
# still inside hdr.kernel_size (that is where lk2nd's decompress() dtb_offset points).
dtb_off = 0xb97828
dtb_size = struct.unpack('>I', k[dtb_off + 4:dtb_off + 8])[0]
dtb = k[dtb_off:dtb_off + dtb_size]
img = gzip.decompress(k[:dtb_off])
print('gzip stream 0x%x -> Image %d bytes (0x%x)' % (dtb_off, len(img), len(img)))
print('dtb at +0x%x size %d, magic %s' % (dtb_off, dtb_size, dtb[:4].hex()))

# arm64 Image header sanity
print('Image: code0=0x%08x text_offset=0x%x image_size=0x%x flags=0x%x magic@0x38=%r'
      % (struct.unpack('<I', img[0:4])[0], struct.unpack('<Q', img[8:16])[0],
         struct.unpack('<Q', img[0x10:0x18])[0], struct.unpack('<Q', img[0x18:0x20])[0], img[0x38:0x3C]))

payload = bytearray(img + dtb)
new_dtb_off = len(img)
struct.pack_into('<I', payload, 0x2C, new_dtb_off)
print('patched *(u32*)(payload+0x2C) = 0x%x (len(Image))' % new_dtb_off)

out = build(bytes(payload), info['ramdisk'],
            kernel_addr=info['kernel_addr'], ramdisk_addr=info['ramdisk_addr'],
            tags_addr=info['tags_addr'], page=info['page'],
            os_version=info['os_version'], cmdline=info['cmdline'], name=info['name'])
open(r'D:\mido\test\mido-uncompressed.img', 'wb').write(out)
print('written test/mido-uncompressed.img: %d bytes' % len(out))

# self-check: parse it back
i2 = parse(out)
print('reparse: kernel_size=0x%x ramdisk=%d dtb_field=%s' % (i2['kernel_size'], len(i2['ramdisk']), i2.get('header_version')))
