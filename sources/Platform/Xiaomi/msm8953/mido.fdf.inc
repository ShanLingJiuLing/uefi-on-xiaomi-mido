// per-device BSP DXEs for Xiaomi Redmi Note 4X (mido)
//
// ACPI tables and the mainline FDT blob that other devices ship here are
// intentionally omitted: the firmware currently runs DSDT-less.

// Volume-Up on TLMM GPIO 85 -> EFI_SIMPLE_TEXT_INPUT_PROTOCOL
// Short press moves the selection down, a >=800 ms hold acts as Enter.
INF Platform/Xiaomi/msm8953/ButtonsDxe/ButtonsDxe.inf

//
// eMMC over SDCC1.
//   SdhcPlatformDxe          registers 0x07824900 as a non-discoverable device
//   NonDiscoverablePciDeviceDxe  turns it into a fake PCI IO instance
//   SdMmcPciHcDxe            drives the SDHCI controller (SdMmcPassThru)
//   EmmCxe                   enumerates the eMMC and produces BlockIo
//
INF Platform/Xiaomi/msm8953/SdhcPlatformDxe/SdhcPlatformDxe.inf
INF MdeModulePkg/Bus/Pci/NonDiscoverablePciDeviceDxe/NonDiscoverablePciDeviceDxe.inf
INF MdeModulePkg/Bus/Pci/SdMmcPciHcDxe/SdMmcPciHcDxe.inf
INF MdeModulePkg/Bus/Sd/EmmcDxe/EmmcDxe.inf

// eMMC bring-up diagnostic application
INF Platform/Xiaomi/msm8953/MmcProbe/MmcProbe.inf

//
// Auto-run storage probe at shell start, so the result is visible without
// any keyboard (we only have a single GPIO button so far).
//
FILE FREEFORM = 7b1e2c94-6a3d-4f58-9c27-8d5a41b3e6f2 {
  SECTION RAW = Platform/Xiaomi/msm8953/mido-startup.nsh
  SECTION UI  = "startup.nsh"
}
