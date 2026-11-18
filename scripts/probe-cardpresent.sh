#!/bin/bash
cd $HOME/edk2-msm
echo "=== SdMmcPciHcDxe 是否检查 Card Present ==="
grep -rn 'CARD_PRESENT\|CardPresent\|BIT16' Common/edk2/MdeModulePkg/Bus/Pci/SdMmcPciHcDxe/*.c Common/edk2/MdeModulePkg/Bus/Pci/SdMmcPciHcDxe/*.h 2>/dev/null | head -20
echo
echo "=== SdMmcHcCardPresent 定义 ==="
grep -rn -A12 'SdMmcHcCardPresent' Common/edk2/MdeModulePkg/Bus/Pci/SdMmcPciHcDxe/SdMmcHcDxe.c Common/edk2/MdeModulePkg/Bus/Pci/SdMmcPciHcDxe/SdMmcHcDxe.h 2>/dev/null | head -30
echo
echo "=== EmmCxe 的 CMD0 / CMD1 发送 ==="
grep -rn 'CommandIndex\|ResponseType' Common/edk2/MdeModulePkg/Bus/Sd/EmmcDxe/EmmcBlockIo.c 2>/dev/null | head -12
ls Common/edk2/MdeModulePkg/Bus/Sd/EmmcDxe/
