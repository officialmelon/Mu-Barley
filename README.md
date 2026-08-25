# Barley Windows drivers

Source drivers for the Lenovo Tab M11 TB330XU (`barley_row_lte`).  Hardware
resources are described by the platform ACPI tables in Mu-Silicium; the
drivers consume those resources instead of embedding board addresses in PnP
matching code.

- `Input/BarleyInput`: physical power and volume keys exposed through VHF.
- `Msdc`: MediaTek MSDC storage-controller work.
- `Touch/BarleyHimaxTouch`: HX83102J zero-flash touchscreen over MT6768 SPI0.

Build products and vendor firmware packages are intentionally excluded from
source control.  Each driver directory documents its own ARM64 build and
deployment procedure.
