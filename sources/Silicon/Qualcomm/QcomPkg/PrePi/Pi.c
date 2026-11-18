// Pi.c: Entry point for SEC(Security).
//
// [mido] instrumented with stage markers (M0..M10) printed at EFI_D_ERROR
// level so that they survive both RELEASE and DEBUG builds. Boot bring-up
// aid only; remove once the platform is stable.

#include <PiPei.h>
#include <Pi/PiBootMode.h>

#include <Library/PrePiLib.h>
#include <Library/PrintLib.h>
#include <Library/PrePiHobListPointerLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/PlatformPrePiLib.h>

#include <Ppi/GuidedSectionExtraction.h>

#include "Pi.h"

#define IS_XIP()  (((UINT64)FixedPcdGet64 (PcdFdBaseAddress) > mSystemMemoryEnd) ||\
                  ((FixedPcdGet64 (PcdFdBaseAddress) + FixedPcdGet32 (PcdFdSize)) <= FixedPcdGet64 (PcdSystemMemoryBase)))

UINT64  mSystemMemoryEnd = FixedPcdGet64 (PcdSystemMemoryBase) +
                           FixedPcdGet64 (PcdSystemMemorySize) - 1;

VOID EFIAPI ProcessLibraryConstructorList(VOID);

VOID
PrePiMain(
  IN VOID *StackBase,
  IN UINTN StackSize
  )
{

  EFI_HOB_HANDOFF_INFO_TABLE *HobList;
  EFI_STATUS                  Status;

  UINTN MemoryBase     = 0;
  UINTN MemorySize     = 0;
  UINTN UefiMemoryBase = 0;
  UINTN UefiMemorySize = 0;

  DEBUG((EFI_D_ERROR, "[mido] M0 enter PrePiMain, EL=%d\n", (UINT32)ArmReadCurrentEL()));

  // Architecture-specific initialization
  // Enable Floating Point
  ArmEnableVFP();
  DEBUG((EFI_D_ERROR, "[mido] M1 vfp ok\n"));

  if (ArmReadCurrentEL() == AARCH64_EL2) {
    DEBUG((EFI_D_ERROR, "[mido] M2 at EL2, setting HCR/CnthCtl\n"));
    // Trap General Exceptions. All exceptions that would be routed to EL1 are routed to EL2
    ArmWriteHcr(ARM_HCR_TGE);

    /* Enable Timer access for non-secure EL1 and EL0
       The cnthctl_el2 register bits are architecturally
       UNKNOWN on reset.
       Disable event stream as it is not in use at this stage
    */
    ArmWriteCntHctl(CNTHCTL_EL2_EL1PCTEN | CNTHCTL_EL2_EL1PCEN);
    DEBUG((EFI_D_ERROR, "[mido] M3 hcr/cnthctl ok\n"));
  }

  /* Enable program flow prediction, if supported */
  ArmEnableBranchPrediction();
  DEBUG((EFI_D_ERROR, "[mido] M4 branchpred ok\n"));

  // Declare UEFI region
  MemoryBase     = FixedPcdGet32(PcdSystemMemoryBase);
  MemorySize     = FixedPcdGet32(PcdSystemMemorySize);
  UefiMemoryBase = FixedPcdGet32(PcdUefiMemPoolBase);
  UefiMemorySize = FixedPcdGet32(PcdUefiMemPoolSize);
  StackBase      = (VOID *)(UefiMemoryBase + UefiMemorySize - StackSize);

  DEBUG(
      (EFI_D_ERROR,
       "[mido] M5 mem: base=0x%llx size=0x%llx pool=0x%llx poolsz=0x%llx stack=0x%llx stacksz=0x%llx\n",
       (UINT64)MemoryBase, (UINT64)MemorySize, (UINT64)UefiMemoryBase,
       (UINT64)UefiMemorySize, (UINT64)(UINTN)StackBase, (UINT64)StackSize));

  // Set up HOB
  HobList = HobConstructor(
      (VOID *)UefiMemoryBase, UefiMemorySize, (VOID *)UefiMemoryBase,
      StackBase);
  DEBUG((EFI_D_ERROR, "[mido] M6 hob ok\n"));

  PrePeiSetHobList (HobList);

  // Invalidate cache
  DEBUG((EFI_D_ERROR, "[mido] M7 invalidating FD cache\n"));
  InvalidateDataCacheRange(
      (VOID *)(UINTN)PcdGet64(PcdFdBaseAddress), PcdGet32(PcdFdSize));
  DEBUG((EFI_D_ERROR, "[mido] M8 cache invalidated\n"));

  // Initialize MMU
  DEBUG((EFI_D_ERROR, "[mido] M9 MemoryPeim in\n"));
  Status = MemoryPeim(UefiMemoryBase, UefiMemorySize);
  DEBUG((EFI_D_ERROR, "[mido] M10 MemoryPeim out: %r\n", Status));
  ASSERT_EFI_ERROR (Status);

  // Add HOBs
  BuildStackHob ((UINTN)StackBase, StackSize);

  // TODO: Call CpuPei as a library
  BuildCpuHob (ArmGetPhysicalAddressBits (), PcdGet8 (PcdPrePiCpuIoSize));

  // Set the Boot Mode
  SetBootMode (BOOT_WITH_DEFAULT_SETTINGS);

  // Initialize Platform HOBs (CpuHob and FvHob)
  DEBUG((EFI_D_ERROR, "[mido] M11 PlatformPeim in\n"));
  Status = PlatformPeim();
  DEBUG((EFI_D_ERROR, "[mido] M12 PlatformPeim out: %r\n", Status));
  ASSERT_EFI_ERROR (Status);

  // Now, the HOB List has been initialized, we can register performance information
  // PERF_START (NULL, "PEI", NULL, StartTimeStamp);

  // SEC phase needs to run library constructors by hand.
  DEBUG((EFI_D_ERROR, "[mido] M13 constructors in\n"));
  ProcessLibraryConstructorList();
  DEBUG((EFI_D_ERROR, "[mido] M14 constructors out\n"));

  // Assume the FV that contains the SEC (our code) also contains a compressed FV.
  DEBUG((EFI_D_ERROR, "[mido] M15 DecompressFirstFv in\n"));
  Status = DecompressFirstFv();
  DEBUG((EFI_D_ERROR, "[mido] M16 DecompressFirstFv out: %r\n", Status));
  ASSERT_EFI_ERROR (Status);

  // Load the DXE Core and transfer control to it
  DEBUG((EFI_D_ERROR, "[mido] M17 LoadDxeCoreFromFv in\n"));
  Status = LoadDxeCoreFromFv(NULL, 0);
  DEBUG((EFI_D_ERROR, "[mido] M18 LoadDxeCoreFromFv out: %r (should never return)\n", Status));
  ASSERT_EFI_ERROR (Status);
}

VOID
CEntryPoint(
  IN VOID *StackBase,
  IN UINTN StackSize
  )
{
  // Do platform specific initialization here
  DEBUG((EFI_D_ERROR, "[mido] CEntryPoint in\n"));
  PlatformInitialize();
  DEBUG((EFI_D_ERROR, "[mido] PlatformInitialize returned\n"));

  // Goto primary Main.
  PrePiMain(StackBase, StackSize);

  // DXE Core should always load and never return
  ASSERT(FALSE);
}

VOID
SecondaryCEntryPoint(
  IN  UINTN  MpId
  )
{
  // We must never get into this function on UniCore system
  ASSERT(FALSE);
}
