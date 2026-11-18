#ifndef _PLATFORM_UTILS_H_
#define _PLATFORM_UTILS_H_

#include <Library/PcdLib.h>

/*
 * APSS watchdog (apps_wdog) on MSM8953.
 *
 * Was 0x17817000 - that is the MSM8998 address and is unmapped on mido,
 * so the very first MmioWrite32() caused an external abort at EL2 (no
 * exception vectors installed yet) and the device died right after the
 * firmware banner. Verified against /proc/iomem ("0b017000-0b017fff :
 * msm-watchdog") and the live DTB ("qcom,wdt@b017000").
 */
#define APSS_WDT_BASE 0x0B017000
#define APSS_WDT_ENABLE_OFFSET 0x8

VOID PlatformInitialize();

#endif /* _PLATFORM_UTILS_H_ */
