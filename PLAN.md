# Pebble "Server Time" Watchface — Plan

Unix-epoch watchface for the **Emery** platform (Pebble Time 2, 200×228 color).

## Decisions

- **Language:** C (single `src/c/main.c`, no package installs)
- **Target:** `["emery"]` only
- **Update rate:** every second (`tick_timer_service_subscribe(SECOND_UNIT, ...)`),
  so the epoch counter ticks live. Accepted battery cost per user choice.

## Screen layout (200×228)

```
┌──────────────────────────┐
│   12:34   (large, top)   │  local time
│   Thu, Nov 20            │  local date
│                          │
│   1732123456             │  Unix epoch (large, middle)
│                          │
│   12:34                  │  UTC time
│   Thu, Nov 20 (UTC)      │  UTC date
└──────────────────────────┘
```

- Top: local time in a large system font (Roboto-Bold ~42), local date under it.
- Middle: epoch seconds via `snprintf("%ld", time(NULL))`, highlight color
  (cyan) on black, with a small "Unix time" caption.
- Bottom: UTC time and date, anchored so the bottom edge stays ≤ 223 px.
- All elements checked against the 200×228 bounds — no cropping.

## Implementation notes

- One `time(NULL)` call per tick; `localtime_r()` for the top block,
  `gmtime_r()` for the bottom block; one handler updates all five TextLayers.
- Static char buffers (no per-tick allocation); layers destroyed in `window_unload`.
- White-on-black base palette; system fonts only.

## Files

| File | Purpose |
|---|---|
| `package.json` | watchface, `targetPlatforms: ["emery"]`, fresh UUID |
| `wscript` | stock build script |
| `src/c/main.c` | whole watchface (~150 lines) |
| `resources/` | empty dir (build requirement) |

## Verify loop

1. `pebble build` → PBW
2. Background log capture → `pebble install --emulator emery` →
   `pebble screenshot --no-open` → visual check (cropping / position / color)
3. Iterate until clean; check logs for errors
4. Generate app icons from the screenshot; skip preview GIF (per-second epoch
   shows no meaningful change in a static capture)

## Explicitly out of scope

Gabbro/round support, Clay settings, weather, animation — none requested.
