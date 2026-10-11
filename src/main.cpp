/**
 * -------------------------------------------------------------------------------------------------
 * BENCH FIRMWARE
 *
 * The light control UI on the real OLED and buttons, with fake lights in RAM (no Wi-Fi, no ESPHome),
 * for quick UI iteration. The lights mirror balconylights.yaml: Fairy lights (H-bridge, 4 effects)
 * and Wall lights (WS2812B, 10 effects, 12 colours). Every change is logged to the USB serial port.
 *
 * Flash over the ESP32-S3's native USB port: GPIO43/44 (the UART0 pins) are buttons B1/B2.
 * -------------------------------------------------------------------------------------------------
 */

#include <Arduino.h>
#include <U8g2lib.h>

#include "lcui_fake_light.h"
#include "lcui_ui.h"

// Pins as in balconylights.yaml
constexpr uint8_t PIN_SDA = 5;
constexpr uint8_t PIN_SCL = 6;
constexpr uint8_t BUTTON_PINS[4] = {43, 44, 7, 8};  // B1 top-left, B2 bottom-left, B3 bottom-right, B4 top-right

constexpr uint32_t DEBOUNCE_MS = 25;
constexpr uint32_t REFRESH_MS = 1000;  // redraw at least this often, like ESPHome's display update_interval

// SH1106 128x64, full frame buffer: one I2C transfer per frame
U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE, PIN_SCL, PIN_SDA);

/**
 * lcui::Canvas on U8g2, with the U8g2 builds of the fonts in ../fonts (same glyphs as ESPHome).
 */
class U8g2Canvas : public lcui::Canvas {
 public:
  void text(int x, int baseline, lcui::Font font, const char *str) override {
    oled.setFont(font_(font));
    oled.drawStr(x, baseline, str);
  }

  // Sum of glyph advances, as ESPHome measures (getStrWidth() uses the last glyph's ink width)
  int text_width(lcui::Font font, const char *str) override {
    oled.setFont(font_(font));
    int width = 0;
    for (const char *p = str; *p != '\0'; p++) width += u8g2_GetGlyphWidth(oled.getU8g2(), (uint8_t) *p);
    return width;
  }

  void bitmap(int x, int y, const uint8_t *bmp, int w, int h) override {
    oled.drawBitmap(x, y, (w + 7) / 8, h, bmp);
  }

  void fill_rect(int x, int y, int w, int h, bool on) override {
    oled.setDrawColor(on ? 1 : 0);
    oled.drawBox(x, y, w, h);
    oled.setDrawColor(1);
  }

  void rect(int x, int y, int w, int h) override { oled.drawFrame(x, y, w, h); }

 private:
  static const uint8_t *font_(lcui::Font font) {
    return font == lcui::Font::LARGE ? u8g2_font_6x13B_tr : u8g2_font_5x7_tr;
  }
};

const char *const FAIRY_EFFECTS[] = {"Fade", "Breathe", "Smooth Random", "Twinkle"};
const char *const WALL_EFFECTS[] = {"Rainbow 360", "Rainbow Horizontal", "Aurora Blue", "Aurora Purple",
                                    "Aurora Multi", "Sunset", "Sunrise", "Lava", "Forest", "Candlelight"};
const char *const WALL_COLOURS[] = {"Warm White", "Yellow", "Orange", "Red", "Pink", "Purple",
                                    "Blue", "Cyan", "Aquamarine", "Green", "Cool White", "Neutral White"};

lcui::FakeLight fairy_lights("Fairy lights", FAIRY_EFFECTS, 4);
lcui::FakeLight wall_lights("Wall lights", WALL_EFFECTS, 10, WALL_COLOURS, 12);

lcui::Ui ui;
U8g2Canvas canvas;

struct ButtonState {
  bool pressed = false;
  bool reading = false;
  uint32_t changed_ms = 0;
};
ButtonState buttons[4];
uint32_t last_draw_ms = 0;

const char *mode_name(lcui::Mode mode) {
  switch (mode) {
    case lcui::Mode::STATIC:
      return "STATIC";
    case lcui::Mode::EFFECT:
      return "EFFECT";
    case lcui::Mode::COLOUR:
      return "COLOUR";
    case lcui::Mode::DUAL_COLOUR:
      return "DUAL_COLOUR";
  }
  return "?";
}

void log_change(const lcui::FakeLight &light, const char *change) {
  Serial.printf("%s: %s -> %s, %u%%, %s", light.name(), change, light.is_on() ? "on" : "off",
                light.brightness(), mode_name(light.mode()));
  if (light.mode() == lcui::Mode::EFFECT) Serial.printf(", effect %s", light.effect_name(light.effect_index()));
  if (light.has_colour() && light.mode() != lcui::Mode::EFFECT) {
    Serial.printf(", colours %s / %s", light.colour_name(light.colour_index(0)),
                  light.colour_name(light.colour_index(1)));
  }
  Serial.println();
}

void draw() {
  oled.clearBuffer();
  ui.render(canvas);
  oled.sendBuffer();
  last_draw_ms = millis();
}

void setup() {
  Serial.begin(115200);
  for (uint8_t pin : BUTTON_PINS) pinMode(pin, INPUT_PULLUP);

  oled.setBusClock(400000);
  oled.begin();
  oled.setFontMode(1);    // transparent text, as ESPHome draws it
  oled.setBitmapMode(1);  // icons: draw only the 1-bits

  lcui::FakeLight::log = log_change;
  ui.add_light(&fairy_lights);
  ui.add_light(&wall_lights);
  draw();
  Serial.println("Light control UI bench ready");
}

void loop() {
  uint32_t now = millis();
  bool redraw = false;

  for (uint8_t i = 0; i < 4; i++) {
    ButtonState &b = buttons[i];
    bool reading = digitalRead(BUTTON_PINS[i]) == LOW;  // active low with pull-up
    if (reading != b.reading) {
      b.reading = reading;
      b.changed_ms = now;
    }
    if (b.reading != b.pressed && now - b.changed_ms >= DEBOUNCE_MS) {
      b.pressed = b.reading;
      auto button = (lcui::Button) i;
      redraw |= b.pressed ? ui.press(button, now) : ui.release(button, now);
    }
  }

  redraw |= ui.tick(now);
  if (redraw || now - last_draw_ms >= REFRESH_MS) draw();
}
