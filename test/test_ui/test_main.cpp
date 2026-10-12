/**
 * UI behaviour tests (host): pio test -e native -f test_ui
 *
 * The lights mirror the balcony: Fairy (H-bridge, 4 effects) and Wall RGB (WS2812B, 10 effects,
 * 12 colours). Text is measured with the real fonts, so truncation matches the device.
 */

#include <unity.h>

#include <algorithm>

#include "../support/recording_canvas.h"
#include "lcui_fake_light.h"
#include "lcui_icons.h"
#include "lcui_ui.h"

using lcui::Button;
using lcui::FakeLight;
using lcui::Mode;
using lcui::Screen;
namespace ic = oli::icons_12x12;

const char *const FAIRY_EFFECTS[] = {"Fade", "Breathe", "Smooth Random", "Twinkle"};
const char *const WALL_EFFECTS[] = {"Rainbow 360", "Rainbow 180", "Aurora Blue", "Aurora Purple",
                                    "Aurora Multi", "Sunset", "Sunrise", "Lava", "Forest", "Candlelight"};
const char *const WALL_COLOURS[] = {"Warm W.", "Yellow", "Orange", "Red", "Pink", "Purple",
                                    "Blue", "Cyan", "Aqua", "Green", "Cool W.", "Neutral W."};

// Icon positions (lcui_ui.cpp)
constexpr int B4_ICON_X = 115, B4_ICON_Y = 1;
constexpr int CENTRE_ICON_X = 58, CENTRE_ICON_Y = 22;

BdfFonts fonts;
FakeLight *fairy;
FakeLight *wall;
lcui::Ui *ui;
uint32_t now;

void setUp() {
  fairy = new FakeLight("Fairy", FAIRY_EFFECTS, 4);
  wall = new FakeLight("Wall RGB", WALL_EFFECTS, 10, WALL_COLOURS, 12);
  ui = new lcui::Ui();
  ui->add_light(fairy);
  ui->add_light(wall);
  now = 1000;
}

void tearDown() {
  delete ui;
  delete wall;
  delete fairy;
}

bool press(Button b) {
  bool changed = ui->press(b, now);
  ui->release(b, now + 50);
  now += 100;
  return changed;
}

RecordingCanvas render() {
  RecordingCanvas c(fonts);
  ui->render(c);
  return c;
}

void select_wall() {
  press(Button::TOP_LEFT);
  press(Button::TOP_LEFT);
}

void test_load_fonts() { TEST_ASSERT_TRUE(fonts.load()); }

/**
 * HOME
 */

void test_home_starts_on_all_lights() {
  auto c = render();
  TEST_ASSERT_TRUE(c.has_text("All lights"));
  TEST_ASSERT_TRUE(c.has_text("ON"));
  TEST_ASSERT_TRUE(c.has_text("60%"));
  TEST_ASSERT_TRUE(c.has_bitmap(ic::swapL));
  TEST_ASSERT_TRUE(c.has_bitmap(ic::brightness));
  // No B4 on All lights
  TEST_ASSERT_FALSE(c.has_bitmap(ic::colour));
  TEST_ASSERT_FALSE(c.has_bitmap(ic::unblocked));
}

void test_b1_cycles_lights_and_wraps() {
  press(Button::TOP_LEFT);
  TEST_ASSERT_TRUE(render().has_text("Fairy"));
  press(Button::TOP_LEFT);
  TEST_ASSERT_TRUE(render().has_text("Wall RGB"));
  press(Button::TOP_LEFT);
  TEST_ASSERT_TRUE(render().has_text("All lights"));
}

void test_single_light_has_no_all_lights_entry() {
  lcui::Ui single;
  single.add_light(fairy);
  TEST_ASSERT_EQUAL(1, single.entry_count());
  RecordingCanvas c(fonts);
  single.render(c);
  TEST_ASSERT_TRUE(c.has_text("Fairy"));
}

void test_power_all_lights_turns_all_off_then_on() {
  press(Button::BOTTOM_LEFT);
  TEST_ASSERT_FALSE(fairy->is_on());
  TEST_ASSERT_FALSE(wall->is_on());
  TEST_ASSERT_TRUE(render().has_text("OFF"));
  press(Button::BOTTOM_LEFT);
  TEST_ASSERT_TRUE(fairy->is_on());
  TEST_ASSERT_TRUE(wall->is_on());
}

