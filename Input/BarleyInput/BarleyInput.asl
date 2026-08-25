// Firmware-side contract. The canonical copy currently lives in barleyPkg's
// DSDT; this file documents the ABI consumed by BarleyInput.sys.
Device (BKPD)
{
    Name (_HID, "BAR0001")
    Name (_UID, Zero)
    Name (_DDN, "Barley physical keys")
    Name (_CRS, ResourceTemplate ()
    {
        // Resource order is part of BAR0001 v1: KPD first, PWRAP second.
        Memory32Fixed (ReadWrite, 0x10010000, 0x00001000)
        Memory32Fixed (ReadWrite, 0x1000D000, 0x00001000)
    })
    Method (_STA, 0, NotSerialized) { Return (0x0F) }
}

