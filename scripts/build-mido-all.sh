#!/bin/bash
# Canonical build script for mido.
# Installs every file we own, then does a clean rebuild.
# (Incremental builds have silently reused stale objects more than once.)
set -e
cd $HOME/edk2-msm || exit 1
export PATH=$PATH:/usr/sbin
export USE_IASL=true

echo "===== 1. 安装我方文件 ====="
D=Platform/Xiaomi/msm8953
mkdir -p $D/SdhcPlatformDxe $D/ButtonsDxe $D/MmcProbe

cp "$HOME/mido.dsc"            $D/mido.dsc
cp "$HOME/mido.fdf.inc"        $D/mido.fdf.inc
cp "$HOME/mido-startup.nsh"    $D/mido-startup.nsh
cp "$HOME/SdhcPlatformDxe.c"   $D/SdhcPlatformDxe/SdhcPlatformDxe.c
cp "$HOME/SdhcPlatformDxe.inf" $D/SdhcPlatformDxe/SdhcPlatformDxe.inf
cp "$HOME/ButtonsDxe.c"        $D/ButtonsDxe/ButtonsDxe.c
cp "$HOME/ButtonsDxe.inf"      $D/ButtonsDxe/ButtonsDxe.inf
cp "$HOME/MmcProbe.c"          $D/MmcProbe/MmcProbe.c
cp "$HOME/MmcProbe.inf"        $D/MmcProbe/MmcProbe.inf
cp "$HOME/SimpleFbDxe.c"       Silicon/Qualcomm/QcomPkg/Drivers/SimpleFbDxe/SimpleFbDxe.c
cp "$HOME/PlatformUtils_fixed.h" Silicon/Qualcomm/msm8953/Library/PlatformPrePiLib/PlatformUtils.h
cp "$HOME/Pi_instrumented.c"   Silicon/Qualcomm/QcomPkg/PrePi/Pi.c
cp "$HOME/BootShim_patched.S"  tools/BootShim/BootShim.S

echo "--- 指纹（确认装进去的是新文件）---"
md5sum $D/SdhcPlatformDxe/SdhcPlatformDxe.c $D/mido-startup.nsh
grep -n 'dmem\|dh -p' $D/mido-startup.nsh | head -3
grep -n 'SDCC_VENDOR_FUNC_REG\|SDCC_VENDOR_FUNC_VAL' $D/SdhcPlatformDxe/SdhcPlatformDxe.c | head -4 || true

echo "===== 2. 给 SdMmcPciHcDxe 打「eMMC 常驻」补丁 ====="
python3 - <<'PYEOF'
p = 'Common/edk2/MdeModulePkg/Bus/Pci/SdMmcPciHcDxe/SdMmcPciHci.c'
s = open(p).read()
old = """  if ((PresentState & BIT16) != 0) {
    *MediaPresent = TRUE;
  } else {
    *MediaPresent = FALSE;
  }"""
new = """  //
  // [mido] MSM8953's SDHCI does not assert the "Card Inserted" bit
  // (PRESENT_STATE bit16) for the soldered-on eMMC, which made every
  // command fail with EFI_NO_MEDIA. Treat the slot as always populated.
  //
  *MediaPresent = TRUE;
  (VOID)PresentState;"""
if new in s:
    print('already patched')
elif old in s:
    open(p, 'w').write(s.replace(old, new))
    print('patched')
else:
    print('PATTERN NOT FOUND - check the source')
PYEOF
grep -n 'mido' Common/edk2/MdeModulePkg/Bus/Pci/SdMmcPciHcDxe/SdMmcPciHci.c | head -3

echo "===== 2b. 撤销「槽位为内嵌 eMMC」补丁（现在同时支持 SDCC2=SD）====="
python3 - <<'PYEOF'
p = 'Common/edk2/MdeModulePkg/Bus/Pci/SdMmcPciHcDxe/SdMmcPciHcDxe.c'
s = open(p).read()
old = """    //
    // [mido] MSM8953 reports SlotType = Removable (CAPABILITIES bits 30:31
    // = 00) even for the soldered-on eMMC. BuildDevicePath() would then emit
    // an SD node and EmmcDxe rejects the controller with EFI_UNSUPPORTED.
    //
    Private->Slot[Slot].SlotType = EmbeddedSlot;"""