void test_power_all_lights_any_on_counts_as_on() {
  fairy->set_on(false);
  TEST_ASSERT_TRUE(render().has_text("ON"));
  press(Button::BOTTOM_LEFT);
  TEST_ASSERT_FALSE(fairy->is_on());
  TEST_ASSERT_FALSE(wall->is_on());
}

void test_power_single_light() {
  press(Button::TOP_LEFT);  // Fairy
  press(Button::BOTTOM_LEFT);
  TEST_ASSERT_FALSE(fairy->is_on());
  TEST_ASSERT_TRUE(wall->is_on());
}

void test_b4_unused_on_all_lights() {
  TEST_ASSERT_FALSE(press(Button::TOP_RIGHT));
  TEST_ASSERT_EQUAL(Screen::HOME, ui->screen());
}

void test_light_off_hides_and_disables_b3_b4() {
  select_wall();
  press(Button::BOTTOM_LEFT);  // off
  auto c = render();
  TEST_ASSERT_TRUE(c.has_text("OFF"));
  TEST_ASSERT_TRUE(c.has_bitmap(ic::power));
  TEST_ASSERT_FALSE(c.has_bitmap(ic::brightness));
  TEST_ASSERT_FALSE(c.has_text("60%"));
  TEST_ASSERT_FALSE(c.has_bitmap(ic::colour));
  TEST_ASSERT_FALSE(c.has_text("1 COLOUR"));
  TEST_ASSERT_EQUAL(2, (int) c.bitmaps.size());  // B1 and B2 only
  TEST_ASSERT_FALSE(press(Button::BOTTOM_RIGHT));
  TEST_ASSERT_FALSE(press(Button::TOP_RIGHT));
  TEST_ASSERT_EQUAL(Screen::HOME, ui->screen());
}

void test_all_lights_off_hides_and_disables_b3() {
  press(Button::BOTTOM_LEFT);  // all off
  auto c = render();
  TEST_ASSERT_FALSE(c.has_bitmap(ic::brightness));
  TEST_ASSERT_FALSE(c.has_text_containing("%"));
  TEST_ASSERT_FALSE(press(Button::BOTTOM_RIGHT));
  TEST_ASSERT_EQUAL(Screen::HOME, ui->screen());
}

/** B4 is the mode icon alone: the title, ON and the brightness are the only text. */
void test_home_b4_shows_mode_icon_only() {
  select_wall();
  auto c = render();
  TEST_ASSERT_TRUE(c.has_bitmap_at(ic::colour, B4_ICON_X, B4_ICON_Y));
  TEST_ASSERT_EQUAL(3, (int) c.texts.size());

  wall->set_colour(1, 3);  // colour 2 -> two-colour mode
  c = render();
  TEST_ASSERT_TRUE(c.has_bitmap_at(ic::contrast, B4_ICON_X, B4_ICON_Y));
  TEST_ASSERT_EQUAL(3, (int) c.texts.size());

  wall->set_effect(2);
  c = render();
  TEST_ASSERT_TRUE(c.has_bitmap_at(ic::pattern, B4_ICON_X, B4_ICON_Y));
  TEST_ASSERT_EQUAL(3, (int) c.texts.size());

  press(Button::TOP_LEFT);  // All lights
  press(Button::TOP_LEFT);  // Fairy (STATIC)
  c = render();
  TEST_ASSERT_TRUE(c.has_bitmap_at(ic::unblocked, B4_ICON_X, B4_ICON_Y));
  TEST_ASSERT_EQUAL(3, (int) c.texts.size());
}

/** No "3 / 12"-style positions on any screen. */
void test_no_positions_shown() {
  const Button route[] = {Button::TOP_LEFT,  Button::TOP_RIGHT, Button::TOP_RIGHT, Button::TOP_LEFT,
                          Button::TOP_LEFT,  Button::TOP_RIGHT, Button::TOP_RIGHT, Button::TOP_RIGHT,
                          Button::TOP_RIGHT, Button::TOP_LEFT,  Button::BOTTOM_RIGHT};
  TEST_ASSERT_FALSE(render().has_text_containing("/"));
  for (Button b : route) {
    press(b);
    TEST_ASSERT_FALSE(render().has_text_containing("/"));
  }
}

