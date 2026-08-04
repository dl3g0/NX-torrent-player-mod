# Vendored borealis

This directory is a copy of borealis, checked into this repository rather than
pulled as a submodule, so a clone builds without extra steps.

| | |
|---|---|
| Upstream | https://github.com/xfangfang/borealis.git |
| Commit | `5f08b286f3df737f3321d2247a6fe633fcead03c` |
| Date | 2026-04-25 |

Everything here is upstream's, under its own license (see `LICENSE`), **except**
the local patch below.

## Local patches

All are marked with a `LOCAL PATCH` comment in the source.

### 1. `library/include/borealis/core/application.hpp` — `setInputType` public

Moved `Application::setInputType` from the private section to public. The player
forces `InputType::GAMEPAD` each frame so a stray screen touch does not cost the
first A press (borealis otherwise consumes the first gamepad button after a touch
just to switch input type back, so pause/resume took two presses).

### 2. `library/lib/platforms/switch/switch_wrapper.c` — smaller TCP buffers

`userAppInit()` sets smaller initial TCP socket buffers.

Every TCP socket draws its buffers from one fixed pool, sized
`sb_efficiency * (tcp_*_buf_max_size + udp_*)`, while each socket consumes
`tcp_*_buf_size` out of it. Stock defaults (32 KB tx / 64 KB rx initial) assume
a few large streams; a torrent engine wants the opposite — many small sockets (a
peer sends 16 KB blocks, we send 17-byte requests). With the defaults the pool
ran dry at ~20 sockets and `socket()` returned ENOBUFS ("No buffer space
available"), capping concurrent peers regardless of swarm size.

The max sizes are left at stock, so the memory footprint matches unpatched
borealis: only the per-socket initial buffers are cut. Growing the pool instead
starved the nvtegra video decoder, which then fell back to software H.264.

The patch is marked with a `LOCAL PATCH` comment in the source.

### 3. `library/lib/views/hint.cpp` — hint icon font size

Lowered the footer hint's `icon` label `fontSize` from the upstream `25.5` to
`21.5`, matching the `hint` text label (left at `21.5`). The Stremio view hint
(main.cpp) packs a second controller-button glyph into the hint *text* so both
the L and R glyphs sit in one chip; the text glyph must be the same size as the
auto icon glyph next to it. Lowering the icon (rather than raising the text)
keeps every footer label at its original size -- only the button glyphs shrink
slightly, to sit level with their labels.

### 4. `library/lib/platforms/switch/switch_font.cpp` — Space Grotesk as the primary font

`loadFonts()` loads the app's bundled `romfs:/SpaceGrotesk-Medium.ttf` as the
PRIMARY font. `getDefaultFont()` is `FONT_CHINESE_SIMPLIFIED` on Switch, so Space
Grotesk is loaded into that slot and every system font (Standard, Simplified
Chinese, plus the ext/traditional/Korean/symbol fonts and Material icons) is added
as a fallback for the glyphs it lacks. So all Latin UI text is Space Grotesk while
CJK/kana/symbols/icons still render from the system/resource fonts. It must be the
primary, not merely `FONT_REGULAR`: the system Chinese font that used to be the
primary carries Latin glyphs, so a `FONT_REGULAR` fallback was never reached for
Latin. Falls back to the stock system-Chinese-primary setup if the romfs file is
missing. The `.ttf` is bundled by the top-level CMakeLists.

The patch is marked with a `LOCAL PATCH` comment in the source.

### 5. `library/lib/views/label.cpp` — three-dot ellipsis

`ELLIPSIS` changed from `"…"` (the single horizontal-ellipsis glyph) to
`"..."`. In our font the U+2026 glyph rendered vertically centred (mid-line) on
truncated labels; three periods sit on the baseline like the rest of the text.

The patch is marked with a `LOCAL PATCH` comment in the source.

### 6. `library/lib/core/view.cpp` + `library/lib/core/animation.cpp` — focus highlight animation

Three changes, all so the Horizon-style shimmer on the focus ring is actually
visible. The first two are in `View::drawHighlight`:

- **`pulsationColor` mixes `color1` with `color2`.** Upstream mixes `color1`
  with `color1`, which is the identity — the animated `color` term cancels out
  and the base stroke never pulses. All that was left of the effect were the two
  radial gradients below.
- **The gradient radii scale with the view.** Upstream uses a fixed
  `strokeWidth * 10` / `strokeWidth * 40` (50 / 200 px), which is about right on
  a full-width list row but swamps anything small: a poster card or a section
  button sits entirely inside one blob, so the ring lights up uniformly and the
  travel cannot be seen. They are now `min(original, perimeter * 0.08)`, so the
  lit arc is the same fraction of the border at any size and a long row keeps
  exactly the radius it had.

...and the third is in `updateHighlightAnimation` (`animation.cpp`):

- **The colour pulse runs at the orbit's rate.** Upstream drives it with
  `/ HIGHLIGHT_SPEED * 2.0` against the gradients' `/ HIGHLIGHT_SPEED / 3.0` —
  six times faster, a ~0.4 s cycle, which strobes. It was invisible upstream
  because of the identity mix above, so nobody hit it. Now `/ 3.0` like the
  gradients: a ~2.4 s breath.

`app/theme.cpp` (`applyAccent`) is the other half of this: it sets `color1` to
the accent and `color2` to a lighter tint of it. Setting both to the same colour
— which it used to do, to kill borealis' default cyan — makes the gradients
invisible however they are sized, since they then match what they travel over.

Both patches are marked with a `LOCAL PATCH` comment in the source.

## Updating

Re-cloning upstream **drops the patch** — re-apply it, and check this file's
commit hash is updated. To see the patch as a diff against upstream:

```sh
git clone https://github.com/xfangfang/borealis.git /tmp/borealis-upstream
cd /tmp/borealis-upstream && git checkout 5f08b286f3df737f3321d2247a6fe633fcead03c
diff -u /tmp/borealis-upstream/library/lib/platforms/switch/switch_wrapper.c \
        library/lib/platforms/switch/switch_wrapper.c
```
