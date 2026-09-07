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
- 3.3 V operation with a conservative 20 MHz clock ceiling.
- PIO single- and multi-block reads/writes; Auto CMD12 is not advertised.
- Controller reset, FIFO clearing, W1C interrupt handling and error recovery.
- One outstanding request; no DMA and no crash-dump claim.

## Remaining production work

1. Replace inherited clock/rail/pin ownership with MT6768 clock, PMIC and GPIO
   dependencies that support D-states, cold start and resume.
2. Add GPIO18 card-detect debounce and surprise-removal handling.
3. Add a production DMA data path; the current polling PIO path is correct but
   far too slow while Windows probes the many Android eMMC partitions.
4. Preserve the hardware-derived SDHCI-format R2 response conversion validated
   by both SD and eMMC enumeration; do not add CID/CSD identity substitutions.
5. Add eMMC HS200 and SD high-speed tuning only after normal-speed stability.
6. Qualify flush, removal, reboot, hibernation and crash-dump behavior before
   treating eMMC as a production Windows system disk.

Both controllers now enumerate and complete read-only DiskPart discovery. The
next validation is repeated cold-boot reads followed by a bounded file-level
write/read/flush test on the existing microSD FAT32 filesystem. Never automate
partition or raw-sector writes to eMMC.
