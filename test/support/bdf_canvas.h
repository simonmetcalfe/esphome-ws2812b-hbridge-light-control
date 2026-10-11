#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "lcui_canvas.h"

/**
 * -------------------------------------------------------------------------------------------------
 * BDF CANVAS + PNG WRITER (host previews)
 *
 * Draws into a 128x64 frame using the BDF files in ../fonts, so previews show the same glyphs as
 * ESPHome and U8g2. Glyphs are placed by their BDF bounding box: a glyph with no descent sits with
 * its bottom row on baseline - 1.
 * -------------------------------------------------------------------------------------------------
 */

class BdfFont {
 public:
  struct Glyph {
    int advance = 0;
    int w = 0, h = 0, x_off = 0, y_off = 0;
    std::vector<uint8_t> rows;  // h rows of (w + 7) / 8 bytes, MSB = leftmost pixel
  };

  bool load(const char *path) {
    FILE *f = fopen(path, "r");
    if (f == nullptr) return false;
    char line[256];
    int encoding = -1;
    Glyph g;
    bool in_bitmap = false;
    while (fgets(line, sizeof(line), f) != nullptr) {
      if (strncmp(line, "ENCODING ", 9) == 0) {
        encoding = atoi(line + 9);
        g = Glyph();
      } else if (strncmp(line, "DWIDTH ", 7) == 0) {
        g.advance = atoi(line + 7);
      } else if (strncmp(line, "BBX ", 4) == 0) {
        sscanf(line + 4, "%d %d %d %d", &g.w, &g.h, &g.x_off, &g.y_off);
      } else if (strncmp(line, "BITMAP", 6) == 0) {
        in_bitmap = true;
      } else if (strncmp(line, "ENDCHAR", 7) == 0) {
        in_bitmap = false;
        if (encoding >= 0 && encoding < 128) this->glyphs_[encoding] = g;
      } else if (in_bitmap) {
        const int bytes = (g.w + 7) / 8;
        for (int i = 0; i < bytes; i++) {
          char hex[3] = {line[i * 2], line[i * 2 + 1], '\0'};
          g.rows.push_back((uint8_t) strtol(hex, nullptr, 16));
        }
      }
    }
    fclose(f);
    return true;
  }

  const Glyph &glyph(char c) const { return this->glyphs_[(uint8_t) c & 0x7F]; }

  /** Sum of the glyph advances, as ESPHome and U8g2 measure. */
  int width(const char *str) const {
    int w = 0;
    for (const char *p = str; *p != '\0'; p++) w += this->glyph(*p).advance;
    return w;
  }

 private:
  Glyph glyphs_[128];
};

/** The UI's two fonts: the files in ../fonts that ESPHome loads. */
struct BdfFonts {
  BdfFont small;
  BdfFont large;

  bool load() {
    return this->small.load(LCUI_FONTS_DIR "/helvR08.bdf") && this->large.load(LCUI_FONTS_DIR "/helvB10.bdf");
  }
  const BdfFont &get(lcui::Font font) const { return font == lcui::Font::LARGE ? this->large : this->small; }
};

class BdfCanvas : public lcui::Canvas {
 public:
  static constexpr int W = 128;
  static constexpr int H = 64;

  explicit BdfCanvas(const BdfFonts &fonts) : fonts_(fonts) {}

  void clear() { memset(this->frame_, 0, sizeof(this->frame_)); }

  void text(int x, int baseline, lcui::Font font, const char *str) override {
    const BdfFont &f = this->fonts_.get(font);
    for (const char *p = str; *p != '\0'; p++) {
      const BdfFont::Glyph &g = f.glyph(*p);
      const int bytes = (g.w + 7) / 8;
      const int top = baseline - g.y_off - g.h;
      for (int r = 0; r < g.h; r++) {
        for (int col = 0; col < g.w; col++) {
          if (g.rows[r * bytes + col / 8] & (0x80 >> (col % 8))) this->set_(x + g.x_off + col, top + r, true);
        }
      }
      x += g.advance;
    }
  }