/**
 * BRIGHTNESS
 */

void test_brightness_all_lights_starts_from_brightest_and_syncs() {
  fairy->set_brightness(30);
  wall->set_brightness(73);
  TEST_ASSERT_TRUE(render().has_text("73%"));
  press(Button::BOTTOM_RIGHT);
  TEST_ASSERT_EQUAL(Screen::BRIGHTNESS, ui->screen());
  press(Button::BOTTOM_RIGHT);  // up
  TEST_ASSERT_EQUAL(75, fairy->brightness());
  TEST_ASSERT_EQUAL(75, wall->brightness());
  press(Button::BOTTOM_LEFT);  // down
  TEST_ASSERT_EQUAL(70, fairy->brightness());
  TEST_ASSERT_EQUAL(70, wall->brightness());
}

void test_brightness_steps_and_limits() {
  press(Button::TOP_LEFT);  // Fairy
  press(Button::BOTTOM_RIGHT);
  fairy->set_brightness(63);
  press(Button::BOTTOM_RIGHT);
  TEST_ASSERT_EQUAL(65, fairy->brightness());
  fairy->set_brightness(63);
  press(Button::BOTTOM_LEFT);
  TEST_ASSERT_EQUAL(60, fairy->brightness());
  fairy->set_brightness(100);
  press(Button::BOTTOM_RIGHT);
  TEST_ASSERT_EQUAL(100, fairy->brightness());
  wall->set_brightness(42);
  TEST_ASSERT_EQUAL(100, fairy->brightness());  // only the selected light changes
}

void test_brightness_down_to_zero_turns_off() {
  press(Button::TOP_LEFT);  // Fairy
  press(Button::BOTTOM_RIGHT);
  fairy->set_brightness(5);
  press(Button::BOTTOM_LEFT);
  TEST_ASSERT_FALSE(fairy->is_on());
  TEST_ASSERT_EQUAL(5, fairy->brightness());  // kept for the next turn-on
  TEST_ASSERT_TRUE(wall->is_on());
  TEST_ASSERT_EQUAL(0, (int) render().fill_widths.size());  // empty bar
  press(Button::BOTTOM_LEFT);
  TEST_ASSERT_FALSE(fairy->is_on());
  press(Button::BOTTOM_RIGHT);  // up from 0 turns it on
  TEST_ASSERT_TRUE(fairy->is_on());
  TEST_ASSERT_EQUAL(5, fairy->brightness());
  fairy->set_brightness(3);  // below the first step (set elsewhere): down turns it off
  press(Button::BOTTOM_LEFT);
  TEST_ASSERT_FALSE(fairy->is_on());
}

void test_brightness_all_lights_changes_only_lights_that_are_on() {
  fairy->set_on(false);
  wall->set_brightness(20);
  TEST_ASSERT_TRUE(render().has_text("20%"));  // Fairy's 60 % doesn't count while it's off
  press(Button::BOTTOM_RIGHT);
  press(Button::BOTTOM_RIGHT);  // up
  TEST_ASSERT_EQUAL(25, wall->brightness());
  TEST_ASSERT_FALSE(fairy->is_on());
  TEST_ASSERT_EQUAL(60, fairy->brightness());
  for (int i = 0; i < 5; i++) press(Button::BOTTOM_LEFT);  // 20, 15, 10, 5, 0
  TEST_ASSERT_FALSE(wall->is_on());
  TEST_ASSERT_FALSE(fairy->is_on());
  press(Button::BOTTOM_RIGHT);  // none on: up turns them all on
  TEST_ASSERT_TRUE(fairy->is_on());
  TEST_ASSERT_TRUE(wall->is_on());
  TEST_ASSERT_EQUAL(5, fairy->brightness());
  TEST_ASSERT_EQUAL(5, wall->brightness());
}

