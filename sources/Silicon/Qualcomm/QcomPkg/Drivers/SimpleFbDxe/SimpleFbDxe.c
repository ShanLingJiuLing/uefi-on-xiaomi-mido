/* SimpleFbDxe: Simple FrameBuffer
 *
 * [mido] Modified: the original driver hardcodes a 32bpp scanout. On the
 * Redmi Note 4X the panel is driven by lk2nd as 24bpp RGB888 (stride
 * 1080*3 = 3240), which is also what the mainline simple-framebuffer node
 * describes. Feeding a 32bpp GOP into a 24bpp buffer shears every scanline
 * by 1080 bytes, so the console shows overlapping/garbled text.
 *
 * When PcdMipiFrameBufferPixelBpp != 32 we now:
 *   - hand a 32bpp shadow buffer to FrameBufferBltLib / GOP, and
 *   - convert the touched rectangle to packed 24bpp (B,G,R byte order, which
 *     is what FrameBufferSerialPortLib produces) on every Blt.
 *
 * Platforms that really are 32bpp are unaffected (same code path as before).
 */
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/DebugLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/FrameBufferBltLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <PiDxe.h>
#include <Protocol/GraphicsOutput.h>
#include <Uefi.h>

/// Defines
#define VNBYTES(bpix) (1 << (bpix)) / 8
#define VNBITS(bpix) (1 << (bpix))

#define FB_BITS_PER_PIXEL (32)
#define FB_BYTES_PER_PIXEL (FB_BITS_PER_PIXEL / 8)

enum video_log2_bpp {
  VIDEO_BPP1 = 0,
  VIDEO_BPP2,
  VIDEO_BPP4,
  VIDEO_BPP8,
  VIDEO_BPP16,
  VIDEO_BPP32,
};

typedef struct {
  VENDOR_DEVICE_PATH DisplayDevicePath;
  EFI_DEVICE_PATH    EndDevicePath;
} DISPLAY_DEVICE_PATH;

DISPLAY_DEVICE_PATH mDisplayDevicePath = {
    {{HARDWARE_DEVICE_PATH,
      HW_VENDOR_DP,
      {
          (UINT8)(sizeof(VENDOR_DEVICE_PATH)),
          (UINT8)((sizeof(VENDOR_DEVICE_PATH)) >> 8),
      }},
     EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID},
    {END_DEVICE_PATH_TYPE,
     END_ENTIRE_DEVICE_PATH_SUBTYPE,
     {sizeof(EFI_DEVICE_PATH_PROTOCOL), 0}}};

/// Declares

STATIC FRAME_BUFFER_CONFIGURE *mFrameBufferBltLibConfigure;
STATIC UINTN                   mFrameBufferBltLibConfigureSize;

//
// [mido] shadow-buffer / conversion state
//
STATIC UINT32  mPanelFrameBufferBase = 0;
STATIC UINT32  mPanelWidth           = 0;
STATIC UINT32  mPanelHeight          = 0;
STATIC BOOLEAN mNeedsConversion      = FALSE;

STATIC
EFI_STATUS
EFIAPI
DisplayQueryMode(
    IN EFI_GRAPHICS_OUTPUT_PROTOCOL *This, IN UINT32 ModeNumber,
    OUT UINTN *SizeOfInfo, OUT EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **Info);

STATIC
EFI_STATUS
EFIAPI
DisplaySetMode(IN EFI_GRAPHICS_OUTPUT_PROTOCOL *This, IN UINT32 ModeNumber);

STATIC
EFI_STATUS
EFIAPI
DisplayBlt(
    IN EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL *            BltBuffer,
    OPTIONAL IN EFI_GRAPHICS_OUTPUT_BLT_OPERATION BltOperation,
    IN UINTN SourceX, IN UINTN SourceY, IN UINTN DestinationX,
    IN UINTN DestinationY, IN UINTN Width, IN UINTN Height,
    IN UINTN Delta OPTIONAL);

STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL mDisplay = {
    DisplayQueryMode, DisplaySetMode, DisplayBlt, NULL};

STATIC
EFI_STATUS
EFIAPI
DisplayQueryMode(
    IN EFI_GRAPHICS_OUTPUT_PROTOCOL *This, IN UINT32 ModeNumber,
    OUT UINTN *SizeOfInfo, OUT EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **Info)
{
  EFI_STATUS Status;
  Status = gBS->AllocatePool(
      EfiBootServicesData, sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION),
      (VOID **)Info);

  ASSERT_EFI_ERROR(Status);

  *SizeOfInfo                   = sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);
  (*Info)->Version              = This->Mode->Info->Version;
  (*Info)->HorizontalResolution = This->Mode->Info->HorizontalResolution;
  (*Info)->VerticalResolution   = This->Mode->Info->VerticalResolution;
  (*Info)->PixelFormat          = This->Mode->Info->PixelFormat;
  (*Info)->PixelsPerScanLine    = This->Mode->Info->PixelsPerScanLine;

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
DisplaySetMode(IN EFI_GRAPHICS_OUTPUT_PROTOCOL *This, IN UINT32 ModeNumber)
{
  return EFI_SUCCESS;
}

