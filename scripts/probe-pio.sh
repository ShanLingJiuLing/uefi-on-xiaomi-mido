#!/bin/bash
cd $HOME/edk2-msm
D=Common/edk2/MdeModulePkg/Bus/Pci/SdMmcPciHcDxe
echo "=== PIO 支持相关 ==="
grep -rn 'Pio\|PIO' $D/*.c $D/*.h | head -20
echo
echo "=== CAPABILITIES 解析处 ==="
grep -n -A40 'SdMmcHcGetCapability' $D/SdMmcPciHci.c | head -60
