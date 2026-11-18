## @file
#
#  Qualcomm MSM8953 (Snapdragon 625) SoC level description.
#  All addresses below were measured on a Redmi Note 4X (mido), see
#  mido-uefi-feasibility.md sections 1.3 / 1.8.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
#
##

################################################################################
#
# Defines Section - statements that will be processed to create a Makefile.
#
################################################################################

[Defines]
  SOC_PLATFORM           = MSM8953
  USE_PHYSICAL_TIMER     = TRUE

!include Silicon/Qualcomm/QcomPkg/QcomCommonDsc.inc

[PcdsFixedAtBuild.common]
  #
  # DRAM: 3 GiB at 0x4000_0000 (measured, memory node of the live DTB)
  #
  gArmTokenSpaceGuid.PcdSystemMemoryBase|0x40000000
  gArmTokenSpaceGuid.PcdSystemMemorySize|0xC0000000

  gArmTokenSpaceGuid.PcdCpuVectorBaseAddress|0x91400000     # CPU Vectors
  gArmTokenSpaceGuid.PcdArmArchTimerFreqInHz|19200000
  #
  # GIC INTIDs of the generic timer PPIs. /proc/interrupts on the running
  # device reports the arch_timer on GIC INTID 20.
  #
  gArmTokenSpaceGuid.PcdArmArchTimerSecIntrNum|18
  gArmTokenSpaceGuid.PcdArmArchTimerIntrNum|20
  #
  # GICv2 (compatible "qcom,msm-qgic2"): distributor at 0x0B000000,
  # CPU interface at 0x0B002000. There are no redistributors on GICv2.
  #
  # PcdGicInterruptInterfaceBase MUST be set: it is a [Pcd.common] (dynamic)
  # PCD whose default is 0, and ArmGicV2Dxe feeds it straight into
  # MmioWrite32(mGicInterruptInterfaceBase + ARM_GIC_ICCBPR, 7). Leaving it
  # unset produced a write to address 0xB and a second-level translation
  # fault (data abort) inside ArmGicDxe.
  #
  gArmTokenSpaceGuid.PcdGicDistributorBase|0x0B000000
  gArmTokenSpaceGuid.PcdGicInterruptInterfaceBase|0x0B002000
  gArmTokenSpaceGuid.PcdGicRedistributorsBase|0x0B002000

  gEfiMdeModulePkgTokenSpaceGuid.PcdAcpiDefaultOemRevision|0x00008953
  gEmbeddedTokenSpaceGuid.PcdPrePiStackBase|0x91404000       # UEFI Stack
  gEmbeddedTokenSpaceGuid.PcdPrePiStackSize|0x00040000       # 256K stack
  gEmbeddedTokenSpaceGuid.PcdPrePiCpuIoSize|44

  gQcomTokenSpaceGuid.PcdUefiMemPoolBase|0xA0000000          # DXE Heap base
  gQcomTokenSpaceGuid.PcdUefiMemPoolSize|0x60000000          # 1.5 GiB
  gQcomTokenSpaceGuid.PcdMipiFrameBufferAddress|0x90001000   # lk2nd cont splash

  gArmPlatformTokenSpaceGuid.PcdCoreCount|8
  gArmPlatformTokenSpaceGuid.PcdClusterCount|1

  #
  # SimpleInit
  #
  gSimpleInitTokenSpaceGuid.PcdDeviceTreeStore|0x9F200000
  gSimpleInitTokenSpaceGuid.PcdLoggerdUseConsole|FALSE

[LibraryClasses.common]

  # Ported from SurfaceDuoPkg
  PlatformMemoryMapLib|Platform/$(VENDOR_NAME)/msm8953/Library/$(PLATFORM_NAME)/PlatformMemoryMapLib/PlatformMemoryMapLib.inf

  PlatformPeiLib|Silicon/Qualcomm/msm8953/Library/PlatformPeiLib/PlatformPeiLib.inf
  PlatformPrePiLib|Silicon/Qualcomm/msm8953/Library/PlatformPrePiLib/PlatformPrePiLib.inf
  AslUpdateLib|Silicon/Qualcomm/QcomPkg/Library/DxeAslUpdateLib/DxeAslUpdateLib.inf
  MsPlatformDevicesLib|Silicon/Qualcomm/msm8953/Library/MsPlatformDevicesLib/MsPlatformDevicesLib.inf
  SOCSmbiosInfoLib|Silicon/Qualcomm/msm8953/Library/SOCSmbiosInfoLib/SOCSmbiosInfoLib.inf

[Components.common]
  Platform/EFI_Binaries/Applications/LinuxSimpleMassStorage/LinuxSimpleMassStorage.inf
