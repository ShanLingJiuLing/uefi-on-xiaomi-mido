#!/bin/bash
cd $HOME/edk2-msm
F=Common/edk2/MdeModulePkg/Bus/Pci/SdMmcPciHcDxe/SdMmcPciHci.c
echo "=== RESPONSE 相关 ==="
grep -n 'RESPONSE\|Resp\[' "$F" | head -20
echo
echo "=== 响应读取上下文 ==="
LN=$(grep -n 'SD_MMC_HC_RESPONSE' "$F" | head -1 | cut -d: -f1)
echo "first at line $LN"
awk -v s=$((LN-25)) -v e=$((LN+35)) 'NR>=s && NR<=e' "$F"