void test_brightness_has_20_steps() {
  press(Button::TOP_LEFT);  // Fairy
  press(Button::BOTTOM_RIGHT);
  fairy->set_on(false);
  for (int step = 1; step <= 20; step++) {
    press(Button::BOTTOM_RIGHT);
    TEST_ASSERT_EQUAL(step * 5, fairy->brightness());
  }
  press(Button::BOTTOM_RIGHT);
  TEST_ASSERT_EQUAL(100, fairy->brightness());
}

void test_brightness_bar_fill() {
  press(Button::BOTTOM_RIGHT);
  auto c = render();
  TEST_ASSERT_EQUAL(1, (int) c.fill_widths.size());
  TEST_ASSERT_EQUAL(54, c.fill_widths[0]);  // 60% of 90 px
  TEST_ASSERT_TRUE(c.has_bitmap(ic::minus));
  TEST_ASSERT_TRUE(c.has_bitmap(ic::plus));
}

void test_brightness_hold_repeats() {
  press(Button::TOP_LEFT);  // Fairy
  press(Button::BOTTOM_RIGHT);
  uint32_t t = now;
  ui->press(Button::BOTTOM_RIGHT, t);  // 60 -> 65
  TEST_ASSERT_EQUAL(65, fairy->brightness());
  TEST_ASSERT_FALSE(ui->tick(t + 499));
  TEST_ASSERT_TRUE(ui->tick(t + 500));  // -> 70
  TEST_ASSERT_EQUAL(70, fairy->brightness());
  TEST_ASSERT_FALSE(ui->tick(t + 600));
  TEST_ASSERT_TRUE(ui->tick(t + 650));  // -> 75
  TEST_ASSERT_EQUAL(75, fairy->brightness());
  ui->release(Button::BOTTOM_RIGHT, t + 700);
  TEST_ASSERT_FALSE(ui->tick(t + 2000));
  TEST_ASSERT_EQUAL(75, fairy->brightness());
}

void test_brightness_b1_returns_home() {
  press(Button::BOTTOM_RIGHT);
  auto c = render();
  TEST_ASSERT_TRUE(c.has_bitmap(ic::arrowLeftLarge));
  TEST_ASSERT_TRUE(c.has_text("Brightness"));
  TEST_ASSERT_FALSE(c.has_text("All lights"));
  TEST_ASSERT_TRUE(c.has_bitmap_at(ic::brightness, CENTRE_ICON_X, CENTRE_ICON_Y));
  TEST_ASSERT_EQUAL(1, (int) c.texts.size());  // no percentage
  press(Button::TOP_LEFT);
  TEST_ASSERT_EQUAL(Screen::HOME, ui->screen());
  TEST_ASSERT_EQUAL(0, ui->selected());  // still All lights
}

void test_submenu_timeout_returns_home() {
  press(Button::BOTTOM_RIGHT);
  uint32_t t = now;
  TEST_ASSERT_FALSE(ui->tick(t + 29000));
  TEST_ASSERT_EQUAL(Screen::BRIGHTNESS, ui->screen());
  TEST_ASSERT_TRUE(ui->tick(t + 31000));
  TEST_ASSERT_EQUAL(Screen::HOME, ui->screen());
}

/**
 * MODE
 */

void test_mode_cycle_colour_light() {
  select_wall();
  press(Button::TOP_RIGHT);
  TEST_ASSERT_EQUAL(Screen::MODE, ui->screen());
  auto c = render();
  TEST_ASSERT_TRUE(c.has_text("1 Colour"));
  TEST_ASSERT_FALSE(c.has_text("1 COLOUR"));
  TEST_ASSERT_FALSE(c.has_text("Wall RGB"));
  TEST_ASSERT_FALSE(c.has_text("MODE"));
  TEST_ASSERT_TRUE(c.has_bitmap_at(ic::swapL, B4_ICON_X, B4_ICON_Y));
  TEST_ASSERT_TRUE(c.has_bitmap_at(ic::colour, CENTRE_ICON_X, CENTRE_ICON_Y));
  press(Button::TOP_RIGHT);
  TEST_ASSERT_EQUAL(Mode::DUAL_COLOUR, wall->mode());
  c = render();
  TEST_ASSERT_TRUE(c.has_text("2 Colour"));
  TEST_ASSERT_FALSE(c.has_text("MODE"));
  TEST_ASSERT_TRUE(c.has_bitmap_at(ic::swapL, B4_ICON_X, B4_ICON_Y));
  TEST_ASSERT_TRUE(c.has_bitmap_at(ic::contrast, CENTRE_ICON_X, CENTRE_ICON_Y));
  press(Button::TOP_RIGHT);
  TEST_ASSERT_EQUAL(Mode::EFFECT, wall->mode());
  c = render();
  TEST_ASSERT_TRUE(c.has_text("Effects"));
  TEST_ASSERT_FALSE(c.has_text("MODE"));
  TEST_ASSERT_TRUE(c.has_bitmap_at(ic::swapL, B4_ICON_X, B4_ICON_Y));
  TEST_ASSERT_TRUE(c.has_bitmap_at(ic::pattern, CENTRE_ICON_X, CENTRE_ICON_Y));
  press(Button::TOP_RIGHT);
  TEST_ASSERT_EQUAL(Mode::COLOUR, wall->mode());
}