  int text_width(lcui::Font font, const char *str) override { return this->fonts_.get(font).width(str); }

  void bitmap(int x, int y, const uint8_t *bmp, int w, int h) override {
    const int bytes = (w + 7) / 8;
    for (int r = 0; r < h; r++) {
      for (int col = 0; col < w; col++) {
        if (bmp[r * bytes + col / 8] & (0x80 >> (col % 8))) this->set_(x + col, y + r, true);
      }
    }
  }

  void fill_rect(int x, int y, int w, int h, bool on) override {
    for (int r = y; r < y + h; r++) {
      for (int col = x; col < x + w; col++) this->set_(col, r, on);
    }
  }

  void rect(int x, int y, int w, int h) override {
    for (int col = x; col < x + w; col++) {
      this->set_(col, y, true);
      this->set_(col, y + h - 1, true);
    }
    for (int r = y; r < y + h; r++) {
      this->set_(x, r, true);
      this->set_(x + w - 1, r, true);
    }
  }

  bool pixel(int x, int y) const { return x >= 0 && x < W && y >= 0 && y < H && this->frame_[y][x]; }

 private:
  void set_(int x, int y, bool on) {
    if (x >= 0 && x < W && y >= 0 && y < H) this->frame_[y][x] = on;
  }

  const BdfFonts &fonts_;
  bool frame_[H][W]{};
};

/** 8-bit greyscale PNG, uncompressed (stored deflate blocks); enough for small previews. */
class PngWriter {
 public:
  static bool write(const char *path, const std::vector<uint8_t> &grey, int w, int h) {
    std::vector<uint8_t> raw;
    for (int y = 0; y < h; y++) {
      raw.push_back(0);  // filter: none
      raw.insert(raw.end(), grey.begin() + y * w, grey.begin() + (y + 1) * w);
    }
    std::vector<uint8_t> z = {0x78, 0x01};
    for (size_t pos = 0; pos < raw.size();) {
      size_t n = raw.size() - pos < 65535 ? raw.size() - pos : 65535;
      bool last = pos + n == raw.size();
      z.push_back(last ? 1 : 0);
      z.push_back(n & 0xFF);
      z.push_back(n >> 8);
      z.push_back(~n & 0xFF);
      z.push_back((~n >> 8) & 0xFF);
      z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + n);
      pos += n;
    }
    uint32_t a = 1, b = 0;
    for (uint8_t v : raw) {
      a = (a + v) % 65521;
      b = (b + a) % 65521;
    }
    put32_(z, (b << 16) | a);

    std::vector<uint8_t> png = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    std::vector<uint8_t> ihdr;
    put32_(ihdr, w);
    put32_(ihdr, h);
    ihdr.insert(ihdr.end(), {8, 0, 0, 0, 0});  // 8-bit greyscale
    chunk_(png, "IHDR", ihdr);
    chunk_(png, "IDAT", z);
    chunk_(png, "IEND", {});

    FILE *f = fopen(path, "wb");
    if (f == nullptr) return false;
    fwrite(png.data(), 1, png.size(), f);
    fclose(f);
    return true;
  }

 private:
  static void put32_(std::vector<uint8_t> &v, uint32_t x) {
    for (int s = 24; s >= 0; s -= 8) v.push_back((x >> s) & 0xFF);
  }
  static void chunk_(std::vector<uint8_t> &png, const char *type, const std::vector<uint8_t> &data) {
    put32_(png, (uint32_t) data.size());
    size_t start = png.size();
    png.insert(png.end(), type, type + 4);
    png.insert(png.end(), data.begin(), data.end());
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = start; i < png.size(); i++) {
      crc ^= png[i];
      for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320 & (0 - (crc & 1)));
    }
    put32_(png, crc ^ 0xFFFFFFFF);
  }
};
