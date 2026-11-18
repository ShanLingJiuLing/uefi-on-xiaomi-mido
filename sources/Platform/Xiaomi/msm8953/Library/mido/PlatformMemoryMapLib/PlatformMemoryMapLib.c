/*
 * PlatformMemoryMapLib for Xiaomi Redmi Note 4X (mido), MSM8953, 3 GiB RAM
 *
 * Layout derived from /proc/iomem + the reserved-memory nodes of the live DTB
 * (see mido-uefi-feasibility.md section 1.3):
 *
 *   0x40000000 - 0x849FFFFF  usable
 *   0x84A00000 - 0x867FFFFF  TZ / other_ext (30 MiB)          -> reserved
 *   0x86800000 - 0x86BFFFFF  usable (4 MiB)
 *   0x86C00000 - 0x8D5FFFFF  modem firmware (106 MiB)         -> reserved
 *   0x8D600000 - 0x8E6FFFFF  adsp firmware (17 MiB)           -> reserved
 *   0x8E700000 - 0x8EDFFFFF  wcnss firmware (7 MiB)           -> reserved
 *   0x8EE00000 - 0x8F4FFFFF  UEFI FD (FD_BASE, 7 MiB)
 *   0x8F500000 - 0x8FFFFFFF  usable
 *   0x90000000 - 0x90000FFF  dfps data                        -> reserved
 *   0x90001000 - 0x913FFFFF  lk2nd contiguous splash / GOP    -> reserved
 *   0x91400000 - 0x9FEFFFFF  usable (vectors, stack, ram)
 *   0x9FF00000 - 0x9FFFFFFF  pstore / ramoops                -> reserved
 *   0xA0000000 - 0xFFFFFFFF  DXE heap
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#include <Library/BaseLib.h>
#include <Library/PlatformMemoryMapLib.h>

static ARM_MEMORY_REGION_DESCRIPTOR_EX gDeviceMemoryDescriptorEx[] = {
  /* ---- usable DRAM, low bank ---- */
  {"RAM Partition",    0x40000000, 0x44A00000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv,   WRITE_BACK_XN},

  /* ---- TZ / qseecom carve-out ---- */
  {"Removed Mem",      0x84A00000, 0x01E00000, AddMem, SYS_MEM, SYS_MEM_CAP, Reserv, NS_DEVICE},

  {"RAM Partition",    0x86800000, 0x00400000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv,   WRITE_BACK_XN},

  /* ---- remote processor firmware ---- */
  {"MODEM FW",         0x86C00000, 0x06A00000, AddMem, SYS_MEM, SYS_MEM_CAP, Reserv, NS_DEVICE},
  {"ADSP FW",          0x8D600000, 0x01100000, AddMem, SYS_MEM, SYS_MEM_CAP, Reserv, NS_DEVICE},
  {"WCNSS FW",         0x8E700000, 0x00700000, AddMem, SYS_MEM, SYS_MEM_CAP, Reserv, NS_DEVICE},

  /* ---- UEFI firmware volume ---- */
  {"UEFI FD",          0x8EE00000, 0x00700000, AddMem, SYS_MEM, SYS_MEM_CAP, BsData, WRITE_BACK},

  {"RAM Partition",    0x8F500000, 0x00B00000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv,   WRITE_BACK_XN},

  /* ---- display ---- */
  {"DFPS Data",        0x90000000, 0x00001000, AddMem, SYS_MEM, SYS_MEM_CAP, Reserv, WRITE_BACK_XN},
  {"Display Reserved", 0x90001000, 0x013FF000, AddMem, MEM_RES, SYS_MEM_CAP, Reserv, WRITE_THROUGH_XN},

  /* ---- boot services scratch ---- */
  {"CPU Vectors",      0x91400000, 0x00001000, AddMem, SYS_MEM, SYS_MEM_CAP, BsData, WRITE_BACK},
  {"MMU PageTables",   0x91401000, 0x00003000, AddMem, SYS_MEM, SYS_MEM_CAP, BsData, WRITE_BACK_XN},
  {"UEFI Stack",       0x91404000, 0x00040000, AddMem, SYS_MEM, SYS_MEM_CAP, BsData, WRITE_BACK_XN},
  {"SimpleInit DT",    0x9F200000, 0x00100000, AddMem, SYS_MEM, SYS_MEM_CAP, BsData, WRITE_BACK_XN},

  {"RAM Partition",    0x91444000, 0x0DBBC000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv,   WRITE_BACK_XN},
  {"RAM Partition",    0x9F300000, 0x00C00000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv,   WRITE_BACK_XN},

  /* ---- pstore ---- */
  {"PSTORE",           0x9FF00000, 0x00100000, AddMem, SYS_MEM, SYS_MEM_CAP, Reserv, WRITE_BACK_XN},

  /* ---- DXE heap ---- */
  {"DXE Heap",         0xA0000000, 0x60000000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv,   WRITE_BACK_XN},

  /* ---- MMIO ---- */
  {"IMEM Base",        0x08600000, 0x00010000, NoHob,  MMAP_IO, INITIALIZED, Conv,   NS_DEVICE},
  {"SMEM",             0x08630000, 0x00100000, NoHob,  MMAP_IO, UNCACHEABLE, Reserv, UNCACHED_UNBUFFERED_XN},
  {"TLMM",             0x01000000, 0x00300000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"GCC",              0x01800000, 0x00080000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"MDSS",             0x01A00000, 0x000A0000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"GIC",              0x0B000000, 0x00400000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"WDT",              0x0B017000, 0x00001000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"PS_HOLD",          0x004AB000, 0x00001000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"SPMI",             0x0200F000, 0x00001000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"BLSP1 UART",       0x078AF000, 0x00001000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"SDCC1",            0x07824000, 0x00002000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"SDCC2",            0x07864000, 0x00002000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"USB3",             0x07000000, 0x00100000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},

  {"Terminator",       0,          0,          0,      0,       0,           0,      0}
};

ARM_MEMORY_REGION_DESCRIPTOR_EX *GetPlatformMemoryMap()
{
  return gDeviceMemoryDescriptorEx;
}
