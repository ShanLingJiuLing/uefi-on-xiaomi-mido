#!/bin/bash
cd $HOME/edk2-msm
H=Common/edk2/MdePkg/Include/Protocol/SdMmcPassThru.h
echo "=== 文件行数 ==="; wc -l "$H"
echo "=== 结构体与函数指针 ==="
grep -nE 'EFI_SD_MMC_|typedef|UINT|SendCommand|GetNextSlot|ResetDevice|Slot|ResponseType|CommandIndex' "$H" | head -60