void test_mode_cycle_hbridge_light() {
  press(Button::TOP_LEFT);  // Fairy
  press(Button::TOP_RIGHT);
  auto c = render();
  TEST_ASSERT_TRUE(c.has_text("Static"));
  TEST_ASSERT_EQUAL(1, (int) c.texts.size());  // the title only: no STATIC label, no B4 label
  TEST_ASSERT_TRUE(c.has_bitmap_at(ic::unblocked, CENTRE_ICON_X, CENTRE_ICON_Y));
  TEST_ASSERT_TRUE(c.has_bitmap_at(ic::swapL, B4_ICON_X, B4_ICON_Y));
  TEST_ASSERT_FALSE(press(Button::BOTTOM_RIGHT));  // STATIC: B2/B3 unused
  press(Button::TOP_RIGHT);
  TEST_ASSERT_EQUAL(Mode::EFFECT, fairy->mode());
  c = render();
  TEST_ASSERT_TRUE(c.has_text("Effects"));
  TEST_ASSERT_TRUE(c.has_text("FADE"));
  press(Button::TOP_RIGHT);
  TEST_ASSERT_EQUAL(Mode::STATIC, fairy->mode());
}

void test_colour_prev_next_wraps() {
  select_wall();
  press(Button::TOP_RIGHT);
  press(Button::BOTTOM_LEFT);  // previous from the first -> last
  auto c = render();
  TEST_ASSERT_TRUE(c.has_text("NEUTRAL W."));
  TEST_ASSERT_TRUE(c.has_bitmap(ic::chevronLeft));
  press(Button::BOTTOM_RIGHT);
  TEST_ASSERT_EQUAL(0, wall->colour_index(0));
  TEST_ASSERT_TRUE(render().has_text("WARM W."));
}

void test_custom_colour_shows_custom() {
  select_wall();
  wall->set_custom_colour(0);
  press(Button::TOP_RIGHT);  // MODE
  TEST_ASSERT_TRUE(render().has_text("CUSTOM"));
  press(Button::BOTTOM_RIGHT);  // next -> first colour
  TEST_ASSERT_EQUAL(0, wall->colour_index(0));
}

void test_dual_colour_buttons_step_each_colour() {
  select_wall();
  press(Button::TOP_RIGHT);
  press(Button::TOP_RIGHT);  // 2 COLOUR (colours 0 and 1)
  press(Button::BOTTOM_LEFT);
  TEST_ASSERT_EQUAL(1, wall->colour_index(0));
  press(Button::BOTTOM_RIGHT);
  TEST_ASSERT_EQUAL(2, wall->colour_index(1));
  TEST_ASSERT_EQUAL(Mode::DUAL_COLOUR, wall->mode());
  auto c = render();
  TEST_ASSERT_TRUE(c.has_text("YELLOW"));
  TEST_ASSERT_TRUE(c.has_text("ORANGE"));
}