new = "    Private->Slot[Slot].SlotType = Private->Capability[Slot].SlotType;"
if old in s:
    open(p, 'w').write(s.replace(old, new, 1))
    print('reverted')
elif new in s:
    print('already upstream')
else:
    print('PATTERN NOT FOUND - check the source')
PYEOF
grep -c 'mido' Common/edk2/MdeModulePkg/Bus/Pci/SdMmcPciHcDxe/SdMmcPciHcDxe.c || true

echo "===== 2c. 给 DriverBindingStart 的三处 continue 加探针标记 ====="
python3 - <<'PYEOF'
p = 'Common/edk2/MdeModulePkg/Bus/Pci/SdMmcPciHcDxe/SdMmcPciHcDxe.c'
s = open(p).read()

old = """    Status = SdMmcHcReset (Private, Slot);
    if (EFI_ERROR (Status)) {
      continue;
    }

    //
    // Check whether there is a SD/MMC card attached
    //
    if (Private->Slot[Slot].SlotType == RemovableSlot) {
      Status = SdMmcHcCardDetect (PciIo, Slot, &MediaPresent);
      if (EFI_ERROR (Status) && (Status != EFI_MEDIA_CHANGED)) {
        continue;
      } else if (!MediaPresent) {
        DEBUG ((
          DEBUG_INFO,
          "SdMmcHcCardDetect: No device attached in Slot[%d]!!!\\n",
          Slot
          ));
        continue;
      }
    }

    Status = SdMmcHcInitHost (Private, Slot);
    if (EFI_ERROR (Status)) {
      continue;
    }"""

new = """    // [mido] probe markers: 28=reset fail, 29=carddetect fail, 30=inithost fail
    *(volatile UINT32 *)(0x9F310000 + 31 * 4) = Slot;

    Status = SdMmcHcReset (Private, Slot);
    if (EFI_ERROR (Status)) {
      *(volatile UINT32 *)(0x9F310000 + 28 * 4) = (UINT32)Status | 0x01000000;
      continue;
    }

    //
    // Check whether there is a SD/MMC card attached
    //
    if (Private->Slot[Slot].SlotType == RemovableSlot) {
      Status = SdMmcHcCardDetect (PciIo, Slot, &MediaPresent);
      if (EFI_ERROR (Status) && (Status != EFI_MEDIA_CHANGED)) {
        *(volatile UINT32 *)(0x9F310000 + 29 * 4) = (UINT32)Status | 0x02000000;
        continue;
      } else if (!MediaPresent) {
        *(volatile UINT32 *)(0x9F310000 + 29 * 4) = 0x02DEAD00;
        continue;
      }
    }

    Status = SdMmcHcInitHost (Private, Slot);
    if (EFI_ERROR (Status)) {
      *(volatile UINT32 *)(0x9F310000 + 30 * 4) = (UINT32)Status | 0x03000000;
      continue;
    }
    *(volatile UINT32 *)(0x9F310000 + 30 * 4) = 0x03000000;"""

if 'mido] probe markers' in s:
    print('already patched')
elif old in s:
    open(p, 'w').write(s.replace(old, new, 1))
    print('patched')
else:
    print('PATTERN NOT FOUND - check the source')
PYEOF
grep -n 'probe markers' Common/edk2/MdeModulePkg/Bus/Pci/SdMmcPciHcDxe/SdMmcPciHcDxe.c | head -2

echo "===== 2d. 旁路 NO_MEDIA 早退检查（让探针能真正发命令）====="
python3 - <<'PYEOF'
p = 'Common/edk2/MdeModulePkg/Bus/Pci/SdMmcPciHcDxe/SdMmcPciHcDxe.c'
s = open(p).read()
pats = [
    "if (!Private->Slot[Trb->Slot].MediaPresent) {",
    "if (!Private->Slot[Slot].MediaPresent) {",
]
n = 0
for pat in pats:
    if pat in s:
        s = s.replace(pat, "if (FALSE) {   // [mido] bypass media check")
        n += 1
if "mido] bypass media check" in s:
    open(p, 'w').write(s)
    print('bypassed %d pattern(s)' % n)
else:
    print('PATTERN NOT FOUND - check the source')
PYEOF
grep -c 'bypass media check' Common/edk2/MdeModulePkg/Bus/Pci/SdMmcPciHcDxe/SdMmcPciHcDxe.c || true

