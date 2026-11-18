#!/bin/bash
P=http://192.168.137.1:7897
cd $HOME/edk2-msm || exit 1
echo "===== 拉 SimpleInit 与 EFI_Binaries ====="
git -c http.proxy=$P -c https.proxy=$P submodule update --init --depth 1 \
  GPLDrivers/Library/SimpleInit Platform/EFI_Binaries 2>&1 | tail -8
echo "===== 检查关键文件 ====="
ls GPLDrivers/Library/SimpleInit/SimpleInit.inc 2>&1
ls Platform/EFI_Binaries/Applications/LinuxSimpleMassStorage/*.inf 2>&1 | head -3
echo "===== 空间 ====="
du -sh $HOME/edk2-msm; df -h / | tail -1
