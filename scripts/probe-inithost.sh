#!/bin/bash
cd $HOME/edk2-msm
F=Common/edk2/MdeModulePkg/Bus/Pci/SdMmcPciHcDxe/SdMmcPciHci.c
echo "=== SdMmcHcInitHost ==="
LN=$(grep -n 'SdMmcHcInitHost' "$F" | head -1 | cut -d: -f1)
echo "at line $LN"
awk -v s="$LN" -v e="$((LN+50))" 'NR>=s && NR<=e' "$F"
