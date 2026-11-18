/** @file
  MmcProbe - walks the SD/MMC identification sequence on every SDHCI
  controller registered by SdhcPlatformDxe, printing each step.

  The firmware has no UART and the shell console wipes early DEBUG output,
  so this is the only way to see exactly which command fails.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>

#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/IoLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

#include <Protocol/SdMmcPassThru.h>

STATIC UINT8  mBuf[512];

//
// SDHCI register windows, indexed like the SdMmcPassThru instances.
//
STATIC CONST UINT64  mSdhciBase[] = { 0x07824900ULL, 0x07864900ULL };

#define SDHCI_HOST_CTRL1   0x28
#define SDHCI_POWER_CTRL   0x29
#define HOST_CTRL1_4BIT    0x02
#define HOST_CTRL1_DMA_MASK (0x03 << 3)
#define HOST_CTRL1_ADMA32  (0x02 << 3)

STATIC
VOID
Force1BitBus (
  IN UINTN  Index
  )
{
  UINT8  Hc1;

  if (Index >= (sizeof (mSdhciBase) / sizeof (mSdhciBase[0]))) {
    return;
  }

  //
  // HOST_CTRL1 is an 8-bit register at 0x28; 0x29 is POWER_CONTROL, so a
  // 16-bit write here would clobber the power setting.
  //
  Hc1 = MmioRead8 (mSdhciBase[Index] + SDHCI_HOST_CTRL1);
  Print (L"[%u] HOST_CTRL1 = 0x%02x  POWER = 0x%02x\n",
         Index, Hc1, MmioRead8 (mSdhciBase[Index] + SDHCI_POWER_CTRL));

  //
  // 1-bit bus, and DMA select = ADMA2 32-bit to match what SdMmcPciHcDxe
  // chose (SysBus64V3 = 0, controller version 3.00). lk2nd left it on 64-bit.
  //
  Hc1 &= (UINT8)~HOST_CTRL1_4BIT;
  Hc1 &= (UINT8)~HOST_CTRL1_DMA_MASK;
  Hc1 |= (UINT8)HOST_CTRL1_ADMA32;

  MmioWrite8 (mSdhciBase[Index] + SDHCI_HOST_CTRL1, Hc1);
  Print (L"[%u] HOST_CTRL1 -> 0x%02x (1-bit, ADMA2-32b)\n",
         Index, MmioRead8 (mSdhciBase[Index] + SDHCI_HOST_CTRL1));
}

STATIC
EFI_STATUS
Send (
  IN  EFI_SD_MMC_PASS_THRU_PROTOCOL *Pt,
  IN  UINT8                          Slot,
  IN  UINT16                         Index,
  IN  UINT32                         Arg,
  IN  UINT32                         CmdType,
  IN  UINT32                         RespType,
  OUT EFI_SD_MMC_STATUS_BLOCK        *St
  )
{
  EFI_SD_MMC_COMMAND_BLOCK           Cmd;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET Pkt;

  ZeroMem (&Cmd, sizeof (Cmd));
  ZeroMem (&Pkt, sizeof (Pkt));
  ZeroMem (St, sizeof (*St));

  Cmd.CommandIndex    = Index;
  Cmd.CommandArgument = Arg;
  Cmd.CommandType     = CmdType;
  Cmd.ResponseType    = RespType;

  Pkt.Timeout        = 2500000;
  Pkt.SdMmcCmdBlk    = &Cmd;
  Pkt.SdMmcStatusBlk = St;

  return Pt->PassThru (Pt, Slot, &Pkt, NULL);
}

STATIC
EFI_STATUS
ReadBlock (
  IN  EFI_SD_MMC_PASS_THRU_PROTOCOL *Pt,
  IN  UINT8                          Slot,
  IN  UINT32                         Lba,
  OUT UINT8                         *Buf
  )
{
  EFI_SD_MMC_COMMAND_BLOCK           Cmd;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET Pkt;
  EFI_SD_MMC_STATUS_BLOCK            St;

  ZeroMem (&Cmd, sizeof (Cmd));
  ZeroMem (&Pkt, sizeof (Pkt));
  ZeroMem (&St, sizeof (St));
  ZeroMem (Buf, 512);

  Cmd.CommandIndex    = 17;
  Cmd.CommandArgument = Lba * 512;
  Cmd.CommandType     = SdMmcCommandTypeAdtc;
  Cmd.ResponseType    = SdMmcResponseTypeR1;

  Pkt.Timeout          = 5000000;
  Pkt.SdMmcCmdBlk      = &Cmd;
  Pkt.SdMmcStatusBlk   = &St;
  Pkt.InDataBuffer     = Buf;
  Pkt.InTransferLength = 512;

  return Pt->PassThru (Pt, Slot, &Pkt, NULL);
}

STATIC
VOID
ProbeOne (
  IN EFI_SD_MMC_PASS_THRU_PROTOCOL *Pt,
  IN UINTN                          Index
  )
{
  EFI_SD_MMC_STATUS_BLOCK  St;
  EFI_STATUS               Status;
  UINT8                    Slot;
  UINT16                   Rca;
  UINTN                    Retry;
  BOOLEAN                  IsSd;

  Print (L"\n--- controller %u ---\n", Index);

  Slot   = 0xFF;
  Status = Pt->GetNextSlot (Pt, &Slot);
  Print (L"[%u] GetNextSlot -> %r slot=%u\n", Index, Status, Slot);
  if (EFI_ERROR (Status)) {
    return;
  }

  //
  // lk2nd leaves the host in 8-bit mode, but a card that has just seen CMD0
  // is back on 1-bit. Command transfers work either way (single CMD line) but
  // data transfers fail on a width mismatch. Nobody sets the width for us
  // because we bypass EmmCxe, so do it here.
  //
  Force1BitBus (Index);

  //
  // FAST PATH: do not reset anything. lk2nd read boot.img from the eMMC just
  // before handing off, so the card may still be selected and sitting in the
  // "transfer" state. CMD16/CMD17 carry no RCA in their argument, so they can
  // work even though we do not know the RCA. This is the cheapest way to find
  // out whether the data path is usable as-is.
  //
  Print (L"[%u] --- fast path (no reset, card may already be selected) ---\n", Index);

  Status = Send (Pt, Slot, 13, 0, SdMmcCommandTypeAc, SdMmcResponseTypeR1, &St);
  Print (L"[%u] FAST CMD13 STATUS     -> %r status=0x%08x\n", Index, Status, St.Resp0);

  Status = Send (Pt, Slot, 16, 512, SdMmcCommandTypeAc, SdMmcResponseTypeR1, &St);
  Print (L"[%u] FAST CMD16 SET_BLKLEN -> %r\n", Index, Status);

  Status = ReadBlock (Pt, Slot, 0, mBuf);
  Print (L"[%u] FAST CMD17 READ(0)    -> %r sig=%02x%02x\n", Index, Status, mBuf[510], mBuf[511]);
  if (!EFI_ERROR (Status)) {
    Print (L"[%u] FAST bytes: %02x %02x %02x %02x %02x %02x %02x %02x\n", Index,
           mBuf[0], mBuf[1], mBuf[2], mBuf[3], mBuf[4], mBuf[5], mBuf[6], mBuf[7]);
  }

  Print (L"[%u] --- full identification path ---\n", Index);

  Status = Pt->ResetDevice (Pt, Slot);
  Print (L"[%u] ResetDevice -> %r\n", Index, Status);

  //
  // A host reset re-applies defaults; make sure we are back on 1-bit before
  // touching the data path again.
  //
  Force1BitBus (Index);

  //
  // SECOND FAST PATH: the host has now been re-initialised (400 kHz, 1-bit)
  // but a host reset does NOT reset the card, so it may still be selected
  // from lk2nd's session. CMD16/CMD17 carry no RCA in their argument, so try
  // the data path again at a clean clock before disturbing the card with CMD0.
  //
  Print (L"[%u] --- fast path 2 (host re-inited, card still selected?) ---\n", Index);

  Status = Send (Pt, Slot, 16, 512, SdMmcCommandTypeAc, SdMmcResponseTypeR1, &St);
  Print (L"[%u] F2 CMD16 SET_BLKLEN -> %r\n", Index, Status);

  Status = ReadBlock (Pt, Slot, 0, mBuf);
  Print (L"[%u] F2 CMD17 READ(0)    -> %r sig=%02x%02x\n", Index, Status, mBuf[510], mBuf[511]);
  if (!EFI_ERROR (Status)) {
    Print (L"[%u] F2 bytes: %02x %02x %02x %02x %02x %02x %02x %02x\n", Index,
           mBuf[0], mBuf[1], mBuf[2], mBuf[3], mBuf[4], mBuf[5], mBuf[6], mBuf[7]);
  }

  Status = Send (Pt, Slot, 0, 0, SdMmcCommandTypeBc, SdMmcResponseTypeR1, &St);
  Print (L"[%u] CMD0  GO_IDLE       -> %r\n", Index, Status);

  //
  // CMD8 only answers on SD (v2.0+) cards; success means "this is an SD card".
  //
  Status = Send (Pt, Slot, 8, 0x1AA, SdMmcCommandTypeBcr, SdMmcResponseTypeR7, &St);
  IsSd   = !EFI_ERROR (Status);
  Print (L"[%u] CMD8  SEND_IF_COND  -> %r (=> %a)\n", Index, Status, IsSd ? "SD" : "MMC");

  if (IsSd) {
    //
    // ACMD41: CMD55 (R1) then CMD41 (R3) until the card reports ready.
    //
    for (Retry = 0; Retry < 20; Retry++) {
      Status = Send (Pt, Slot, 55, 0, SdMmcCommandTypeAc, SdMmcResponseTypeR1, &St);
      if (EFI_ERROR (Status)) {
        break;
      }
      Status = Send (Pt, Slot, 41, 0x40FF8000, SdMmcCommandTypeBcr, SdMmcResponseTypeR3, &St);
      if (EFI_ERROR (Status) || ((St.Resp0 & 0x80000000U) != 0)) {
        break;
      }
      gBS->Stall (10000);
    }
    Print (L"[%u] ACMD41 SEND_OP_COND -> %r rc=%u ocr=0x%08x\n", Index, Status, Retry, St.Resp0);
  } else {
    for (Retry = 0; Retry < 20; Retry++) {
      Status = Send (Pt, Slot, 1, 0x40FF8080, SdMmcCommandTypeBcr, SdMmcResponseTypeR3, &St);
      if (EFI_ERROR (Status) || ((St.Resp0 & 0x80000000U) != 0)) {
        break;
      }
      gBS->Stall (10000);
    }
    Print (L"[%u] CMD1  SEND_OP_COND -> %r rc=%u ocr=0x%08x\n", Index, Status, Retry, St.Resp0);
  }

  Status = Send (Pt, Slot, 2, 0, SdMmcCommandTypeBcr, SdMmcResponseTypeR2, &St);
  Print (L"[%u] CMD2  ALL_SEND_CID  -> %r cid=0x%08x%08x%08x%08x\n",
         Index, Status, St.Resp0, St.Resp1, St.Resp2, St.Resp3);

  if (IsSd) {
    //
    // SD: the card assigns its own RCA and reports it in the R6 response.
    //
    Status = Send (Pt, Slot, 3, 0, SdMmcCommandTypeAc, SdMmcResponseTypeR6, &St);
    Rca    = (UINT16)(St.Resp0 >> 16);
    Print (L"[%u] CMD3  SET_REL_ADDR  -> %r rca=0x%04x (SD, card-chosen)\n", Index, Status, Rca);
  } else {
    //
    // eMMC: the HOST assigns the RCA and CMD3 answers with R1, not R6.
    // Mirroring lk2nd platform/msm_shared/mmc.c:mmc_boot_send_relative_address()
    //     cmd.argument  = (MMC_RCA << 16);   // MMC_RCA == 2
    //     cmd.cmd_type  = MMC_BOOT_CMD_ADDRESS;
    //     cmd.resp_type = MMC_BOOT_RESP_R1;
    //
    Rca    = 2;
    Status = Send (Pt, Slot, 3, (UINT32)Rca << 16, SdMmcCommandTypeAc, SdMmcResponseTypeR1, &St);
    Print (L"[%u] CMD3  SET_REL_ADDR  -> %r rca=0x%04x (MMC, host-assigned) status=0x%08x\n",
           Index, Status, Rca, St.Resp0);
  }

  Status = Send (Pt, Slot, 9, (UINT32)Rca << 16, SdMmcCommandTypeAc, SdMmcResponseTypeR2, &St);
  Print (L"[%u] CMD9  SEND_CSD      -> %r csd=0x%08x%08x\n", Index, Status, St.Resp0, St.Resp1);

  //
  // CMD7 with RCA=0 is a "deselect", so try the reported RCA first and fall
  // back to 1 when the card reported 0.
  //
  Status = Send (Pt, Slot, 7, (UINT32)Rca << 16, SdMmcCommandTypeAc, SdMmcResponseTypeR1, &St);
  Print (L"[%u] CMD7  SELECT rca=%u  -> %r\n", Index, Rca, Status);
  if (EFI_ERROR (Status) && (Rca == 0)) {
    Status = Send (Pt, Slot, 7, 1U << 16, SdMmcCommandTypeAc, SdMmcResponseTypeR1, &St);
    Print (L"[%u] CMD7  SELECT rca=1  -> %r\n", Index, Status);
  }

  Status = Send (Pt, Slot, 13, (UINT32)Rca << 16, SdMmcCommandTypeAc, SdMmcResponseTypeR1, &St);
  Print (L"[%u] CMD13 SEND_STATUS   -> %r status=0x%08x\n", Index, Status, St.Resp0);

  Status = Send (Pt, Slot, 16, 512, SdMmcCommandTypeAc, SdMmcResponseTypeR1, &St);
  Print (L"[%u] CMD16 SET_BLOCKLEN  -> %r\n", Index, Status);

  Status = ReadBlock (Pt, Slot, 0, mBuf);
  Print (L"[%u] CMD17 READ_BLOCK(0) -> %r sig=%02x%02x\n", Index, Status, mBuf[510], mBuf[511]);
  if (!EFI_ERROR (Status)) {
    Print (L"[%u] bytes: %02x %02x %02x %02x %02x %02x %02x %02x\n", Index,
           mBuf[0], mBuf[1], mBuf[2], mBuf[3], mBuf[4], mBuf[5], mBuf[6], mBuf[7]);
  }
}

EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_HANDLE  *Handles = NULL;
  UINTN       Count = 0;
  UINTN       Index;
  EFI_STATUS  Status;

  Print (L"\n=== MmcProbe ===\n");

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiSdMmcPassThruProtocolGuid,
                  NULL,
                  &Count,
                  &Handles
                  );
  Print (L"MmcProbe: SdMmcPassThru instances=%u (%r)\n", Count, Status);
  if (EFI_ERROR (Status) || (Count == 0)) {
    Print (L"=== MmcProbe done ===\n");
    return EFI_SUCCESS;
  }

  for (Index = 0; Index < Count; Index++) {
    EFI_SD_MMC_PASS_THRU_PROTOCOL  *Pt = NULL;

    Status = gBS->HandleProtocol (Handles[Index], &gEfiSdMmcPassThruProtocolGuid, (VOID **)&Pt);
    if (EFI_ERROR (Status) || (Pt == NULL)) {
      Print (L"[%u] HandleProtocol -> %r\n", Index, Status);
      continue;
    }
    ProbeOne (Pt, Index);
  }

  FreePool (Handles);
  Print (L"=== MmcProbe done ===\n");
  return EFI_SUCCESS;
}
