#!/bin/bash
# msm8953 (mido) boot image packing.
#
# The generic hook uses mkbootimg.py defaults (base 0x10000000), but mido's
# DRAM starts at 0x40000000, so lk2nd's check_ddr_addr_range_bound() would
# reject the image. Use the same load addresses as the known-good stock
# Android boot image on this device:
#
#   kernel  -> 0x80008000
#   ramdisk -> 0x81000000
#   tags    -> 0x80000100

function platform_build_bootimg(){
	python3 "${ROOTDIR}/tools/mkbootimg.py" \
		--kernel "${WORKSPACE}/uefi-${DEVICE}-kernel" \
		--ramdisk ramdisk \
		--base 0x80000000 \
		--kernel_offset 0x00008000 \
		--ramdisk_offset 0x01000000 \
		--tags_offset 0x00000100 \
		--os_version "${BOOTIMG_OS_VERSION}" \
		--os_patch_level "${BOOTIMG_OS_PATCH_LEVEL}" \
		--header_version "${BOOTIMG_HEADER_VERSION}" \
		-o "${OUTDIR}/boot-${DEVICE}${EXT}.img" \
		||return "$?"
}
