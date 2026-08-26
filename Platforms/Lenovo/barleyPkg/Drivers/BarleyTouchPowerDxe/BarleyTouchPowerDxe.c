/**
  Keep the touchscreen supply enabled across the UEFI-to-OS handoff.

  Linux describes the HX83102J supply as vtouch-supply = ldo_vldo28 and marks
  that regulator default-on.  The EDK2 PMIC driver intentionally exposes the
  regulator API but does not interpret Linux device-tree consumer properties,
  so this small platform consumer performs the equivalent enable operation.
  It leaves the voltage selected by LK/PMIC unchanged.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>

#include <Library/DebugLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Protocol/MtkPmic.h>

#define BARLEY_TOUCH_VTOUCH_REGULATOR  "ldo_vldo28"

EFI_STATUS
EFIAPI
BarleyTouchPowerEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS         Status;
  MTK_PMIC_PROTOCOL  *Pmic;
  BOOLEAN            Enabled;

  Pmic = NULL;
  Status = gBS->LocateProtocol (
                  &gMediaTekPmicProtocolGuid,
                  NULL,
                  (VOID **)&Pmic
                  );
  if (EFI_ERROR (Status) || (Pmic == NULL)) {
    DEBUG ((
      EFI_D_ERROR,
      "BarleyTouchPower: PMIC protocol unavailable: %r\n",
      Status
      ));
    return EFI_SUCCESS;
  }

  Enabled = FALSE;
  Status = Pmic->RegulatorIsEnabled (
                       BARLEY_TOUCH_VTOUCH_REGULATOR,
                       &Enabled
                       );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      EFI_D_ERROR,
      "BarleyTouchPower: unable to read %a state: %r\n",
      BARLEY_TOUCH_VTOUCH_REGULATOR,
      Status
      ));
    return EFI_SUCCESS;
  }

  if (!Enabled) {
    Status = Pmic->RegulatorSetEnable (
                         BARLEY_TOUCH_VTOUCH_REGULATOR,
                         TRUE
                         );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        EFI_D_ERROR,
        "BarleyTouchPower: unable to enable %a: %r\n",
        BARLEY_TOUCH_VTOUCH_REGULATOR,
        Status
        ));
      return EFI_SUCCESS;
    }

    // Allow the rail to settle before a subsequent reset/SPI transaction.
    MicroSecondDelay (1000);
    DEBUG ((
      EFI_D_INFO,
      "BarleyTouchPower: enabled %a\n",
      BARLEY_TOUCH_VTOUCH_REGULATOR
      ));
  } else {
    DEBUG ((
      EFI_D_INFO,
      "BarleyTouchPower: %a already enabled\n",
      BARLEY_TOUCH_VTOUCH_REGULATOR
      ));
  }

  return EFI_SUCCESS;
}
