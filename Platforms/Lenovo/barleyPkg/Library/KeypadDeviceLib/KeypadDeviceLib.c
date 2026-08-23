/** @file
  Lenovo Barley PMIC-key implementation for the common Silicium keypad stack.

  The MT6358 secondary key is the only verified navigation key on Barley.  The
  tablet's other volume key is provided by the SoC keypad controller and is
  intentionally left unsupported until that controller is brought up.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/KeypadDeviceLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Protocol/MtkPmic.h>

#define MILLISECONDS_TO_NANOSECONDS(Milliseconds) \
  (((UINT64)(Milliseconds)) * 1000000ULL)

STATIC MTK_PMIC_PROTOCOL  *mPmicProtocol;
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

  return RETURN_SUCCESS;
}

EFI_STATUS
EFIAPI
KeypadDeviceReset (
  IN KEYPAD_DEVICE_PROTOCOL  *This
  )
{
  ResetKeyContext (&mNavigationKey);
  mNavigationKey.KeyData.Key.ScanCode = SCAN_UP;

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

  IsPressed = FALSE;
  mPmicProtocol->HomeButtonPressed (&IsPressed);
  UpdateKey (&mNavigationKey, KeypadReturnApi, IsPressed, Delta, FALSE);

  IsPressed = FALSE;
  mPmicProtocol->PowerButtonPressed (&IsPressed);
  UpdateKey (&mPowerKey, KeypadReturnApi, IsPressed, Delta, TRUE);

  return EFI_SUCCESS;
}
