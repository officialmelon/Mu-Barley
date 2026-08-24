DefinitionBlock ("", "DSDT", 2, "LENOVO", "BARLEY", 1)
{
    Scope (\_SB)
    {
        Device (CPU0)
        {
            Name (_HID, "ACPI0007")
            Name (_UID, 0)
            Method (_STA, 0, NotSerialized) { Return (0x0F) }
        }

        Device (CPU1)
        {
            Name (_HID, "ACPI0007")
            Name (_UID, 1)
            Method (_STA, 0, NotSerialized) { Return (0x0F) }
        }

        Device (CPU2)
        {
            Name (_HID, "ACPI0007")
            Name (_UID, 2)
            Method (_STA, 0, NotSerialized) { Return (0x0F) }
        }

        Device (CPU3)
        {
            Name (_HID, "ACPI0007")
            Name (_UID, 3)
            Method (_STA, 0, NotSerialized) { Return (0x0F) }
        }

        Device (CPU4)
        {
            Name (_HID, "ACPI0007")
            Name (_UID, 4)
            Method (_STA, 0, NotSerialized) { Return (0x0F) }
        }

        Device (CPU5)
        {
            Name (_HID, "ACPI0007")
            Name (_UID, 5)
            Method (_STA, 0, NotSerialized) { Return (0x0F) }
        }

        Device (CPU6)
        {
            Name (_HID, "ACPI0007")
            Name (_UID, 6)
            Method (_STA, 0, NotSerialized) { Return (0x0F) }
        }

        Device (CPU7)
        {
            Name (_HID, "ACPI0007")
            Name (_UID, 7)
            Method (_STA, 0, NotSerialized) { Return (0x0F) }
        }

        // Barley's physical keys are split between the MT6768 keypad block
        // and the MT6358 PMIC reached through PWRAP. BAR0001 is consumed by
        // the Barley Windows HID miniport; other operating systems may ignore
        // it. These are real hardware resources, not a fabricated GPIO button
        // array.
        Device (BKPD)
        {
            Name (_HID, "BAR0001")
            Name (_UID, 0)
            Name (_DDN, "Barley physical keys")
            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite, 0x10010000, 0x00001000)
                Memory32Fixed (ReadWrite, 0x1000D000, 0x00001000)
            })

            Method (_STA, 0, NotSerialized) { Return (0x0F) }
        }
    }
}
