## @file
#
#  Redmi Note 4X (mido) platform description.
#  SoC-level definitions live in Platform/Qualcomm/msm8953/msm8953.dsc;
#  this file only adds device specific PCD overrides.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
#
##

[Defines]
  VENDOR_NAME                    = Xiaomi
  PLATFORM_NAME                  = mido
  PLATFORM_GUID                  = 7a3c1e52-2f4b-4d8a-9c61-5e0f3b2a84d7
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION              = 0x00010019
  OUTPUT_DIRECTORY               = Build/$(PLATFORM_NAME)
  SUPPORTED_ARCHITECTURES        = AARCH64
  BUILD_TARGETS                  = DEBUG|RELEASE
  SKUID_IDENTIFIER               = DEFAULT
  FLASH_DEFINITION               = Platform/Qualcomm/msm8953/msm8953.fdf
  DEVICE_DXE_FV_COMPONENTS       = Platform/Xiaomi/msm8953/mido.fdf.inc

!include Platform/Qualcomm/msm8953/msm8953.dsc

[BuildOptions.common]
  GCC:*_*_AARCH64_CC_FLAGS = -DENABLE_SIMPLE_INIT

[PcdsFixedAtBuild.common]
  # 5.5" 1080x1920, framebuffer is the lk2nd cont-splash buffer at 0x90001000
  gQcomTokenSpaceGuid.PcdMipiFrameBufferWidth|1080
  gQcomTokenSpaceGuid.PcdMipiFrameBufferHeight|1920

  # lk2nd leaves the panel scanning a 24bpp RGB888 buffer (stride 3240),
  # which matches the mainline simple-framebuffer description for mido.
  gQcomTokenSpaceGuid.PcdMipiFrameBufferPixelBpp|24|UINT32|0x0000a403
  gQcomTokenSpaceGuid.PcdMipiFrameBufferVisibleWidth|1080|UINT32|0x0000a404
  gQcomTokenSpaceGuid.PcdMipiFrameBufferVisibleHeight|1920|UINT32|0x0000a405

  gRenegadePkgTokenSpaceGuid.PcdDeviceVendor|"Xiaomi"
  gRenegadePkgTokenSpaceGuid.PcdDeviceProduct|"Redmi Note 4X"
  gRenegadePkgTokenSpaceGuid.PcdDeviceCodeName|"mido"

  # Simple Init
  gSimpleInitTokenSpaceGuid.PcdGuiDefaultDPI|401

[Components.common]
  # Volume-Up button -> SimpleTextIn (short press = Down, long press = Enter)
  Platform/Xiaomi/msm8953/ButtonsDxe/ButtonsDxe.inf

  #
  # eMMC: our platform driver registers SDCC1's SDHCI window as a
  # non-discoverable PCI device; the generic EDK2 stack does the rest.
  #
  Platform/Xiaomi/msm8953/SdhcPlatformDxe/SdhcPlatformDxe.inf
  MdeModulePkg/Bus/Pci/NonDiscoverablePciDeviceDxe/NonDiscoverablePciDeviceDxe.inf
  MdeModulePkg/Bus/Pci/SdMmcPciHcDxe/SdMmcPciHcDxe.inf
  MdeModulePkg/Bus/Sd/EmmcDxe/EmmcDxe.inf

  # eMMC bring-up diagnostic application (invoked from startup.nsh)
  Platform/Xiaomi/msm8953/MmcProbe/MmcProbe.inf