/**
  [mido] Convert a rectangle from the 32bpp shadow buffer to the packed
  24bpp (B,G,R) panel buffer.
**/
STATIC
VOID
ConvertRectTo24bpp(
    IN UINTN X, IN UINTN Y, IN UINTN Width, IN UINTN Height)
{
  UINT8  *DstBase;
  UINT32 *SrcBase;
  UINTN   Row, Col;

  if (!mNeedsConversion || (mPanelFrameBufferBase == 0)) {
    return;
  }

  if (X >= mPanelWidth || Y >= mPanelHeight) {
    return;
  }
  if ((X + Width) > mPanelWidth) {
    Width = mPanelWidth - X;
  }
  if ((Y + Height) > mPanelHeight) {
    Height = mPanelHeight - Y;
  }

  DstBase = (UINT8 *)(UINTN)mPanelFrameBufferBase;
  SrcBase = (UINT32 *)(UINTN)mDisplay.Mode->FrameBufferBase; // shadow

  for (Row = 0; Row < Height; Row++) {
    UINT8  *Dst = DstBase + ((Y + Row) * mPanelWidth + X) * 3;
    UINT32 *Src = SrcBase + ((Y + Row) * mPanelWidth) + X;

    for (Col = 0; Col < Width; Col++) {
      UINT32 Pixel = *Src++;
      *Dst++ = (UINT8)(Pixel & 0xFF);         // B
      *Dst++ = (UINT8)((Pixel >> 8) & 0xFF);  // G
      *Dst++ = (UINT8)((Pixel >> 16) & 0xFF); // R
    }
  }

  WriteBackInvalidateDataCacheRange(
      DstBase + (Y * mPanelWidth + X) * 3, (Height - 1) * mPanelWidth * 3 + Width * 3);
}

