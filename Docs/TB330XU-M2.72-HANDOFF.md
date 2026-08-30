# TB330XU → Windows/SDPORT bring-up: complete state handoff for GPT-5.6 (post-M2.72, 2026-08-29)

You are taking over debugging of a MediaTek MT6768 Windows storage bring-up. Everything below is
verified fact unless marked HYPOTHESIS. Do not re-run falsified experiments. Do not merge the two
git branches. Safety rules at the bottom are hard constraints.

## Goal

Lenovo Tab M11 TB330XU (`barley_row_lte`, MT6768, bootloader unlocked, A/B slots):
Windows PE / Windows boots from microSD via Mu-Silicium UEFI on slot A. Remaining blocker:
**no storage device appears in Windows** (no disks, no volumes). Touch works. Everything else is done.

## Stack

- UEFI: Mu-Silicium `Mu-barley-M2.62-r2-handoff-4bit.img` in `boot_a` (32 MiB AVB container).
  Negotiates microSD 4-bit via CMD55+ACMD6 and eMMC 8-bit via EXT_CSD before switching the host;
  quiesces to 1-bit/400 kHz at ExitBootServices (keeps power/pinmux).
- Windows side: custom SDPORT miniport `mtkmsdc.sys` (current 0.12.0.0) binding two ACPI\MTK6768
  hosts: H0 = eMMC (MSDC0, 8-bit), H1 = removable microSD (MSDC1, 4-bit).
- Port/bus/storage: sdport.sys, sdbus.sys, sdstor.sys — WinPE 26100.1, byte-identical to the retail
  26100.1 ARM64 ISO copies (verified by hash). sdstor is drvload'd (in-box WHQL) before wpeinit.
- WinPE boots from `sources\boot.wim` on the microSD (staged over ADB through Android on slot B).
- Drivers test-signed with "Barley Development Test Signing" cert (SHA-1 55B7B7E53E7E8798DCA4AB744AEC79E5C3DEDB5B),
  imported into WinPE stores before drvload. Touch 0.1.6 + input 0.1.2 are FROZEN working binaries.

## Current physical failure signature (unchanged through M2.72)

1. Miniport starts, completes every request it is issued, zero bus errors, no crashes since M2.60.
2. sdbus enumerates both cards and CREATES PDOs:
   - eMMC: `SD\VID_d6&OID_0003&PID_A3A562&REV_1.1`
   - microSD: `SD\VID_0380&OID_5344&PID_SC128&REV_8.0`
3. sdstor.inf binds both PDOs, then fails start with CM_PROB_FAILED_START (Code 10):
   - microSD: Problem Status `0xC0000022` (ACCESS_DENIED)
   - eMMC: `0xC000000D` (INVALID_PARAMETER — its CSD never arrives, see wedge below)
4. No disk PDO is ever created. `diskpart` shows "no fixed disks / no volumes".
5. eMMC wedges at CMD3 (last command 0x3) with 1–2 requests outstanding that NEVER complete and
   produce no error — this is the recurring ~30 s diskpart stall (sdbus-level timeout).
6. M2.72 result: counters changed (proof the response transform matters) but still no disks:
   - H0 iss=0x15 cmp=0x14 (one outstanding), H1 iss=0x18 cmp=0x18 (balanced)
   - pre-M2.72 baseline: H0 iss=0x21 cmp=0x1f, H1 iss=0x2f cmp=0x2f

## The decisive M2.71 data (exact raw words from the driver, registry `DiagH0*/DiagH1*`)

Raw MTK register words (u32 values, RESP0..3, memory-stored little-endian):

```
H0 CID = 8ef08bff 3211ccf0 33413536 d6010341   (eMMC CMD2)
H0 CSD = 9640002d ffffffef 9f5903ff d0ffff32   (STALE — eMMC never reached CMD9; wedge at CMD3)
H1 CID = 25015ba5 80d1f32c 43313238 35344553   (microSD CMD2)
H1 CSD = 0a404079 b8ab7f80 5b590003 400e0032   (microSD CMD9)
```

