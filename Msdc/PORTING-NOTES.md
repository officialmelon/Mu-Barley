# MT6768 MSDC runtime contract

## Why SDPORT

Windows already supplies SD/MMC protocol and disk layers in `sdport.sys`,
`sdbus.sys`, and `sdstor.sys`. The Barley driver is therefore only a hardware
miniport. Using StorPort would duplicate card-protocol and media behavior.

## Resources and inherited state

| Host | Main window | MSDC-TOP | Interrupt | Media |
| --- | --- | --- | --- | --- |
| 0 | `0x11230000 + 0x10000` | `0x11CD0000 + 0x1000` | SPI 100 / GSI 132 | eMMC, 8-bit |
| 1 | `0x11240000 + 0x10000` | `0x11C90000 + 0x1000` | SPI 101 / GSI 133 | microSD, 4-bit |

ACPI exposes only the main register window consumed by SDPORT. The established
Mu-Silicium handoff leaves both controllers' gates, rails, pins and top-level
tuning usable; the Windows miniport does not rewrite MSDC-TOP.

## Implemented contract

- One ACPI/INF hardware ID and one binary for both hosts.
- Host identity determined from the translated physical base address.
- eMMC permanently present; microSD present-at-boot until GPIO18 support.
- Normal-speed 3.3 V operation with a 25 MHz ceiling.
- PIO single- and multi-block reads/writes with Auto CMD12 support.
- Controller reset, FIFO clearing, W1C interrupt handling and error recovery.
- One outstanding request; no DMA and no crash-dump claim.

## Remaining production work

1. Replace inherited clock/rail/pin ownership with MT6768 clock, PMIC and GPIO
   dependencies that support D-states, cold start and resume.
2. Add GPIO18 card-detect debounce and surprise-removal handling.
3. Convert the conservative polling path to interrupt/DPC completion and add
   DMA after correctness is established.
4. Validate R2/CID/CSD response ordering against Windows traces.
5. Add eMMC HS200 and SD high-speed tuning only after normal-speed stability.
6. Qualify flush, removal, reboot, hibernation and crash-dump behavior before
   treating eMMC as a production Windows system disk.

The first deployment must enumerate and read both controllers before any
automated write test. A write test belongs on a disposable file on microSD;
never automate partition or raw-sector writes to eMMC.
