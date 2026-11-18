#!/bin/bash
cat > /tmp/dumpgcc.py <<'PYEOF'
import mmap, os, struct, sys

NAMES = ["BCR","CMD_RCGR","CFG_RCGR","M","N","D","+0x18","+0x1C",
         "APPS_CBCR","AHB_CBCR"]

def dump(base, title):
    print(title)
    try:
        f = os.open('/dev/mem', os.O_RDONLY | os.O_SYNC)
        m = mmap.mmap(f, 4096, offset=base)
        for i in range(0, 0x28, 4):
            v = struct.unpack('<I', m[i:i+4])[0]
            print("  +0x%02x  %-10s = 0x%08x" % (i, NAMES[i//4], v))
        m.close(); os.close(f)
    except Exception as e:
        print("  ERROR:", type(e).__name__, e)

dump(0x01842000, "=== GCC SDCC1 (eMMC, live) ===")
dump(0x01843000, "=== GCC SDCC2 (microSD, live) ===")
PYEOF
echo "=== kernel ==="; uname -r
echo 666666 | sudo -S -p '' python3 /tmp/dumpgcc.py
echo
echo "=== SD 卡是否在挂载中 ==="
findmnt -no SOURCE,TARGET,FSTYPE | grep -i mmcblk1 || echo "(no mmcblk1 mount)"
