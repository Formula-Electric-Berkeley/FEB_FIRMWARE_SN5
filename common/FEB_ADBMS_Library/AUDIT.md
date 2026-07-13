# ADBMS6830B command/register audit — status

Goal: every opcode and bitfield in this library is traceable to a source, with
the **datasheet as final authority**. Three sources are used:

- **DS** — ADBMS6830B datasheet (PDF pending — final authority; items marked
  DS-pending must be re-checked when it lands in the repo)
- **ADI** — ADI reference driver command list, cross-read 2026-07-12 from the
  mirror in `ManchesterStingerMotorsports/g474-bms`
  (`Core/custom_lib/{inc,src}/bms_cmdlist.{h,c}`, derived from ADI's
  ADBMS6830 reference code)
- **SN5** — field-proven headers on `main` before the rewrite
  (`ADBMS6830B_Commands.h`, `FEB_CMDCODES.h`, ran on the SN5 accumulator)

## Verified (ADI + SN5 agree; treated as correct pending DS spot-check)

Config: `WRCFGA 0x0001, RDCFGA 0x0002, WRCFGB 0x0024, RDCFGB 0x0026`
Cell V: `RDCVA 0x0004, RDCVB 0x0006, RDCVC 0x0008, RDCVD 0x000A, RDCVE 0x0009,
RDCVF 0x000B, RDCVALL 0x000C`
Avg V: `RDACA 0x0044, RDACB 0x0046, RDACC 0x0048, RDACD 0x004A, RDACE 0x0049,
RDACF 0x004B, RDACALL 0x004C`
S V: `RDSVA 0x0003, RDSVB 0x0005, RDSVC 0x0007, RDSVD 0x000D, RDSVE 0x000E,
RDSVF 0x000F, RDSALL 0x0010, RDCSALL 0x0011, RDACSALL 0x0051`
Filtered: `RDFCA..RDFCF 0x0012..0x0017, RDFCALL 0x0018`
Aux: `RDAUXA 0x0019, RDAUXB 0x001A, RDAUXC 0x001B, RDAUXD 0x001F` (C/D from
SN5 only), `RDRAXA 0x001C, RDRAXB 0x001D`
Status: `RDSTATA..RDSTATE 0x0030..0x0034, RDASALL 0x0035`; `RDSTATC` ERR bit =
CC6 (ADI lists RDSTATC|ERR = `0x0072`, matching our `RDSTATC(err)` macro)
Clear: `CLRCELL 0x0711, CLRAUX 0x0712, CLRFC 0x0714, CLOVUV 0x0715,
CLRSPIN 0x0716, CLRFLAG 0x0717`
Poll: `PLADC 0x0718, PLCADC 0x071C, PLSADC 0x071D, PLAUX 0x071E, PLAUX2 0x071F`
Comm: `WRCOMM 0x0721, RDCOMM 0x0722`
Control: `MUTE 0x0028, UNMUTE 0x0029, SRST 0x0027, RDSID 0x002C, SNAP 0x002D,
RSTCC 0x002E, UNSNAP 0x002F`

ADC command composition (verified against ADI's `ADCV_t/ADSV_t/ADAX_t`
bitfield structs):

- `ADCV 0x0260` + RD=CC8, CONT=CC7, DCP=CC4, RSTF=CC2, OW[1:0]=CC[1:0]
- `ADSV 0x0168` + CONT=CC7, DCP=CC4, OW[1:0]=CC[1:0]
- `ADAX 0x0410` + OW=CC8, PUP=CC7, **CH4=CC6**, CH[3:0]=CC[3:0]

## Fixed in this audit pass

- `ADAX(ow, pup, ch)` shifted CH4 to CC10 (`(ch & 0x10) << 6`), colliding with
  the base command's bit 10 — every channel ≥ 0x10 (VREF2/VD/VA/ITEMP/VPV/
  VMV/RES) silently measured a GPIO instead. Now `(ch & 0x10) << 2` → CC6.
- `ADAX_PUP` flag was `1 << 6` (that's CH4); now `1 << 7` per ADI `ADAX_t`.
  Added `ADAX_CH4 (1 << 6)`.

## Unverified — re-check against the datasheet when the PDF lands

| Item | Current value | Sources so far |
| --- | --- | --- |
| `ADAX2` CH placement | `0x0400 \| CH[3:0]` | ADI bitfield packing ambiguous |
| Retention `ULRR/WRRR/RDRR` | `0x0038/0x0039/0x003A` | branch only |
| LPCM `CMDIS/CMEN/CMHB` | `0x0040/0x0041/0x0043` | branch only |
| LPCM `WRCMCFG..RDCMFLAG` | `0x0058..0x005F` | branch only |
| `RDRAXC/RDRAXD` | `0x001E/0x0025` | branch + SN5 |
| PWM `WRPWMA/WRPWMB/RDPWMA/RDPWMB` | `0x0020/0x0021/0x0022/0x0023` | branch + SN5 |
| `STCOMM` | `0x0723` | branch + SN5 |
| VUV/VOV encoding | 12-bit, 2.4 mV LSB, +1.5 V offset; VUV has −1 offset | SN5 driver corroborates the offset; **−1 asymmetry and code signedness (sub-1.5 V thresholds) DS-pending** (Table 107) |
| STATD `C_UV/C_OV` bit layout | per `RDSTATD_DECODE` | DS-pending |
| Daisy-chain read byte order | parse loop maps first-received → IC0 | verify on 10-IC hardware + DS |
| ADI lists `PLAUT 0x0719`, `DIAGN 0x0715` | not defined here | `DIAGN` collides with `CLOVUV` in the mirror — DS must adjudicate before either is added |

## Guard

`scripts/check-adbms-opcodes.py` fails the build/pre-commit if two plain
`#define` opcodes in `ADBMS6830B_Commands.h` share a value (the pre-rewrite
header shipped `CMHB 0x0011` colliding with `RDCSALL` — this catches that
class of error).