void test_dual_colour_names_share_the_line() {
  select_wall();
  wall->set_colour(0, 3);   // Red
  wall->set_colour(1, 11);  // Neutral W.: longer than half, but Red is short
  press(Button::TOP_RIGHT);
  auto c = render();
  TEST_ASSERT_TRUE(c.has_text("RED"));
  TEST_ASSERT_TRUE(c.has_text("NEUTRAL W."));
  wall->set_colour(0, 11);  // both long: half each
  c = render();
  const int half_w = (127 - 2 - 4) / 2;
  int truncated = 0;
  for (const auto &t : c.texts) {
    if (t.str.rfind("NEUTRAL", 0) != 0) continue;
    TEST_ASSERT_TRUE(t.str != "NEUTRAL W.");
    TEST_ASSERT_TRUE(c.text_width(lcui::Font::SMALL, t.str.c_str()) <= half_w);
    truncated++;
  }
  TEST_ASSERT_EQUAL(2, truncated);
}

void test_effect_prev_next_wraps_and_resumes() {
  select_wall();
  press(Button::TOP_RIGHT);
  press(Button::TOP_RIGHT);
  press(Button::TOP_RIGHT);  // EFFECTS
  press(Button::BOTTOM_RIGHT);
  TEST_ASSERT_TRUE(render().has_text("RAINBOW 180"));
  press(Button::BOTTOM_LEFT);
  press(Button::BOTTOM_LEFT);
  TEST_ASSERT_EQUAL(9, wall->effect_index());
  press(Button::TOP_RIGHT);  // 1 COLOUR
  press(Button::TOP_RIGHT);  // 2 COLOUR
  press(Button::TOP_RIGHT);  // EFFECTS again: resumes Candlelight
  TEST_ASSERT_TRUE(render().has_text("CANDLELIGHT"));
}

void test_longest_effect_fits_between_the_chevrons() {
  press(Button::TOP_LEFT);  // Fairy
  press(Button::TOP_RIGHT);
  press(Button::TOP_RIGHT);  // EFFECTS
  press(Button::BOTTOM_RIGHT);
  press(Button::BOTTOM_RIGHT);  // Smooth Random
  TEST_ASSERT_TRUE(render().has_text("SMOOTH RANDOM"));
}

void test_mode_b1_returns_home() {
  select_wall();
  press(Button::TOP_RIGHT);
  press(Button::TOP_LEFT);
  TEST_ASSERT_EQUAL(Screen::HOME, ui->screen());
  TEST_ASSERT_TRUE(render().has_text("Wall RGB"));
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_load_fonts);
  RUN_TEST(test_home_starts_on_all_lights);
  RUN_TEST(test_b1_cycles_lights_and_wraps);
  RUN_TEST(test_single_light_has_no_all_lights_entry);
  RUN_TEST(test_power_all_lights_turns_all_off_then_on);
  RUN_TEST(test_power_all_lights_any_on_counts_as_on);
  RUN_TEST(test_power_single_light);
  RUN_TEST(test_b4_unused_on_all_lights);
  RUN_TEST(test_light_off_hides_and_disables_b3_b4);
  RUN_TEST(test_all_lights_off_hides_and_disables_b3);
  RUN_TEST(test_home_b4_shows_mode_icon_only);
  RUN_TEST(test_no_positions_shown);
  RUN_TEST(test_brightness_all_lights_starts_from_brightest_and_syncs);
  RUN_TEST(test_brightness_steps_and_limits);
  RUN_TEST(test_brightness_down_to_zero_turns_off);
  RUN_TEST(test_brightness_all_lights_changes_only_lights_that_are_on);
  RUN_TEST(test_brightness_has_20_steps);
  RUN_TEST(test_brightness_bar_fill);
  RUN_TEST(test_brightness_hold_repeats);
  RUN_TEST(test_brightness_b1_returns_home);
  RUN_TEST(test_submenu_timeout_returns_home);
  RUN_TEST(test_mode_cycle_colour_light);
  RUN_TEST(test_mode_cycle_hbridge_light);
  RUN_TEST(test_colour_prev_next_wraps);
  RUN_TEST(test_custom_colour_shows_custom);
  RUN_TEST(test_dual_colour_buttons_step_each_colour);
  RUN_TEST(test_dual_colour_names_share_the_line);
  RUN_TEST(test_effect_prev_next_wraps_and_resumes);
  RUN_TEST(test_longest_effect_fits_between_the_chevrons);
  RUN_TEST(test_mode_b1_returns_home);
  return UNITY_END();
}
