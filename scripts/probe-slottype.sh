#!/bin/bash
cd $HOME/edk2-msm
D=Common/edk2/MdeModulePkg/Bus/Pci/SdMmcPciHcDxe
echo "=== SlotType 的定义与赋值 ==="
grep -rn 'SlotType' $D/*.c $D/*.h | head -20
echo
echo "=== BuildDevicePath 里的 SubType ==="
grep -n -B5 -A25 'BuildDevicePath' $D/SdMmcPciHcDxe.c | grep -nE 'SubType|SlotType|MSG_|Status' | head -20
echo
echo "=== EdkiiSdMmcOverride GetSlotType ==="
grep -rn 'GetSlotType' Common/edk2/MdeModulePkg/Include/Protocol/*.h | head -10
