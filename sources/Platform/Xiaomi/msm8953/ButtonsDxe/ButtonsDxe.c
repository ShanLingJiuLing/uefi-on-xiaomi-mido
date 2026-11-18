/** @file
  Minimal GPIO button driver for Xiaomi Redmi Note 4X (mido), MSM8953.

  Design note: this build does NOT use a timer event. Timer-based sampling
  proved unreliable on this firmware (the 10 ms EVT_TIMER callback apparently
  never ran, so no key was ever produced). Instead we sample the GPIO inside
  ReadKeyStroke() and inside the WaitForKey notify function, which the
  consumer (BDS / the boot menu) calls continuously anyway.

  Hardware facts (measured):
    - /soc/gpio_keys/vol_up: gpios = <&tlmm 0x55 0x1>, linux,code = 0x73
      -> Volume Up on TLMM GPIO 85, active low, pull-up.
    - Volume Down / Power are behind PMIC PON (SPMI) - not supported here.
    - The pinctrl group "tlmm_gpio_key" covers GPIO 85/86/87, so we watch all
      three and let any of them act as the button.

  TLMM layout (platform/msm8953/include/platform/iomap.h in lk2nd):
    GPIO_CONFIG_ADDR(n)  = 0x1000000 + n * 0x1000
    GPIO_IN_OUT_ADDR(n)  = 0x1000000 + 0x4 + n * 0x1000   bit0 = input level

  Gesture: short press -> Down arrow, long press (>=800 ms) -> Enter.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Protocol/SimpleTextIn.h>

#define TLMM_BASE_ADDR    0x01000000U

#define GPIO_CFG_ADDR(n)      (TLMM_BASE_ADDR + ((n) * 0x1000))
#define GPIO_IN_OUT_ADDR(n)   (TLMM_BASE_ADDR + 0x4 + ((n) * 0x1000))
#define GPIO_IN_BIT           0x1U
#define GPIO_OE_BIT           0x200U

#define GPIO_CFG_FUNC_GPIO    0x0U
#define GPIO_CFG_PULL_UP      0x3U

#define LONG_PRESS_US         800000U   // 800 ms
#define KEY_QUEUE_SIZE        16U

STATIC CONST UINTN mKeyPins[] = { 85, 86, 87 };
#define KEY_PIN_COUNT (sizeof (mKeyPins) / sizeof (mKeyPins[0]))

STATIC EFI_SIMPLE_TEXT_INPUT_PROTOCOL  mSimpleTextIn;

STATIC BOOLEAN  mKeyDown[KEY_PIN_COUNT];
STATIC UINTN    mPressStart[KEY_PIN_COUNT];

STATIC EFI_INPUT_KEY  mQueue[KEY_QUEUE_SIZE];
STATIC UINTN          mQueueHead;
STATIC UINTN          mQueueTail;

STATIC
VOID
QueuePush (
  IN CONST EFI_INPUT_KEY *Key
  )
{
  UINTN Next = (mQueueTail + 1) % KEY_QUEUE_SIZE;

  if (Next == mQueueHead) {
    return;
  }

  CopyMem (&mQueue[mQueueTail], (VOID *)Key, sizeof (EFI_INPUT_KEY));
  mQueueTail = Next;
}

STATIC
BOOLEAN
QueuePop (
  OUT EFI_INPUT_KEY *Key
  )
{
  if (mQueueHead == mQueueTail) {
    return FALSE;
  }

  CopyMem (Key, &mQueue[mQueueHead], sizeof (EFI_INPUT_KEY));
  mQueueHead = (mQueueHead + 1) % KEY_QUEUE_SIZE;
  return TRUE;
}

STATIC
BOOLEAN
PinPressed (
  IN UINTN Pin
  )
{
  //
  // active low with pull-up: 0 = pressed
  //
  return (BOOLEAN)((MmioRead32 (GPIO_IN_OUT_ADDR (Pin)) & GPIO_IN_BIT) == 0);
}

STATIC
VOID
SampleKeys (
  VOID
  )
{
  UINTN   Index;
  UINT64  Freq;
  UINT64  Now;

  Freq = GetPerformanceCounterProperties (NULL, NULL);
  Now  = GetPerformanceCounter ();

  for (Index = 0; Index < KEY_PIN_COUNT; Index++) {
    BOOLEAN Down = PinPressed (mKeyPins[Index]);

    if (Down && !mKeyDown[Index]) {
      mKeyDown[Index]    = TRUE;
      mPressStart[Index] = Now;
    } else if (!Down && mKeyDown[Index]) {
      EFI_INPUT_KEY Key;
      UINT64        Delta;
      UINT64        Us;

      mKeyDown[Index] = FALSE;

      Delta = Now - mPressStart[Index];
      Us    = (Freq != 0) ? DivU64x64Remainder (Delta * 1000000ULL, Freq, NULL) : 0;

      if (Us >= LONG_PRESS_US) {
        Key.ScanCode    = SCAN_NULL;
        Key.UnicodeChar = CHAR_CARRIAGE_RETURN;
        DEBUG ((DEBUG_ERROR, "[mido] key%u: long (%lu us) -> ENTER\n", mKeyPins[Index], Us));
      } else {
        Key.ScanCode    = SCAN_DOWN;
        Key.UnicodeChar = 0;
        DEBUG ((DEBUG_ERROR, "[mido] key%u: short (%lu us) -> DOWN\n", mKeyPins[Index], Us));
      }

      QueuePush (&Key);
    }
  }
}

STATIC
VOID
EFIAPI
WaitForKeyNotify (
  IN EFI_EVENT Event,
  IN VOID      *Context
  )
{
  SampleKeys ();

  if (mQueueHead != mQueueTail) {
    gBS->SignalEvent (Event);
  }
}

STATIC
EFI_STATUS
EFIAPI
ButtonsReset (
  IN EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This,
  IN BOOLEAN                         ExtendedVerification
  )
{
  mQueueHead = 0;
  mQueueTail = 0;
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
ButtonsReadKeyStroke (
  IN  EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This,
  OUT EFI_INPUT_KEY                  *Key
  )
{
  if (Key == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  SampleKeys ();

  if (QueuePop (Key)) {
    return EFI_SUCCESS;
  }

  return EFI_NOT_READY;
}

STATIC
VOID
ConfigureKeyGpio (
  IN UINTN Pin
  )
{
  UINT32 Cfg;

  Cfg  = MmioRead32 (GPIO_CFG_ADDR (Pin));
  Cfg &= ~((0x3U << 2) | (0x3U << 0) | GPIO_OE_BIT);
  Cfg |= (GPIO_CFG_FUNC_GPIO << 2) | (GPIO_CFG_PULL_UP << 0);
  MmioWrite32 (GPIO_CFG_ADDR (Pin), Cfg);

  DEBUG (
    (DEBUG_ERROR, "[mido] ButtonsDxe: GPIO%u cfg=0x%08x level=%u\n",
     Pin, MmioRead32 (GPIO_CFG_ADDR (Pin)),
     MmioRead32 (GPIO_IN_OUT_ADDR (Pin)) & GPIO_IN_BIT)
    );
}

EFI_STATUS
EFIAPI
ButtonsDxeEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  EFI_HANDLE  Handle = NULL;
  UINTN       Index;

  DEBUG ((DEBUG_ERROR, "[mido] ButtonsDxe: start (poll-on-read)\n"));

  for (Index = 0; Index < KEY_PIN_COUNT; Index++) {
    ConfigureKeyGpio (mKeyPins[Index]);
  }

  mSimpleTextIn.Reset         = ButtonsReset;
  mSimpleTextIn.ReadKeyStroke = ButtonsReadKeyStroke;

  Status = gBS->CreateEvent (
                  EVT_NOTIFY_WAIT,
                  TPL_NOTIFY,
                  WaitForKeyNotify,
                  NULL,
                  &mSimpleTextIn.WaitForKey
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "[mido] ButtonsDxe: CreateEvent: %r\n", Status));
    return Status;
  }

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Handle,
                  &gEfiSimpleTextInProtocolGuid,
                  &mSimpleTextIn,
                  NULL
                  );
  DEBUG ((DEBUG_ERROR, "[mido] ButtonsDxe: installed (%r)\n", Status));

  return Status;
}
