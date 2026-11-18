/** @file
  Register the MSM8953 SDCC controllers (eMMC on SDCC1, microSD on SDCC2) as
  non-discoverable devices so the generic EDK2 SD/MMC stack can bind to them.

  SDCC1 (eMMC) is already clocked when lk2nd hands off, because lk2nd reads
  boot.img from it. SDCC2 (microSD) is NOT: lk2nd never touches that slot
  (the Linux kernel brings it up itself), so its GCC branch clocks are gated
  off and even a register read raises a Synchronous External Abort
  (measured: FAR = 0x078649FE, i.e. SDCC2_SDHCI_BASE + VERSION).

  This driver therefore programs the SDCC2 clock tree itself, using the
  encodings from lk2nd (platform/msm_shared/clock_lib2.{c,h}):

      F(50000000, gpll0, 16, 0, 0)
        m_val       = 0
        n_val       = ~((0-0)) * !!(0) = 0
        d_val       = ~0 = 0xFFFFFFFF
        div_src_val = BVAL(4,0, 2*16-1) | BVAL(10,8, gpll0_source_val=1)
                    = 0x1F | 0x100 = 0x11F

  Any valid clock is enough: the SDHCI core derives the card clock from the
  base clock reported in CAPABILITIES, so a slower-than-nominal input merely
  shifts the real card clock down, which cards tolerate during init.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/NonDiscoverableDeviceRegistrationLib.h>
#include <Library/UefiBootServicesTableLib.h>

#define SDCC1_CORE_BASE    0x07824000ULL
#define SDCC1_SDHCI_BASE   0x07824900ULL
#define SDCC2_CORE_BASE    0x07864000ULL
#define SDCC2_SDHCI_BASE   0x07864900ULL
#define SDCC_WINDOW_SIZE   0x00001000ULL

//
// GCC clock control (lk2nd platform/msm8953/include/platform/iomap.h)
//
#define GCC_SDCC2_BCR          0x01843000U
#define GCC_SDCC2_CMD_RCGR     0x01843004U
#define GCC_SDCC2_CFG_RCGR     0x01843008U
#define GCC_SDCC2_M            0x0184300CU
#define GCC_SDCC2_N            0x01843010U
#define GCC_SDCC2_D            0x01843014U
#define GCC_SDCC2_APPS_CBCR    0x01843018U
#define GCC_SDCC2_AHB_CBCR     0x0184301CU

#define CBCR_BRANCH_ENABLE     (1U << 0)
#define CBCR_BRANCH_OFF        (1U << 31)
#define CMD_RCG_UPDATE         (1U << 0)

#define CFG_SRC_DIV_MASK       (0x1FU << 0)
#define CFG_SRC_SEL_MASK       (0x7U << 8)
#define CFG_MODE_MASK          (0x3U << 12)
#define CFG_SDCC2_50MHZ        ((1U << 8) | 0x1FU)   // src=GPLL0, div=16

//
// Vendor registers (lk2nd platform/msm_shared/sdhci_msm.h)
//
#define SDCC_VENDOR_FUNC_REG   0x010C
#define SDCC_VENDOR_FUNC_VAL   0x0A1C
#define SDCC_MCI_HC_MODE       0x0078
#define HC_MODE_EN             (1U << 0)
#define FF_CLK_SW_RST_DIS      (1U << 13)

//
// SDHCI standard registers. Widths matter: VERSION (0xFE) and
// CLOCK_CONTROL (0x2C) are 16-bit, a 32-bit access at 0xFE traps.
//
#define SDHCI_PRESENT_STATE    0x24
#define SDHCI_CLOCK_CONTROL    0x2C
#define SDHCI_SOFTWARE_RESET   0x2F
#define SDHCI_CAPABILITIES     0x40
#define SDHCI_VERSION          0xFE

#define SDHCI_SCRATCH_PA       0x9F310000ULL
#define SDHCI_SCRATCH_MAGIC    0x4D49444FU   // 'MIDO'

STATIC
VOID
ScratchWrite (
  IN UINTN  Index,
  IN UINT32 Value
  )
{
  MmioWrite32 (SDHCI_SCRATCH_PA + (Index * 4ULL), Value);
}

STATIC
BOOLEAN
EnableBranch (
  IN UINT32  CbcrReg
  )
{
  UINTN  Spin;

  MmioOr32 (CbcrReg, CBCR_BRANCH_ENABLE);

  for (Spin = 0; Spin < 200; Spin++) {
    if ((MmioRead32 (CbcrReg) & CBCR_BRANCH_OFF) == 0) {
      return TRUE;
    }
    gBS->Stall (100);
  }

  return FALSE;
}

STATIC
VOID
EnableSdcc2Clocks (
  VOID
  )
{
  UINT32  Cfg;
  UINTN   Spin;

  //
  // 1. Take the block out of reset.
  //
  MmioWrite32 (GCC_SDCC2_BCR, 0x0);

  //
  // 2. Program the root clock generator: 50 MHz from GPLL0 (800 MHz / 16).
  //
  MmioWrite32 (GCC_SDCC2_M, 0);
  MmioWrite32 (GCC_SDCC2_N, 0);
  MmioWrite32 (GCC_SDCC2_D, 0xFFFFFFFF);

  Cfg  = MmioRead32 (GCC_SDCC2_CFG_RCGR);
  Cfg &= ~(CFG_SRC_DIV_MASK | CFG_SRC_SEL_MASK | CFG_MODE_MASK);
  Cfg |= CFG_SDCC2_50MHZ;
  MmioWrite32 (GCC_SDCC2_CFG_RCGR, Cfg);

  MmioOr32 (GCC_SDCC2_CMD_RCGR, CMD_RCG_UPDATE);
  for (Spin = 0; Spin < 200; Spin++) {
    if ((MmioRead32 (GCC_SDCC2_CMD_RCGR) & CMD_RCG_UPDATE) == 0) {
      break;
    }
    gBS->Stall (10);
  }

  //
  // 3. Ungate the bus and core branches.
  //
  EnableBranch (GCC_SDCC2_AHB_CBCR);
  EnableBranch (GCC_SDCC2_APPS_CBCR);
}

STATIC
BOOLEAN
BringUpController (
  IN UINT64  CoreBase,
  IN UINT64  SdhciBase
  )
{
  UINT32   HcMode;
  UINTN    Spin;
  UINT8    ResetStatus;
  BOOLEAN  Ok;

  //
  // SDHCI (HC) mode, exactly like lk2nd's sdhci_msm_init().
  // NOTE the lk2nd macro: REG_WRITE32(host, v, a) -> value first, then reg.
  //
  MmioWrite32 (SdhciBase + SDCC_VENDOR_FUNC_REG, SDCC_VENDOR_FUNC_VAL);

  HcMode  = MmioRead32 (CoreBase + SDCC_MCI_HC_MODE);
  HcMode |= (HC_MODE_EN | FF_CLK_SW_RST_DIS);
  MmioWrite32 (CoreBase + SDCC_MCI_HC_MODE, HcMode);

  MmioWrite8 (SdhciBase + SDHCI_SOFTWARE_RESET, 0x01);

  Ok = FALSE;
  for (Spin = 0; Spin < 2000; Spin++) {
    ResetStatus = MmioRead8 (SdhciBase + SDHCI_SOFTWARE_RESET);
    if ((ResetStatus & 0x01) == 0) {
      Ok = TRUE;
      break;
    }
    gBS->Stall (100);
  }

  return Ok;
}

STATIC
EFI_STATUS
RegisterSdhci (
  IN UINT64  SdhciBase
  )
{
  return RegisterNonDiscoverableMmioDevice (
           NonDiscoverableDeviceTypeSdhci,
           //
           // The SDCC is not cache-coherent, but declaring it
           // NonCoherent breaks NonDiscoverablePciDeviceDxe on this platform
           // (every command started failing), so keep Coherent and handle
           // the DMA problem from the transfer side instead.
           //
           NonDiscoverableDeviceDmaTypeCoherent,
           NULL,
           NULL,
           1,
           SdhciBase,
           SDCC_WINDOW_SIZE
           );
}

EFI_STATUS
EFIAPI
SdhcPlatformDxeEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS            Status1;
  EFI_STATUS            Status2;
  EFI_STATUS            Status;
  EFI_PHYSICAL_ADDRESS  ScratchPage;
  BOOLEAN               Ok1;
  BOOLEAN               Ok2;

  //
  // Reserve the scratch page so nothing else hands it out and overwrites our
  // markers (we saw random pool strings appear there: "ligned.c").
  //
  ScratchPage = SDHCI_SCRATCH_PA;
  Status      = gBS->AllocatePages (AllocateAddress, EfiBootServicesData, 1, &ScratchPage);

  ScratchWrite (0, SDHCI_SCRATCH_MAGIC);
  ScratchWrite (24, (UINT32)Status);   // 0 = page reserved successfully

  //
  // ---- SDCC1 (eMMC): already clocked by lk2nd ----
  //
  ScratchWrite (1, MmioRead16 (SDCC1_SDHCI_BASE + SDHCI_VERSION));
  ScratchWrite (2, MmioRead32 (SDCC1_SDHCI_BASE + SDHCI_CAPABILITIES));
  ScratchWrite (3, MmioRead32 (SDCC1_SDHCI_BASE + SDHCI_PRESENT_STATE));
  ScratchWrite (4, MmioRead16 (SDCC1_SDHCI_BASE + SDHCI_CLOCK_CONTROL));
  ScratchWrite (5, MmioRead32 (SDCC1_CORE_BASE + SDCC_MCI_HC_MODE));

  Ok1 = BringUpController (SDCC1_CORE_BASE, SDCC1_SDHCI_BASE);

  ScratchWrite (6, Ok1 ? 0 : 1);
  ScratchWrite (8, MmioRead32 (SDCC1_SDHCI_BASE + SDCC_VENDOR_FUNC_REG));
  ScratchWrite (11, MmioRead32 (SDCC1_CORE_BASE + SDCC_MCI_HC_MODE));

  //
  // ---- SDCC2 (microSD): clock tree must be brought up first ----
  //
  ScratchWrite (12, MmioRead32 (GCC_SDCC2_BCR));
  ScratchWrite (13, MmioRead32 (GCC_SDCC2_APPS_CBCR));
  ScratchWrite (14, MmioRead32 (GCC_SDCC2_AHB_CBCR));

  EnableSdcc2Clocks ();

  ScratchWrite (17, MmioRead32 (GCC_SDCC2_APPS_CBCR));
  ScratchWrite (18, MmioRead32 (GCC_SDCC2_AHB_CBCR));
  ScratchWrite (20, MmioRead32 (GCC_SDCC2_CFG_RCGR));
  ScratchWrite (21, MmioRead32 (GCC_SDCC2_CMD_RCGR));

  //
  // First touch of the SDCC2 register window. If the clock bring-up failed
  // this raises a Synchronous External Abort instead of returning.
  //
  ScratchWrite (15, MmioRead16 (SDCC2_SDHCI_BASE + SDHCI_VERSION));
  ScratchWrite (19, MmioRead32 (SDCC2_SDHCI_BASE + SDHCI_CAPABILITIES));

  //
  // Direct test of the base clock: enable the controller's internal clock and
  // see whether the "internal clock stable" bit actually comes up. If it does
  // not, the RCG is not producing a clock and SdMmcHcInitHost() will fail,
  // which leaves the slot with MediaPresent = FALSE ("No Media" on every
  // command).
  //
  MmioWrite16 (SDCC2_SDHCI_BASE + SDHCI_CLOCK_CONTROL, 0x0001);
  {
    UINTN  Spin2;
    UINT16 ClkCtl;
    UINT16 StableSpin = 0xFFFF;

    for (Spin2 = 0; Spin2 < 200; Spin2++) {
      ClkCtl = MmioRead16 (SDCC2_SDHCI_BASE + SDHCI_CLOCK_CONTROL);
      if ((ClkCtl & 0x0002) != 0) {
        StableSpin = (UINT16)Spin2;
        break;
      }
      gBS->Stall (100);
    }

    ScratchWrite (22, StableSpin);
    ScratchWrite (23, MmioRead16 (SDCC2_SDHCI_BASE + SDHCI_CLOCK_CONTROL));
  }

  Ok2 = BringUpController (SDCC2_CORE_BASE, SDCC2_SDHCI_BASE);
  ScratchWrite (16, Ok2 ? 0 : 1);

  Status1 = RegisterSdhci (SDCC1_SDHCI_BASE);
  Status2 = RegisterSdhci (SDCC2_SDHCI_BASE);

  ScratchWrite (9, (UINT32)Status1);
  ScratchWrite (10, (UINT32)Status2);

  DEBUG (
    (DEBUG_ERROR,
     "[mido] SDCC1 reset=%d reg=%r, SDCC2 reset=%d reg=%r\n",
     Ok1, Status1, Ok2, Status2)
    );

  return EFI_SUCCESS;
}
