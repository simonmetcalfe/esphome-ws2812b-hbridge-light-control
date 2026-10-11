# Fonts

| File | Use | Capitals | Advance | Licence |
|---|---|---|---|---|
| `5x7.bdf` | `SMALL` (labels) | 6 px | 5 px | Public domain (X11 misc-fixed) |
| `6x13B.bdf` | `LARGE` (titles) | 9 px | 6 px | Public domain (X11 misc-fixed) |

Copied unchanged from U8g2's source fonts: <https://github.com/olikraus/u8g2/tree/master/tools/font/bdf>.

- **ESPHome** loads these files with the `font:` component (bitmap fonts are drawn at their native size).
- **The UI test firmware** uses U8g2's built-in `u8g2_font_5x7_tr` and `u8g2_font_6x13B_tr`, which U8g2 generates from the same files, so the glyphs are identical.
- **The host preview** (`test/test_preview`) reads these files directly.

To change a size, change it in all three places: the `font:` entries in the ESPHome YAML, `U8g2Canvas` in `src/main.cpp`, and the layout constants at the top of `components/light_control_ui/lcui_ui.cpp` (positions are tuned to these capital heights).