echo "===== 2e. 记录 SdMmcHcInitHost 内部两步的返回值 ====="
python3 - <<'PYEOF'
p = 'Common/edk2/MdeModulePkg/Bus/Pci/SdMmcPciHcDxe/SdMmcPciHci.c'
s = open(p).read()

old1 = """  Status = SdMmcHcInitV4Enhancements (PciIo, Slot, Capability, Private->ControllerVersion[Slot]);
  if (EFI_ERROR (Status)) {
    return Status;
  }"""
new1 = """  *(volatile UINT32 *)(0x9F310000 + 34 * 4) = Private->ControllerVersion[Slot];
  *(volatile UINT32 *)(0x9F310000 + 35 * 4) = Private->Capability[Slot].BaseClkFreq;
  Status = SdMmcHcInitV4Enhancements (PciIo, Slot, Capability, Private->ControllerVersion[Slot]);
  *(volatile UINT32 *)(0x9F310000 + 32 * 4) = (UINT32)Status | 0x04000000;
  if (EFI_ERROR (Status)) {
    return Status;
  }"""

old2 = """  Status = SdMmcHcClockSupply (Private, Slot, 0, TRUE, 400);
  if (EFI_ERROR (Status)) {
    return Status;
  }"""
new2 = """  Status = SdMmcHcClockSupply (Private, Slot, 0, TRUE, 400);
  *(volatile UINT32 *)(0x9F310000 + 33 * 4) = (UINT32)Status | 0x05000000;
  if (EFI_ERROR (Status)) {
    return Status;
  }"""

if 'mido] inithost probe' in s:
    print('already patched')
elif old1 in s and old2 in s:
    s = s.replace(old1, new1, 1).replace(old2, new2, 1)
    open(p, 'w').write(s)
    print('patched')
else:
    print('PATTERN NOT FOUND - check the source')
PYEOF
grep -n '0x04000000\|0x05000000' Common/edk2/MdeModulePkg/Bus/Pci/SdMmcPciHcDxe/SdMmcPciHci.c | head -3

echo "===== 2f. 只清 ADMA2（保留 SDMA）→ 驱动改用 SDMA 单缓冲 ====="
python3 - <<'PYEOF'
p = 'Common/edk2/MdeModulePkg/Bus/Pci/SdMmcPciHcDxe/SdMmcPciHci.c'
s = open(p).read()

old_pio = """  //
  // [mido] experiment: claim no DMA support so the driver uses PIO. Data
  // transfers currently time out in ADMA2 mode; PIO isolates DMA from the
  // card/bus itself.
  //
  Capability->Adma2 = 0;
  Capability->Sdma  = 0;

"""
if old_pio in s:
    s = s.replace(old_pio, "", 1)
    print('removed old PIO patch')

old = """  CopyMem (Capability, &Cap, sizeof (Cap));

  return EFI_SUCCESS;
}"""
new = """  CopyMem (Capability, &Cap, sizeof (Cap));

  //
  // [mido] experiment: drop ADMA2 only, so the driver falls back to SDMA.
  // SDMA needs no descriptor table, which isolates "the controller cannot
  // read our descriptors" from the data path itself.
  //
  Capability->Adma2 = 0;

  return EFI_SUCCESS;
}"""
if 'mido] experiment: drop ADMA2 only' in s:
    print('already patched')
elif old in s:
    s = s.replace(old, new, 1)
    print('patched')
else:
    print('PATTERN NOT FOUND - check the source')
open(p, 'w').write(s)
PYEOF
grep -n 'drop ADMA2 only' Common/edk2/MdeModulePkg/Bus/Pci/SdMmcPciHcDxe/SdMmcPciHci.c | head -2

echo "===== 3. 清理 + 全新构建 ====="
rm -rf workspace/Build/mido
rm -f boot-mido.img
./build.sh -d mido -t GCC5 -r RELEASE -b 2>&1 | grep -E 'SdhcPlatform|FVMAIN|Done|error|Error' | tail -10

echo "===== 4. 产物 ====="
FD=workspace/Build/mido/RELEASE_GCC5/FV/MSM8953_UEFI.fd
ls -l --time-style=full-iso $FD
md5sum $FD
cp boot-mido.img "$HOME/boot-mido.img"
md5sum "$HOME/boot-mido.img"
