#!/bin/bash
cd $HOME/edk2-msm || exit 1
E=$(find workspace/Build/mido/RELEASE_GCC5 -name 'SdhcPlatformDxe.efi' | head -1)
echo "efi = $E"
echo "===== 反汇编中查找关键常量 ====="
aarch64-linux-gnu-objdump -d "$E" 2>/dev/null | grep -iE '0x10c|0x78240|0x78249|movz|movk' | grep -iE '0x10c|0x7824|0x78\b' | head -20
echo
echo "===== 找 mmio write 到 0x7824078 附近的指令序列 ====="
aarch64-linux-gnu-objdump -d "$E" 2>/dev/null | grep -B3 -A3 '0x10c' | head -30
