# Fonts

| File | Use | Capitals | Advance | Licence |
|---|---|---|---|---|
| `helvR08.bdf` | `SMALL` (labels) | 8 px | Proportional | Adobe / DEC permissive licence (in the file) |
| `helvB10.bdf` | `LARGE` (titles) | 11 px | Proportional | Adobe / DEC permissive licence (in the file) |

X11 Adobe Helvetica (100 dpi), copied unchanged from U8g2's source fonts: <https://github.com/olikraus/u8g2/tree/master/tools/font/bdf>. The licence allows copying and redistribution as long as the copyright and permission notices stay with the files; they're in each file's header comments.

- **ESPHome** loads these files with the `font:` component (bitmap fonts are drawn at their native size).
- **The UI test firmware** uses U8g2's built-in `u8g2_font_helvR08_tr` and `u8g2_font_helvB10_tr`, which U8g2 generates from the same files, so the glyphs are identical.
- **The host tests and previews** (`test/`) read these files directly.

To change a font, change it in all four places: the `font:` entries in the ESPHome YAML, `U8g2Canvas` in `src/main.cpp`, `BdfFonts` in `test/support/bdf_canvas.h`, and the layout constants at the top of `components/light_control_ui/lcui_ui.cpp` (positions are tuned to these capital heights).
