#!/bin/bash
cd $HOME/edk2-msm
F=Common/edk2/MdeModulePkg/Bus/Pci/SdMmcPciHcDxe/SdMmcPciHci.c
echo "=== 模式选择 1885-1950 ==="
awk 'NR>=1885 && NR<=1950' "$F"
