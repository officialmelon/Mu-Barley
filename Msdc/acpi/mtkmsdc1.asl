// Diagnostic exposure of Barley's removable MT6768 MSDC1 controller.
//
// The vendor ID is intentional.  MTK MSDC is not register-compatible with
// SDHCI/SDA and therefore does not carry _CID("PNP0D40").

DefinitionBlock ("", "SSDT", 2, "LENOVO", "BLYMSD1", 0x00000001)
{
    Scope (\_SB)
    {
        Device (MSD1)
        {
            Name (_HID, "MTK6768")
            Name (_UID, One)
            Name (_CCA, One)
            Name (_STA, 0x0F)

            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite,
                    0x11240000,       // MSDC1 main register base
                    0x00010000)       // main register window length

                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive)
                {
                    133               // GIC SPI 101 + architectural 32
                }
            })
        }
    }
}
