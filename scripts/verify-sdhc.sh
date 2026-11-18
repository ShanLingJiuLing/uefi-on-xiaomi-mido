#!/bin/bash
cd $HOME/edk2-msm || exit 1
D=Platform/Xiaomi/msm8953/SdhcPlatformDxe
echo "===== 源码是否含 HC 模式代码 ====="
grep -n 'SDCC_MCI_HC_MODE\|HC_MODE_EN\|SDCC_VENDOR_FUNC' $D/SdhcPlatformDxe.c | head
echo
echo "===== 源文件与产物的 md5 / 时间 ====="
md5sum $D/SdhcPlatformDxe.c
E=$(find workspace/Build/mido/RELEASE_GCC5 -name 'SdhcPlatformDxe.efi' | head -1)
echo "efi = $E"
ls -l --time-style=full-iso "$E" 2>/dev/null
md5sum "$E" 2>/dev/null
echo
echo "===== FD 的 md5 ====="
FD=workspace/Build/mido/RELEASE_GCC5/FV/MSM8953_UEFI.fd
ls -l --time-style=full-iso $FD
md5sum $FD
echo
echo "===== FD 里是否出现 0x07824000 / 0x07824900 常量（说明新代码进了 FV）====="
python3 - "$FD" <<'EOF'
import sys
d=open(sys.argv[1],'rb').read()
for name,val in (('SDCC1_CORE 0x07824000',0x07824000),('SDCC1_SDHCI 0x07824900',0x07824900),('MAGIC MIDO',0x4D49444F)):
    pat=val.to_bytes(4,'little'); print('%-24s count=%d' % (name, d.count(pat)))
EOF
