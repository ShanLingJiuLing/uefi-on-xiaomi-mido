#!/bin/bash
cd $HOME/edk2-msm || exit 1
E=Common/edk2
echo "===== 相关模块是否存在 ====="
for p in \
  "$E/MdeModulePkg/Bus/Pci/NonDiscoverablePciDeviceDxe" \
  "$E/MdeModulePkg/Bus/Pci/SdMmcPciHcDxe" \
  "$E/MdeModulePkg/Bus/Sd/EmmcDxe" \
  "$E/MdeModulePkg/Bus/Sd/SdDxe" \
  "$E/MdeModulePkg/Bus/Sd/SdDxe/SdDxe.inf" \
  "$E/EmbeddedPkg/Universal/MmcDxe" \
  "$E/MdeModulePkg/Library/NonDiscoverableDeviceRegistrationLib" \
  "$E/MdeModulePkg/Bus/Pci/SdMmcPciHcDxe/SdMmcPciHcDxe.inf" ; do
  if [ -e "$p" ]; then echo "OK   $p"; else echo "MISS $p"; fi
done
echo
echo "===== 列出 MdeModulePkg/Bus/Sd 与 Bus/Pci 里的 SD 相关 ====="
ls $E/MdeModulePkg/Bus/Sd 2>/dev/null
ls $E/MdeModulePkg/Bus/Pci 2>/dev/null | grep -i 'sd\|non'
echo
echo "===== SdMmcPciHcDxe 绑定的协议 ====="
grep -n 'ProtocolGuid\|PciIo' $E/MdeModulePkg/Bus/Pci/SdMmcPciHcDxe/SdMmcPciHcDxe.inf 2>/dev/null | head -20
echo
echo "===== NonDiscoverableDeviceRegistrationLib 头文件 ====="
ls $E/MdeModulePkg/Include/Library/ | grep -i nondis
