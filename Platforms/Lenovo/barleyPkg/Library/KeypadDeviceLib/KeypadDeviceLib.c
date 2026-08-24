/** @file
  Lenovo Barley physical-key implementation for the common Silicium keypad stack.

  MT6358 Home is the down-navigation key.  The SoC keypad controller supplies
  the up-navigation key through its already-configured, active-low MEM1 bit 0.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/KeypadDeviceLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Protocol/MtkPmic.h>

#define MILLISECONDS_TO_NANOSECONDS(Milliseconds) \
  (((UINT64)(Milliseconds)) * 1000000ULL)

#define BARLEY_KPD_BASE      0x10010000U
#define BARLEY_KPD_STA       (BARLEY_KPD_BASE + 0x0000U)
#define BARLEY_KPD_MEM1      (BARLEY_KPD_BASE + 0x0004U)
#define BARLEY_KPD_DEBOUNCE  (BARLEY_KPD_BASE + 0x0018U)
#define BARLEY_KPD_EN        (BARLEY_KPD_BASE + 0x0024U)

STATIC MTK_PMIC_PROTOCOL  *mPmicProtocol;
STATIC KEY_CONTEXT         mKpdKey;
STATIC KEY_CONTEXT         mNavigationKey;
STATIC KEY_CONTEXT         mPowerKey;

STATIC EFI_KEY_DATA  mEscapeKey = {
  .Key = {
    .ScanCode = SCAN_ESC
  }
};

STATIC
VOID
ResetKeyContext (
  OUT KEY_CONTEXT  *Context
  )
{
  ZeroMem (Context, sizeof (*Context));
  Context->State = KEYSTATE_RELEASED;
}

STATIC
VOID
UpdateKey (
  IN OUT KEY_CONTEXT       *Context,
  IN     KEYPAD_RETURN_API *KeypadReturnApi,
  IN     BOOLEAN            IsPressed,
  IN     UINT64             Delta,
  IN     BOOLEAN            LongPressIsEscape
  )
{
  switch (Context->State) {
    case KEYSTATE_RELEASED:
      if (IsPressed) {
        Context->Time  = 0;
        Context->State = KEYSTATE_PRESSED;
      }

      break;

    case KEYSTATE_PRESSED:
      if (IsPressed) {
        Context->Time += Delta;
        if (LongPressIsEscape &&
            !Context->Longpress &&
            (Context->Time >= MILLISECONDS_TO_NANOSECONDS (500)))
        {
          KeypadReturnApi->PushEfikeyBufTail (KeypadReturnApi, &mEscapeKey);
          Context->Longpress = TRUE;
        }
      } else {
        if (!Context->Longpress) {
          KeypadReturnApi->PushEfikeyBufTail (KeypadReturnApi, &Context->KeyData);
        }

        Context->Time      = 0;
        Context->Repeat    = FALSE;
        Context->Longpress = FALSE;
        Context->State     = KEYSTATE_RELEASED;
      }

      break;

    case KEYSTATE_LONGPRESS_RELEASE:
      Context->Time      = 0;
      Context->Repeat    = FALSE;
      Context->Longpress = FALSE;
      Context->State     = KEYSTATE_RELEASED;
      break;

    default:
      ASSERT (FALSE);
      break;
  }
}

RETURN_STATUS
EFIAPI
KeypadDeviceConstructor (
  VOID
  )
{
  EFI_STATUS  Status;

  Status = gBS->LocateProtocol (
                  &gMediaTekPmicProtocolGuid,
                  NULL,
                  (VOID **)&mPmicProtocol
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Barley keypad: PMIC protocol unavailable: %r\n", Status));
    return RETURN_NOT_FOUND;
  }

  DEBUG ((
    DEBUG_INFO,
    "Barley KPD: STA=0x%04x MEM1=0x%04x DEBOUNCE=0x%04x EN=0x%04x\n",
    MmioRead16 (BARLEY_KPD_STA),
    MmioRead16 (BARLEY_KPD_MEM1),
    MmioRead16 (BARLEY_KPD_DEBOUNCE),
    MmioRead16 (BARLEY_KPD_EN)
    ));

  return RETURN_SUCCESS;
}

EFI_STATUS
EFIAPI
KeypadDeviceReset (
  IN KEYPAD_DEVICE_PROTOCOL  *This
  )
{
  ResetKeyContext (&mKpdKey);
  mKpdKey.KeyData.Key.ScanCode = SCAN_UP;

  ResetKeyContext (&mNavigationKey);
  mNavigationKey.KeyData.Key.ScanCode = SCAN_DOWN;

  ResetKeyContext (&mPowerKey);
  mPowerKey.KeyData.Key.UnicodeChar = CHAR_CARRIAGE_RETURN;

  return EFI_SUCCESS;
}
EFI_STATUS
KeypadDeviceGetKeys (
  IN KEYPAD_DEVICE_PROTOCOL  *This,
  IN KEYPAD_RETURN_API       *KeypadReturnApi,
  IN UINT64                   Delta
  )
{
  BOOLEAN  IsPressed;

  if ((mPmicProtocol == NULL) || (KeypadReturnApi == NULL)) {
    return EFI_NOT_READY;
  }

  IsPressed = (MmioRead16 (BARLEY_KPD_MEM1) & BIT0) == 0;
  UpdateKey (&mKpdKey, KeypadReturnApi, IsPressed, Delta, FALSE);

  IsPressed = FALSE;
  mPmicProtocol->HomeButtonPressed (&IsPressed);
  UpdateKey (&mNavigationKey, KeypadReturnApi, IsPressed, Delta, FALSE);

  IsPressed = FALSE;
  mPmicProtocol->PowerButtonPressed (&IsPressed);
  UpdateKey (&mPowerKey, KeypadReturnApi, IsPressed, Delta, TRUE);

  return EFI_SUCCESS;
}