STATIC
EFI_STATUS
EFIAPI
DisplayBlt(
    IN EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL *            BltBuffer,
    OPTIONAL IN EFI_GRAPHICS_OUTPUT_BLT_OPERATION BltOperation,
    IN UINTN SourceX, IN UINTN SourceY, IN UINTN DestinationX,
    IN UINTN DestinationY, IN UINTN Width, IN UINTN Height,
    IN UINTN Delta OPTIONAL)
{

  RETURN_STATUS Status;
  EFI_TPL       Tpl;
  //
  // We have to raise to TPL_NOTIFY, so we make an atomic write to the frame
  // buffer. We would not want a timer based event (Cursor, ...) to come in
  // while we are doing this operation.
  //
  Tpl    = gBS->RaiseTPL(TPL_NOTIFY);
  Status = FrameBufferBlt(
      mFrameBufferBltLibConfigure, BltBuffer, BltOperation, SourceX, SourceY,
      DestinationX, DestinationY, Width, Height, Delta);
  gBS->RestoreTPL(Tpl);

  // [mido] push the touched rectangle out to the real 24bpp panel buffer
  ConvertRectTo24bpp(DestinationX, DestinationY, Width, Height);

  // zhuowei: hack: flush the cache manually since my memory maps are still
  // broken
  WriteBackInvalidateDataCacheRange(
      (void *)mDisplay.Mode->FrameBufferBase, mDisplay.Mode->FrameBufferSize);
  // zhuowei: end hack

  return RETURN_ERROR(Status) ? EFI_INVALID_PARAMETER : EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
SimpleFbDxeInitialize(
    IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{

  EFI_STATUS Status             = EFI_SUCCESS;
  EFI_HANDLE hUEFIDisplayHandle = NULL;

  /* Retrieve simple frame buffer from pre-SEC bootloader */
  DEBUG(
      (EFI_D_ERROR,
       "SimpleFbDxe: Retrieve MIPI FrameBuffer parameters from PCD\n"));
  UINT32 MipiFrameBufferAddr   = FixedPcdGet32(PcdMipiFrameBufferAddress);
  UINT32 MipiFrameBufferWidth  = FixedPcdGet32(PcdMipiFrameBufferWidth);
  UINT32 MipiFrameBufferHeight = FixedPcdGet32(PcdMipiFrameBufferHeight);
  UINT32 MipiFrameBufferBpp    = FixedPcdGet32(PcdMipiFrameBufferPixelBpp);

  /* Sanity check */
  if (MipiFrameBufferAddr == 0 || MipiFrameBufferWidth == 0 ||
      MipiFrameBufferHeight == 0) {
    DEBUG((EFI_D_ERROR, "SimpleFbDxe: Invalid FrameBuffer parameters\n"));
    return EFI_DEVICE_ERROR;
  }

  /* Prepare struct */
  if (mDisplay.Mode == NULL) {
    Status = gBS->AllocatePool(
        EfiBootServicesData, sizeof(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE),
        (VOID **)&mDisplay.Mode);

    ASSERT_EFI_ERROR(Status);
    if (EFI_ERROR(Status))
      return Status;

    ZeroMem(mDisplay.Mode, sizeof(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE));
  }

  if (mDisplay.Mode->Info == NULL) {
    Status = gBS->AllocatePool(
        EfiBootServicesData, sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION),
        (VOID **)&mDisplay.Mode->Info);

    ASSERT_EFI_ERROR(Status);
    if (EFI_ERROR(Status))
      return Status;

    ZeroMem(mDisplay.Mode->Info, sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION));
  }

  /* Set information */
  mDisplay.Mode->MaxMode       = 1;
  mDisplay.Mode->Mode          = 0;
  mDisplay.Mode->Info->Version = 0;

  mDisplay.Mode->Info->HorizontalResolution = MipiFrameBufferWidth;
  mDisplay.Mode->Info->VerticalResolution   = MipiFrameBufferHeight;

  /* SimpleFB always presents a8r8g8b8 to the GOP */
  UINT32               LineLength = MipiFrameBufferWidth * VNBYTES(VIDEO_BPP32);
  UINT32               FrameBufferSize    = LineLength * MipiFrameBufferHeight;
  EFI_PHYSICAL_ADDRESS FrameBufferAddress = MipiFrameBufferAddr;

  mDisplay.Mode->Info->PixelsPerScanLine = MipiFrameBufferWidth;
  mDisplay.Mode->Info->PixelFormat = PixelBlueGreenRedReserved8BitPerColor;
  mDisplay.Mode->SizeOfInfo      = sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);

  mPanelFrameBufferBase = MipiFrameBufferAddr;
  mPanelWidth           = MipiFrameBufferWidth;
  mPanelHeight          = MipiFrameBufferHeight;

  if (MipiFrameBufferBpp == 32) {
    mNeedsConversion = FALSE;
  } else {
    /*
     * [mido] Panel is packed 24bpp. Give the GOP a 32bpp shadow buffer and
     * convert on every blit instead of shearing the scanout.
     */
    DEBUG(
        (EFI_D_ERROR,
         "SimpleFbDxe: panel is %u bpp, using 32bpp shadow buffer\n",
         MipiFrameBufferBpp));

    VOID *Shadow = AllocatePool(FrameBufferSize);
    if (Shadow == NULL) {
      DEBUG((EFI_D_ERROR, "SimpleFbDxe: cannot allocate shadow buffer\n"));
      return EFI_OUT_OF_RESOURCES;
    }
    ZeroMem(Shadow, FrameBufferSize);
    FrameBufferAddress = (EFI_PHYSICAL_ADDRESS)(UINTN)Shadow;
    mNeedsConversion   = TRUE;

    /* blank the real panel buffer too (24bpp) */
    ZeroMem(
        (VOID *)(UINTN)mPanelFrameBufferBase,
        (UINTN)mPanelWidth * mPanelHeight * (MipiFrameBufferBpp / 8));
    WriteBackInvalidateDataCacheRange(
        (VOID *)(UINTN)mPanelFrameBufferBase,
        (UINTN)mPanelWidth * mPanelHeight * (MipiFrameBufferBpp / 8));
  }

  mDisplay.Mode->FrameBufferBase = FrameBufferAddress;
  mDisplay.Mode->FrameBufferSize = FrameBufferSize;

  //
  // Create the FrameBufferBltLib configuration.
  //
  Status = FrameBufferBltConfigure(
      (VOID *)(UINTN)mDisplay.Mode->FrameBufferBase, mDisplay.Mode->Info,
      mFrameBufferBltLibConfigure, &mFrameBufferBltLibConfigureSize);
  if (Status == RETURN_BUFFER_TOO_SMALL) {
    mFrameBufferBltLibConfigure = AllocatePool(mFrameBufferBltLibConfigureSize);
    if (mFrameBufferBltLibConfigure != NULL) {
      Status = FrameBufferBltConfigure(
          (VOID *)(UINTN)mDisplay.Mode->FrameBufferBase, mDisplay.Mode->Info,
          mFrameBufferBltLibConfigure, &mFrameBufferBltLibConfigureSize);
    }
  }
  ASSERT_EFI_ERROR(Status);

  // zhuowei: clear the screen to black
  ZeroMem((void *)(UINTN)FrameBufferAddress, FrameBufferSize);
  WriteBackInvalidateDataCacheRange(
      (void *)(UINTN)FrameBufferAddress, FrameBufferSize);
  // zhuowei: end

  /* Register handle */
  Status = gBS->InstallMultipleProtocolInterfaces(
      &hUEFIDisplayHandle, &gEfiDevicePathProtocolGuid, &mDisplayDevicePath,
      &gEfiGraphicsOutputProtocolGuid, &mDisplay, NULL);

  ASSERT_EFI_ERROR(Status);

  return Status;
}
