# Major bugs found while fixing the build (2026-09-16)

The project failed to build under STM32CubeIDE 2.1.1 / GCC 14 (which turns several
previously-warnings-only issues, like implicit function declarations, into hard
errors). Getting it compiling again surfaced ~70 issues; most were simply missing
`#include`s or prototypes for functions that already existed elsewhere. Those aren't
listed here - see `git log fix/compile-errors` for the full, commit-by-commit trail
(every fix was verified by rebuilding the affected file before committing, so any
single step can be reverted independently).

This file covers only the fixes that were **real bugs**, not just missing
declarations - things that would have caused incorrect behavior on the actual
hardware, independent of the compiler version.

## 1. ADC DMA completion callbacks read the wrong handle type
**[Core/Src/adcstream.c](Core/Src/adcstream.c)** - `ADC_MultiModeDMAConvM0Cplt`/`M1Cplt`

`HAL_ADCEx_MultiModeStart_DBDMA` (a custom copy of the HAL function, defined in this
file) wired these two up as `hdma->XferCpltCallback` / `XferM1CpltCallback`. The DMA
IRQ handler always invokes those as `callback(hdma)`, passing a `DMA_HandleTypeDef*`
- but both functions were declared taking `ADC_HandleTypeDef*`. Every ADC DMA
completion was silently reinterpreting the DMA handle as an ADC handle and reading
garbage struct fields. The fix matches the pattern already used correctly three
times earlier in the same file: take `DMA_HandleTypeDef *hdma` and recover the real
ADC handle via `hdma->Parent`.

## 2. DNS-lookup failure wrote a string into a binary IP address field
**[Core/Src/udpstream.c](Core/Src/udpstream.c)** - `dnslookup()`

On DNS failure, the fallback was:
```c
ip->addr = "127.0.0.1";   // safe ?
```
`ip->addr` is a `u32_t` holding a raw binary IPv4 address, not text - this assigned
a string literal's pointer value into it. Replaced with `IP4_ADDR(ip, 127, 0, 0, 1);`,
which correctly encodes the same intended loopback fallback.

## 3. `_write()` passed a byte value where a pointer was needed
**[Core/Src/main.c](Core/Src/main.c)** - `_write()`

```c
HAL_UART_Transmit(&huart5, /*(uint8_t*)*/*ptr++, 1, 10);
```
A cast had been commented out here instead of fixing the actual bug: the stray `*`
dereferences `ptr`, passing a single **byte value** where the function needs the
**address** to read from. Fixed by passing `ptr` (and still advancing it): this path
(`_write` called with `file != 1`) would very likely have hard-faulted if ever
exercised.

## 4. Boot-bank-swap logic had a stray pointer type, and a check that could never work
**[Core/Src/eeprom.c](Core/Src/eeprom.c)** - `stampboot()` / `swapboot()`

Both declared `uint32_t *newadd` and assigned it a small option-byte value
(`0x2000`/`0x2040`) meant for `FLASH_OBProgramInitTypeDef.BootAddr0`, which is a
plain `uint32_t` value, not a pointer - a stray `*` in the declaration.

`stampboot()` never dereferenced `newadd`, so it was a pure type-declaration bug.
`swapboot()` *did* dereference it - `if (*newadd != 0xffffffff)` - apparently trying
to check whether the target boot bank looked erased before switching to it. But
`newadd` only ever held that tiny option-byte value, never a real flash address, so
this could never have been reading actual boot-bank content. Fixed both
declarations to plain `uint32_t`, and removed the non-functional check in
`swapboot()` (confirmed with the user, since this is live logic called from
`main.c` on every boot via a button-press bank swap) - it now always applies the
toggled boot address, matching `stampboot()`'s existing behavior.

## 5. The web UI's SSI tag handler had the wrong return type
**[Core/Src/www.c](Core/Src/www.c)** - `tag_callback()`

```c
tSSIHandler tag_callback(int index, char *newstring, int maxlen) {
    ...
    return (strlen(newstring));
}
```
`tSSIHandler` is a function-pointer *typedef* (`u16_t (*)(...)`), not a valid return
type for an actual function - a naming mixup. This broke both the function's own
return statement and its use as the `http_set_ssi_handler()` argument. Its parameter
list already matched lwIP's expected signature exactly; only the return type
(`u16_t`) needed correcting.

## 6. A one-character typo in ST's own vendored HAL header
**[Drivers/STM32F7xx_HAL_Driver/Inc/Legacy/stm32_hal_legacy.h:482](Drivers/STM32F7xx_HAL_Driver/Inc/Legacy/stm32_hal_legacy.h)**

```c
#define FLASH_ERROR_PGP   HAL_FLASH_ERROR_PGS   // should be HAL_FLASH_ERROR_PGP
```
Every other alias in this file maps an old name to the identically-suffixed current
one; this line aliases to a symbol that doesn't exist anywhere (`PGS` instead of
`PGP`). Rather than edit vendored ST code (a future CubeMX regeneration could
silently overwrite the fix), `eeprom.c`'s `printflasherr()` was changed to reference
the real symbol, `HAL_FLASH_ERROR_PGP`, directly.

## 7. `rebootme()` called with no argument
**[Core/Src/httpclient.c](Core/Src/httpclient.c)** - `hc_open()`

`rebootme(int why)` forwards `why` only to `err_leds()` to pick a diagnostic LED
pattern before resetting - nothing else depends on its value. The call here (hit
after too many unanswered polls to the control server) was missing the argument
entirely. Used `7` ("server lookup failed") as the closest existing code; trivial to
change later since it has no other effect.

## Not a code bug, but cost real debugging time
**Debug launch config**: the "Lightningboard" launch configuration in the
STM32CubeIDE workspace had empty `PROJECT_ATTR`/`PROGRAM_NAME` fields (likely
orphaned from an earlier project rename - the same failure previously showed up
under yet another old name, "h55-test2"). Launching it threw an internal
`NullPointerException` before ever touching the ST-Link. The project's own
**`my12 Debug.launch`** is correctly configured (`PROJECT_ATTR=my12`,
`PROGRAM_NAME=Debug\my12.elf`) and works - use that one.
