#pragma once

#include <string.h>

#include <string>
#include <vector>

#include "bdf_canvas.h"
#include "lcui_canvas.h"

/**
 * Canvas for unit tests: records draw calls instead of drawing. Text widths come from the real
 * fonts (BdfFonts), so truncation matches the device.
 */
class RecordingCanvas : public lcui::Canvas {
 public:
  explicit RecordingCanvas(const BdfFonts &fonts) : fonts_(&fonts) {}

  struct Text {
    int x;
    int baseline;
    lcui::Font font;
    std::string str;
  };
  struct Bitmap {
    int x;
    int y;
    int w;
    int h;
    const uint8_t *bmp;
  };

  void text(int x, int baseline, lcui::Font font, const char *str) override {
    texts.push_back({x, baseline, font, str});
  }
  int text_width(lcui::Font font, const char *str) override { return this->fonts_->get(font).width(str); }
  void bitmap(int x, int y, const uint8_t *bmp, int w, int h) override { bitmaps.push_back({x, y, w, h, bmp}); }
  void fill_rect(int x, int y, int w, int h, bool on) override { fill_widths.push_back(w); }
  void rect(int x, int y, int w, int h) override {}

  const Text *find_text(const char *str) const {
    for (const auto &t : texts) {
      if (t.str == str) return &t;
    }
    return nullptr;
  }
  bool has_text(const char *str) const { return this->find_text(str) != nullptr; }
  bool has_text_containing(const char *part) const {
    for (const auto &t : texts) {
      if (t.str.find(part) != std::string::npos) return true;
    }
    return false;
  }
  // Icons are compared by content: oled-lcd-icons arrays are `static`, so each source file has its own copy.
  bool has_bitmap(const uint8_t *bmp) const {
    for (const auto &b : bitmaps) {
      if (same_(b, bmp)) return true;
    }
    return false;
  }
  bool has_bitmap_at(const uint8_t *bmp, int x, int y) const {
    for (const auto &b : bitmaps) {
      if (same_(b, bmp) && b.x == x && b.y == y) return true;
    }
    return false;
  }

  std::vector<Text> texts;
  std::vector<Bitmap> bitmaps;
  std::vector<int> fill_widths;

 private:
  static bool same_(const Bitmap &b, const uint8_t *bmp) {
    return memcmp(b.bmp, bmp, (size_t) ((b.w + 7) / 8 * b.h)) == 0;
  }

  const BdfFonts *fonts_;  // pointer, so test results can be reassigned
};