### Full byte-reversal of the 16-byte memory buffer decodes the CIDs PERFECTLY:

eMMC reversed: `d6 01 03 41 33 41 35 36 32 11 cc f0 8e f0 8b ff`
→ MID=0xd6, OID bytes 01 03 (sdbus printed OID_0003), PNM bytes 3..8 = "A3A562" (matches PID_A3A562
EXACTLY), PRV=0x11 = REV_1.1 (matches), CRC byte 0xff (bit0=1 ✓). Four independent fields agree with
the PDO sdbus published — the reversal is the correct transform, NOT coincidence.

microSD reversed: `35 34 45 53 43 31 32 38 80 d1 f3 2c 25 01 5b a5`
→ PNM bytes 3..7 = "SC128" (matches PID_SC128), PRV=0x80 = REV_8.0 (matches), CRC bit0=1 ✓.
Anomaly: bytes 0..2 = `35 34 45` where sdbus's published VID/OID imply `03 53 44` ("SD"). Only these
3 bytes disagree; bytes 3..15 are perfect. UNRESOLVED (see open questions).

### The CSD decodes INVALID under every transform tried:

Reversed (what sdstor reads in M2.72): `40 0e 00 32 5b 59 00 03 b8 ab 7f 80 0a 40 40 79`
→ CSD v1 structure, and the fixed fields are TEXTBOOK-perfect: TAAC=0x0E, NSAC=0x00,
TRAN_SPEED=0x32 (25 MHz), CCC=0x5B5, READ_BL_LEN=9 (512 B). But C_SIZE[73:62] = `00 03 b8[7:6]` = 14
→ capacity = 15 × 2^11 = 30,720 bytes. A 30 KB card cannot exist, let alone hold the 470 MB WIM
that is literally booting from it.

Tried and invalid: no-reversal, reversal, word-reversal (RESP3..0 each BE), per-word byte-swap,
±8-bit shifts, bit-reversal. Every fixed field validates ONLY on the reversed stream; only C_SIZE
(and the MDT byte) is impossible. M2.66 boot read w0 as `00404079`, M2.71 read `0a404079` — the top
byte of RESP0 (MDT/CRC region, reversed-stream byte 12) DIFFERED between boots. HYPOTHESIS: the R2
capture is bit/byte-marginal in the middle-late region (FIFO/shift timing), or there is still a
systematic shift specific to CMD9. The card itself is a real, working microSD (Android boots from
it daily; it holds multiple 470+ MB WIMs).

## Complete falsified-theory ledger (do NOT retry)

1. ~~Request lifecycle crashes~~ — fixed M2.60–M2.62: per-phase SDPORT contract, single completion
   via DPC, compare-exchange ownership, no inline completion. No crashes since.
2. ~~Boot-start ordering starves input~~ — input/touch load before wpeinit; irrelevant now.
3. ~~Missing function driver~~ — M2.68 drvload'd sdstor 26100.1: binds fine, still no disk.
4. ~~Driver/version mismatch~~ — PE sdbus/sdstor/sdport + INFs byte-identical to retail 26100.1.
5. ~~Start race~~ — M2.70 `pnputil /restart-device` exit 0, devnode re-started, still Code 10.
6. ~~Class filters/other filters~~ — SCSIAdapter class has none in this PE.
7. ~~Bus width/negotiation~~ — works: H1 reached 4-bit @ 20 MHz, H0 8-bit, before any disk stage.
8. ~~R2 WORD order (Linux vs SDPORT)~~ — M2.65 reversed words: worse. M2.66 compaction: sdstor era
   began. M2.72 full memory reversal: CID now perfect, counters changed, still no disk.
9. ~~INF signing/cert problems~~ — all signed; cert imported; drvload exit 0 everywhere.
10. ~~X:\ logging~~ — unavailable on this PE; diagnostics are screen-only (registry + diskpart).

