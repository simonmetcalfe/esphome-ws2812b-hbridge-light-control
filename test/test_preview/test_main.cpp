/**
 * Screen previews (host): pio test -e native -f test_preview
 *
 * Draws each screen with the BDF fonts in ../fonts and writes docs/preview/<screen>.png (4x scale)
 * plus docs/preview/all.png (every screen on one sheet), for comparing with the prototypes
 * without flashing anything. The screens are reached by pressing buttons, as on the device.
 */

#include <sys/stat.h>
#include <unity.h>

#include <string>
#include <vector>

#include "../support/bdf_canvas.h"
#include "lcui_fake_light.h"
#include "lcui_ui.h"

using lcui::Button;
using lcui::FakeLight;

const char *const FAIRY_EFFECTS[] = {"Fade", "Breathe", "Smooth Random", "Twinkle"};
const char *const WALL_EFFECTS[] = {"Rainbow 360", "Rainbow Horizontal", "Aurora Blue", "Aurora Purple",
                                    "Aurora Multi", "Sunset", "Sunrise", "Lava", "Forest", "Candlelight"};
const char *const WALL_COLOURS[] = {"Warm White", "Yellow", "Orange", "Red", "Pink", "Purple",
                                    "Blue", "Cyan", "Aquamarine", "Green", "Cool White", "Neutral White"};

constexpr int SCALE = 4;
constexpr int SHEET_SCALE = 2;
constexpr int SHEET_COLUMNS = 3;
constexpr int SHEET_GAP = 8;

BdfFont small_font;
BdfFont large_font;

struct Shot {
  std::string name;
  BdfCanvas canvas{small_font, large_font};
};
std::vector<Shot *> shots;

/** A fresh UI with the balcony lights, so each screen starts from the same state. */
struct Scene {
  FakeLight fairy{"Fairy lights", FAIRY_EFFECTS, 4};
  FakeLight wall{"Wall lights", WALL_EFFECTS, 10, WALL_COLOURS, 12};
  lcui::Ui ui;
  uint32_t now = 1000;

  Scene() {
    this->ui.add_light(&this->fairy);
    this->ui.add_light(&this->wall);
  }
  Scene &press(Button b, int times = 1) {
    for (int i = 0; i < times; i++) {
      this->ui.press(b, this->now);
      this->ui.release(b, this->now + 50);
      this->now += 100;
    }
    return *this;
  }
};

void setUp() {}
void tearDown() {}

/** Greyscale image: lit pixels near-white on black. */
std::vector<uint8_t> image(const BdfCanvas &c, int scale) {
  const int w = BdfCanvas::W * scale, h = BdfCanvas::H * scale;
  std::vector<uint8_t> grey(w * h);
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) grey[y * w + x] = c.pixel(x / scale, y / scale) ? 235 : 0;
  }
  return grey;
}

void shoot(const char *name, Scene &scene) {
  auto *shot = new Shot();
  shot->name = name;
  shot->canvas.clear();
  scene.ui.render(shot->canvas);
  shots.push_back(shot);
  std::string path = std::string(LCUI_PREVIEW_DIR) + "/" + name + ".png";
  TEST_ASSERT_TRUE_MESSAGE(PngWriter::write(path.c_str(), image(shot->canvas, SCALE), BdfCanvas::W * SCALE,
                                            BdfCanvas::H * SCALE),
                           path.c_str());
}

void test_load_fonts() {
  TEST_ASSERT_TRUE(small_font.load(LCUI_FONTS_DIR "/5x7.bdf"));
  TEST_ASSERT_TRUE(large_font.load(LCUI_FONTS_DIR "/6x13B.bdf"));
  mkdir(LCUI_PREVIEW_DIR, 0755);
}

void test_home_screens() {
  Scene all;
  shoot("home_all_lights", all);

  Scene fairy;
  fairy.fairy.set_effect(0);
  fairy.press(Button::TOP_LEFT);
  shoot("home_fairy_effect", fairy);

  Scene wall;
  wall.wall.set_colour(0, 3);
  wall.press(Button::TOP_LEFT, 2);
  shoot("home_wall_colour", wall);

  Scene dual;
  dual.wall.set_colour(0, 3);
  dual.wall.set_colour(1, 6);
  dual.press(Button::TOP_LEFT, 2);
  shoot("home_wall_dual_colour", dual);

  Scene longest;
  longest.wall.set_effect(1);
  longest.press(Button::TOP_LEFT, 2);
  shoot("home_wall_long_effect", longest);
}

void test_brightness_screen() {
  Scene s;
  s.press(Button::TOP_LEFT).press(Button::BOTTOM_RIGHT);
  shoot("brightness_fairy", s);
}

void test_mode_screens() {
  Scene fairy_effect;
  fairy_effect.fairy.set_effect(0);
  fairy_effect.press(Button::TOP_LEFT).press(Button::TOP_RIGHT);
  shoot("mode_fairy_effect", fairy_effect);

  Scene fairy_static;
  fairy_static.press(Button::TOP_LEFT).press(Button::TOP_RIGHT);
  shoot("mode_fairy_static", fairy_static);

  Scene colour;
  colour.wall.set_colour(0, 3);
  colour.press(Button::TOP_LEFT, 2).press(Button::TOP_RIGHT);
  shoot("mode_wall_colour", colour);

  Scene dual;
  dual.wall.set_colour(0, 3);
  dual.wall.set_colour(1, 11);
  dual.press(Button::TOP_LEFT, 2).press(Button::TOP_RIGHT);
  shoot("mode_wall_dual_colour", dual);

  Scene effect;
  effect.wall.set_effect(2);
  effect.press(Button::TOP_LEFT, 2).press(Button::TOP_RIGHT);
  shoot("mode_wall_effect", effect);
}

/** All screens on one sheet, in the order shot. */
void test_contact_sheet() {
  const int cell_w = BdfCanvas::W * SHEET_SCALE, cell_h = BdfCanvas::H * SHEET_SCALE;
  const int rows = ((int) shots.size() + SHEET_COLUMNS - 1) / SHEET_COLUMNS;
  const int w = SHEET_COLUMNS * (cell_w + SHEET_GAP) + SHEET_GAP;
  const int h = rows * (cell_h + SHEET_GAP) + SHEET_GAP;
  std::vector<uint8_t> sheet(w * h, 96);
  for (size_t i = 0; i < shots.size(); i++) {
    auto img = image(shots[i]->canvas, SHEET_SCALE);
    const int ox = SHEET_GAP + (int) (i % SHEET_COLUMNS) * (cell_w + SHEET_GAP);
    const int oy = SHEET_GAP + (int) (i / SHEET_COLUMNS) * (cell_h + SHEET_GAP);
    for (int y = 0; y < cell_h; y++) {
      for (int x = 0; x < cell_w; x++) sheet[(oy + y) * w + ox + x] = img[y * cell_w + x];
    }
  }
  std::string path = std::string(LCUI_PREVIEW_DIR) + "/all.png";
  TEST_ASSERT_TRUE(PngWriter::write(path.c_str(), sheet, w, h));
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_load_fonts);
  RUN_TEST(test_home_screens);
  RUN_TEST(test_brightness_screen);
  RUN_TEST(test_mode_screens);
  RUN_TEST(test_contact_sheet);
  return UNITY_END();
}
