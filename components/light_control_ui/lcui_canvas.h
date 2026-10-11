#pragma once

#include <stdint.h>

namespace lcui {

/**
 * -------------------------------------------------------------------------------------------------
 * CANVAS
 *
 * The surface the UI draws one frame into. Each platform implements it on top of its own display
 * library:
 *   - ESPHome:   display::Display (`it`), fonts from BDF files          (light_control_ui.cpp)
 *   - Arduino:   U8g2 full-buffer mode, the same fonts built into U8g2  (src/main.cpp)
 *   - Host:      a 128x64 bitmap written to PNG for previews            (test/test_preview)
 *
 * The caller clears the frame before Ui::render() and sends it to the display afterwards, so a
 * frame costs one transfer however many draw calls it makes.
 *
 * Text is positioned by its baseline: every library agrees where the baseline is, but not where
 * the "top" of a font is. Widths are the sum of the glyph advances.
 * -------------------------------------------------------------------------------------------------
 */

enum class Font : uint8_t {
  SMALL,  // labels: X11 5x7 (6 px capitals, 5 px advance)
  LARGE,  // titles: X11 6x13B (9 px capitals, 6 px advance)
};

class Canvas {
 public:
  virtual ~Canvas() = default;

  /** Draw str with its left edge at x and its baseline on row `baseline`. */
  virtual void text(int x, int baseline, Font font, const char *str) = 0;

  /** Width of str in pixels: the sum of its glyph advances. */
  virtual int text_width(Font font, const char *str) = 0;

  /**
   * Draw a 1-bit bitmap: row-major, MSB = leftmost pixel, each row padded to whole bytes (the
   * oled-lcd-icons format). Only the 1-bits are drawn.
   */
  virtual void bitmap(int x, int y, const uint8_t *bmp, int w, int h) = 0;

  /** Fill a rectangle, lit (on = true) or dark. */
  virtual void fill_rect(int x, int y, int w, int h, bool on) = 0;

  /** Draw a 1 px lit rectangle outline. */
  virtual void rect(int x, int y, int w, int h) = 0;
};

}  // namespace lcui