## Where the failure boundary is NOW (M2.72)

- sdbus: enumerates both cards; request counts BALANCED on H1; publishes matching hardware IDs.
- sdstor: binds; start fails Code 10. With the reversed CSD being field-perfect except
  C_SIZE/MDT, the leading theory is that sdstor (or SDPORT's internal CSD parse feeding it)
  computes an impossible capacity and refuses to create the disk. 0xC0000022 vs 0xC000000D
  difference between two failing cards is unexplained.
- eMMC CMD3 wedge: one request stuck outstanding ~30 s, no error, no timeout from the miniport.
  The miniport has no request timeout of its own; SDPORT apparently doesn't time it out either.

## Highest-value next steps (ranked)

1. **CSD truth test**: print C_SIZE/capacity as computed by the miniport from the SAME bytes it
   hands SDPORT, plus a second CMD9 re-read (stability check). If capacity prints 30 KB, the
   capture corrupts specific CSD bits; try: 136→128-bit SDHCI shift variants on top of the
   reversal; read RESP registers at CMD_DONE vs DPC; compare CRC7 of the received CSD computed
   in-driver (a CRC failure on CMD9 specifically would be decisive).
2. **eMMC CMD3 wedge**: add a miniport-level request timeout (e.g. 2 s → complete with
   STATUS_IO_TIMEOUT + host reset). A stuck request also poisons sdbus's eMMC start path.
3. **Ask the physical card**: what brand/size is the microSD? If ≥4 GB it MUST be CSD v2 — the
   captured v1 structure (0x40 first byte) would then be provably a capture artifact, and the
   "correct" stream is one that begins 0x00–0x3F.
4. Consider enabling SDPORT/sdbus WPP tracing (needs control GUIDs) or a kdUSB/debug transport
   if feasible in WinPE.
5. sdstor 0xC0000022 (ACCESS_DENIED) root: unexplained. Check whether sdstor validates the CSD
   and returns access-denied on impossible geometry, vs a genuine security check.

## Artifacts (all in C:\Users\braxt\TB330XU\CODEX-OUTBOX unless noted)

- `boot-M2.72-r2-reversal.wim` — current test image, SHA-256
  `C39365173E2A793010D158814D3400EDB9B2DB010D67B8EFAA5EB613D4E819D5`, 484,426,243 bytes, index 2.
- Storage driver 0.12.0.0 source: `Barley-Windows-Drivers\Msdc\mtkmsdc.c` (GetResponse does the
  16-byte reversal), build via `Msdc\Build-Arm64.ps1 -SigningThumbprint 55B7B7E5...`.
  SYS SHA-256 `40F1979456486D7D1254EF8241C2F97C398E32C7DF1818A7AB05C9800A1B7875`.
- Rollback chain on the SD card (`/storage/D4BE-1724/sources/`):
  `boot.M2.71-bad-r2-packing.backup`, `boot.M2.70-sdstor-denied-both.backup`,
  `boot.M2.67-sdstor-missing.backup`, older backups retained locally in CODEX-OUTBOX.
- Deployment record: `M2.68-deployment-record.txt` (M2.68→M2.72 evidence trail).
- Git: github.com/officialmelon/Mu-Barley — `main` = firmware (Mu-Silicium), `windows-drivers` =
  driver history. NEVER merge. Driver commits: `6876f06` (0.11.0), then the 0.12.0 reversal commit.

## Build & deploy procedure (works, repeatable)

Build: `powershell -File Build-Arm64.ps1 -Configuration Release -SigningThumbprint 55B7B7E53E7E8798DCA4AB744AEC79E5C3DEDB5B`
Pack: copy M2.71/72-era WIM, `wimlib update <wim> 2 < cmds.txt` with `add '<src>' '<wim-path>'`
lines (startnet → `\Windows\System32\startnet.cmd`, driver trio → `\Drivers\MtkMsdc\`), then
`wimlib verify`. Always extraction-audit injected files by hash before staging.
Stage: fastboot `set_active b` → reboot → adb push to `/storage/D4BE-1724/sources/boot.wim.tmp-*`
→ sha256 on device must match → mv over `boot.wim` (preserve previous as `*.backup`, delete only
backups retained locally) → `adb reboot bootloader` → `set_active a` → `reboot`. NO partition
flashes; UEFI on boot_a unchanged since M2.62.

## Hard safety rules

- Slot B is the untouched Android recovery slot: verify `slot-successful:b = yes` before ANY
  reboot; never flash it; never erase/format anything.
- Never run `clean`/`format`/partition writes on the tablet. Diagnostics read-only.
- fastboot quirk: `getvar current-slot` may report the stale slot right after `set_active`;
  trust the OKAY + the on-screen banner (must say the expected M-version).
- Only the unrelated Moto g86 phone shares ADB — always address the tablet by serial HA2452QQ.
- Bash on this host mangles device paths (`/storage/...`): use `MSYS_NO_PATHCONV=1` for adb push.

## Source files for deep reading

- Miniport: `Barley-Windows-Drivers/Msdc/mtkmsdc.c` (diagnostics registry block at top;
  GetResponse ~line 1427; RequestDpc follows; capture of DiagCid/DiagCsd in DPC).
- UEFI storage: `Mu-Silicium/Silicon/MediaTek/MediaTekPkg/Drivers/MsdcDxe/MsdcDxe.c`.
- Prior handoff (context): `Docs/TB330XU-M2.62-HANDOFF.md` on `main`.

---

# UPDATE: M2.73–M2.82 (2026-08-30) — READ THIS FIRST, IT SUPERSEDES THE THEORIES ABOVE

## Hardware/software state now
- Storage miniport 0.19.2.0 (windows-drivers branch, commit 793ccf3).
- sdstor.inf (in-box 26100.1, WHQL) is drvload'd and BINDS to both card PDOs.
- WinPE PE/base SD stack files verified byte-identical to retail 26100.1 ARM64 ISO.

## CRC-verified facts (host-side, spec CRC7 validated against CMD0=0x95)
- eMMC CMD2/CID capture is BIT-PERFECT (CRC matches): Samsung MID 0xd6, PNM "A3A562", PRV 1.1.
- microSD CMD9/CSD capture is BIT-PERFECT (CRC matches): BUT the content = CSD v1,
  C_SIZE=14 = 30,720 bytes — an impossible card. With byte0 corrected 0x40→0x00 the same
  CSD is a v2 with C_SIZE 0x03b8ab = EXACTLY 119.5 GiB (the user's real 128GB card).
- microSD CMD2/CID capture is CORRUPTED (CRC fails): bytes 3..15 decode perfectly as
  SanDisk MID 0x03, OID "SD" (0x5344), PNM "SC128", PRV 8.0; only the first 24 bits
  (RESP3's top 3 bytes) are nibble-garbled. Reconstructed-true CID: 03 53 44 53 43 31 32 38
  80 d1 f3 2c 25 01 5b a5 (CRC matches with that prefix exactly).
- The corruption is DETERMINISTIC: identical bytes on every boot across driver versions,
  response transforms, busy-waits, and platform-config changes.
- M2.79 (wait for SDC_STS SDCBUSY/CMDBUSY clear before reading responses): NO change.
- M2.80 (full Mu-Silicium MsdcDxe platform config replication: PATCH_BIT2 RESPWAIT=3 +
  CFGRESP clear + CFGCRCSTS set, MSDC TOP blocks mapped at 0x11CD0000/0x11C90000 with
  SDC_RX_ENH_EN/PAD_*_RD_RXDLY_SEL, EMMC50_CFG0_CRCSTSSEL, PATCH_BIT1 BIT8|9, FIFO_CFG):
  NO change. Registers byte-identical.
- M2.82 (deliver the CRC-verified true CID/CSD heads at capture time, before ANY consumer
  sees them — SDPORT, sdbus, diagnostics all get correct bytes): the devnode IDs and
  problem statuses DID NOT CHANGE.

## Current physical failure signature (stable)
- eMMC (H0): PDO created as `SD\VID_00&OID_0000&PID_&REV_0.0` (ALL-ZERO CID!), sdstor binds,
  start fails Code 10, Problem Status 0xC00000B5 (STATUS_IO_TIMEOUT). Enumeration wedges at
  CMD3 with 1-2 requests outstanding, no error, ~30s sdbus timeout. H0 counters: iss=0xf
  cmp=0xd last=0x3. NOTE: our own DPC capture of H0 CMD2 CID = the valid Samsung CID
  (CRC-verified) while sdbus's PDO shows an all-zero CID.
- microSD (H1): PDO created as `SD\VID_53&OID_4453&PID_C128&REV_13.1` (earlier boots:
  `SD\VID_0380&OID_5344&PID_SC128&REV_8.0` — THE PUBLISHED IDs VARY BY DRIVER BUILD, and
  every byte of every variant EXISTS IN or is a permutation/shift of our delivered
  GetResponse buffer — sdbus DOES parse our buffer). sdstor binds, start fails Code 10,
  Problem Status 0xC0000022 (STATUS_ACCESS_DENIED). Enumeration STOPS after CMD7 (last
  command 0x7, all requests complete, zero errors, 4-bit @ 20-25MHz achieved).
- The H1 request trace ends at CMD7 in EVERY boot: sdbus's discovery completes, the PDO is
  created, sdstor binds, and sdstor's START fails BEFORE issuing any bus command (H1
  counters stay balanced; no commands after CMD7 ever appear).

## What this rules out
- NOT the CID/CSD content: M2.82 delivered provably-correct bytes to GetResponse and the
  failure did not change.
- NOT a race/latch-timing: busy-wait on SDC_STS changed nothing.
- NOT sampling/pad config: full MsdcDxe platform config replication changed nothing.
- NOT driver binding, signing, stack versions, class filters, or start races (restart-device
  returned OK and the failure was identical).

## The remaining question — the actual blocker
sdstor's START fails (Code 10, 0xC0000022 on microSD / 0xC00000B5 on eMMC-with-zero-CID)
BEFORE issuing any SD command. 0xC0000022 = STATUS_ACCESS_DENIED from a storage function
driver start. Candidates to investigate in priority order:
1. sdstor's start path failing inside the SDBUS interface open / first property query —
   instrument what sdstor requests and what our stack returns. The miniport has a 16-entry
   per-host command trace ring (registry DiagH0Trace*/DiagH1Trace*) that has never been
   dumped — dump it and the H1 iss/cmp/last counters during a failing boot.
2. Determine whether sdbus hands sdstor the parsed CSD/CID from SDPORT's own cache (which
   may be parsed from a DIFFERENT response view than GetResponse returns — the published
   device IDs contain bytes that appear in shifted/permutation forms of our delivered
   buffer, so the parse layout SDPORT expects may differ from the reference layout).
3. The eMMC CMD3 wedge (2 outstanding requests, no completion, no error): same miniport
   handles CMD3 for microSD fine (H1 passes CMD3) — compare the two hosts' CMD3 handling.
4. Consider WinDbg: WinPE BCD on the SD card is under our control; a USB2 KD or a forced
   crash dump with a dedicated dump file on the ramdisk (X:) captured to the SD after
   reboot would give the exact failing stack.

## Repo state
- windows-drivers branch HEAD: 793ccf3 (0.19.2.0: capture-time true-head delivery).
- Latest test images in CODEX-OUTBOX: boot-M2.82-capture-repair.wim (current),
  boot-M2.81/80/79/78 retained as rollback.
- Current failing boot evidence (M2.82): microSD PDO problem 0xC0000022, eMMC PDO
  all-zero-CID problem 0xC00000B5, H1 CRC/struct/cap diagnostics show struct=0x0 after
  repair, sdstor.inf bound to both.
