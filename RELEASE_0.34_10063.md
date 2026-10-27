# Release note: my12 10063 (fw 0.34), 28 Oct 2026

Branch `experiment/agc-tt-decouple` (the 10062 trial line). Two independent changes, in two commits so the LCD one can be cherry-picked to master on its own.

## 1. Server control of the PGA gain (keys `agc` and `pga`)
Why: detector 13 showed the fixed near-miss band (10062) drives its PGA gain to the bottom (steps of 1-2, AGC pressure about 133) while the stock band (`nmm:0`) leaves it at 9.
To investigate a noisy site the server needs to hold the gain; until now only the console (Ctrl-C then a digit) and the HTTP page could.

| Key | Meaning | Range |
|---|---|---|
| `agc` | 0 holds the PGA gain where it is (as console Ctrl-C), 1 hands it back to the AGC | 0-1 |
| `pga` | PGA gain step to hold while the AGC is off | 0-9 |

- `pga` is applied in the main loop (one user of the PGA's SPI bus), once per reply that carries the key, and only when `agc` is 0. A reply without `pga` never re-applies an old value, so a console change is not overridden.
- A missing key leaves the setting unchanged: a server that sets `agc:0` must send `agc:1` explicitly to release the AGC. After a reboot `agc` is 1 until the next reply says otherwise.
- Reply budget: the brace list is parsed with `%255s` and the page buffer is 540 bytes. `agc:0,pga:5` adds 12 characters to a 132-169 character reply.
- With the AGC off the floating threshold is the only automatic protection: a near-miss cascade raises `trigthresh` by 20 (capped at 100 counts) and the jabber guard does not lower the gain.
- Verifying from the status packet (no spare bits for a flag): `pgagain` equals the forced value, `gain_min` equals `gain_max`, `gain_steps` is 0.

## 2. LCD grid locator
10 characters (field, square, subsquare, extended square, sub-extended square: 1.25" x 0.625", about 34 x 19 m at Brisbane) and no space after `Grid:`, so the line is 15 characters like the `Lat:` and `Lon:` lines. `calcLocator()` needs an 11 byte buffer; the `t2` text object (64 characters) takes the 49 character string. Only `miscutils.c` and the format string in `lcd.c` change.

## Images
Same A/I scheme as before: PCB 22 and PCB 11 share one image, PCB 21 has its own (`LIGHTNINGBOARD1_BUILD`).

| File | Bytes | CRC |
|---|---|---|
| `my12-A22-10063.bin`, `my12-A11-10063.bin` | 308128 | `0xcc9a78de` crc1 |
| `my12-I22-10063.bin`, `my12-I11-10063.bin` | 308128 | `0xe9a86d75` crc2 |
| `my12-A21-10063.bin` | 308248 | `0x318aadb7` crc1 |
| `my12-I21-10063.bin` | 308248 | `0xd9282032` crc2 |

Built with the CubeIDE gcc 14.3 toolchain from a scratch copy of the tree (`build_my12.sh`); the same procedure reproduced the published 10062 CRCs byte for byte.

## Rollout
Test on the local units first (15, then 18 and 3), then any remote unit that needs it. Rollback: advertise 10062 (units on 10062 ignore the new keys, so the server can send them safely).
