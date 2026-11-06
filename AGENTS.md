# AGENTS.md

Guidance for AI coding agents (and humans) working on this repository.

## Project

ESP32 firmware for an Inkplate 10 / Inkplate 5 V2 e-paper home dashboard: clock,
SMHI weather forecast and realtime SL departures. Arduino framework, built with
PlatformIO. User settings live in `src/config.h`.

## Build and test

```
pio run                                # all firmware environments
pio run -e release_inkplate10v2        # one board
pio test -e native                     # host-side unit tests (needs a host gcc)
```

Firmware environments: `debug_inkplate10v2`, `release_inkplate10v2`,
`debug_inkplate5v2`, `release_inkplate5v2`. The `native` environment compiles only
the files listed in its `build_src_filter`, so code that should be unit tested must
not depend on Arduino or Inkplate headers.

Formatting is defined by `.clang-format` (see the command in that file). Generated
font and icon headers are excluded.

## Code structure

The code is organised as free functions, plain data structs, and a strict split
between fetching data and drawing it. Do not introduce classes for modules that
hold no state.

| Layer | Form | Rules |
|---|---|---|
| Data | `struct` with public fields | No methods beyond trivial helpers. No JSON or HTTP types. |
| Fetch | free function returning `bool` | Performs HTTP and parsing, fills a data struct passed by reference. Never draws. |
| Parse | free function, no Arduino dependencies where possible | Turns API payloads into data structs so it can be unit tested on the host. |
| Draw | free function taking `const` data by reference | Never performs network calls. Same input gives the same output. |
| State | owned by `main.ino` | Cached data (e.g. the last weather forecast) lives in `main.ino` and is passed to fetch and draw functions. No hidden module globals. |

Draw functions take the display and fonts first, then the data, then the position:

```cpp
void drawWeather(Inkplate &display, const FontCollection &fonts, const WeatherData &weather, int xpos, int ypos);
```

## Conventions

- **Linkage:** helpers and constants used by a single `.cpp` file are `static`.
  Only what a header declares has external linkage.
- **Parameters:** references for required arguments, pointers only when null is a
  meaningful value. Pass small values (`float`, `int`) by value.
- **Strings:** `const char *` for parameters. Use `String` only where an Arduino
  API requires it or a value must be owned (e.g. fields in data structs).
- **Constants:** `static constexpr` for numbers and strings. `#define` only for
  values used in `#if` (e.g. `MONOCHROME`) and the colour aliases in `constants.h`.
- **Naming:** camelCase for functions, variables and parameters; PascalCase for
  types; UPPER_SNAKE_CASE for user settings in `config.h`. Functions that render
  to the display are named `draw*`.
- **Declarations:** declare functions before use, also in `main.ino`, instead of
  relying on Arduino's generated prototypes.
- **On-screen text** is Swedish and ISO-8859-1 encoded: write å, ä, ö as `\xe5`,
  `\xe4`, `\xf6`. Convert UTF-8 API text with `utf8ToIso88591()`.
- **Memory:** avoid heap churn on the ESP32. Parse HTTP responses from the stream
  with an ArduinoJson filter, use fixed-size buffers and `snprintf`, no VLAs.
- **Watchdog:** anything that can block for more than a few seconds must call
  `esp_task_wdt_reset()`.
- **Comments:** explain why, not what. Keep them short.

## When to reconsider

If modules start holding real state, or widgets need to be composed differently
per board, a small `Widget` interface with `update()` and `draw()` is the next
step. Keep the fetch/draw split underneath it.
