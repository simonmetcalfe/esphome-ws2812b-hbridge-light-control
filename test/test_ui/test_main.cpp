/**
 * UI behaviour tests (host): pio test -e native -f test_ui
 *
 * The lights mirror balconylights.yaml: Fairy lights (H-bridge, 4 effects) and Wall lights
 * (WS2812B, 10 effects, 12 colours).
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
const char *const WALL_EFFECTS[] = {"Rainbow 360", "Rainbow Horizontal", "Aurora Blue", "Aurora Purple",
                                    "Aurora Multi", "Sunset", "Sunrise", "Lava", "Forest", "Candlelight"};
const char *const WALL_COLOURS[] = {"Warm White", "Yellow", "Orange", "Red", "Pink", "Purple",
                                    "Blue", "Cyan", "Aquamarine", "Green", "Cool White", "Neutral White"};

FakeLight *fairy;
FakeLight *wall;
lcui::Ui *ui;
uint32_t now;

void setUp() {
  fairy = new FakeLight("Fairy lights", FAIRY_EFFECTS, 4);
  wall = new FakeLight("Wall lights", WALL_EFFECTS, 10, WALL_COLOURS, 12);
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
  RecordingCanvas c;
  ui->render(c);
  return c;
}

void select_wall() {
  press(Button::TOP_LEFT);
  press(Button::TOP_LEFT);
}

/**
 * HOME
 */

void test_home_starts_on_all_lights() {
  auto c = render();
  TEST_ASSERT_TRUE(c.has_text("All lights"));
  TEST_ASSERT_TRUE(c.has_text("1 / 3"));
  TEST_ASSERT_TRUE(c.has_text("ON"));
  TEST_ASSERT_TRUE(c.has_text("60%"));
  TEST_ASSERT_TRUE(c.has_bitmap(ic::swapL));
  // No B4 on All lights
  TEST_ASSERT_FALSE(c.has_bitmap(ic::colour));
  TEST_ASSERT_FALSE(c.has_bitmap(ic::unblocked));
}

void test_b1_cycles_lights_and_wraps() {
  press(Button::TOP_LEFT);
  auto c = render();
  TEST_ASSERT_TRUE(c.has_text("Fairy lights"));
  TEST_ASSERT_TRUE(c.has_text("2 / 3"));
  press(Button::TOP_LEFT);
  c = render();
  TEST_ASSERT_TRUE(c.has_text("Wall lights"));
  TEST_ASSERT_TRUE(c.has_text("3 / 3"));
  press(Button::TOP_LEFT);
  TEST_ASSERT_TRUE(render().has_text("All lights"));
}

void test_single_light_has_no_all_lights_entry() {
  lcui::Ui single;
  single.add_light(fairy);
  TEST_ASSERT_EQUAL(1, single.entry_count());
  RecordingCanvas c;
  single.render(c);
  TEST_ASSERT_TRUE(c.has_text("Fairy lights"));
  TEST_ASSERT_TRUE(c.has_text("1 / 1"));
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
  press(Button::TOP_LEFT);  // Fairy lights
  press(Button::BOTTOM_LEFT);
  TEST_ASSERT_FALSE(fairy->is_on());
  TEST_ASSERT_TRUE(wall->is_on());
}

void test_b4_unused_on_all_lights() {
  TEST_ASSERT_FALSE(press(Button::TOP_RIGHT));
  TEST_ASSERT_EQUAL(Screen::HOME, ui->screen());
}

void test_home_b4_shows_mode() {
  select_wall();
  auto c = render();
  TEST_ASSERT_TRUE(c.has_bitmap(ic::colour));
  TEST_ASSERT_TRUE(c.has_text("WARM WHITE"));

  wall->set_colour(1, 3);  // colour 2 -> two-colour mode
  c = render();
  TEST_ASSERT_TRUE(c.has_bitmap(ic::contrast));
  TEST_ASSERT_TRUE(c.has_text("WARM WHITE"));
  TEST_ASSERT_TRUE(c.has_text("RED"));

  wall->set_effect(2);
  c = render();
  TEST_ASSERT_TRUE(c.has_bitmap(ic::pattern));
  TEST_ASSERT_TRUE(c.has_text("AURORA BLUE"));

  press(Button::TOP_LEFT);  // All lights
  press(Button::TOP_LEFT);  // Fairy lights (STATIC)
  c = render();
  TEST_ASSERT_TRUE(c.has_bitmap(ic::unblocked));
  TEST_ASSERT_TRUE(c.has_text("STATIC"));
}

void test_long_b4_label_moves_to_next_line() {
  select_wall();
  wall->set_effect(2);  // Aurora Blue: fits beside "3 / 3"
  TEST_ASSERT_EQUAL(20, render().find_text("AURORA BLUE")->baseline);
  wall->set_effect(1);  // Rainbow Horizontal: 90 px, only 81 beside "3 / 3"
  auto c = render();
  const auto *t = c.find_text("RAINBOW HORIZONTAL");
  TEST_ASSERT_NOT_NULL(t);
  TEST_ASSERT_EQUAL(28, t->baseline);
  TEST_ASSERT_EQUAL(127 - 18 * 5, t->x);  // right-aligned
}

void test_dual_colour_names_share_the_line() {
  select_wall();
  wall->set_colour(0, 3);   // Red
  wall->set_colour(1, 11);  // Neutral White: longer than half, but Red is short
  press(Button::TOP_RIGHT);
  auto c = render();
  TEST_ASSERT_TRUE(c.has_text("RED"));
  TEST_ASSERT_TRUE(c.has_text("NEUTRAL WHITE"));
  wall->set_colour(0, 11);  // both long: half each
  c = render();
  TEST_ASSERT_EQUAL(2, (int) std::count_if(c.texts.begin(), c.texts.end(),
                                           [](const RecordingCanvas::Text &t) { return t.str == "NEUTRAL WHIT"; }));
}

void test_custom_colour_shows_custom() {
  select_wall();
  wall->set_custom_colour(0);
  TEST_ASSERT_TRUE(render().has_text("CUSTOM"));
  press(Button::TOP_RIGHT);  // MODE
  auto c = render();
  TEST_ASSERT_TRUE(c.has_text("CUSTOM"));
  TEST_ASSERT_TRUE(c.has_text("- / 12"));
  press(Button::BOTTOM_RIGHT);  // next -> first colour
  TEST_ASSERT_EQUAL(0, wall->colour_index(0));
}

/**
 * BRIGHTNESS
 */

void test_brightness_all_lights_starts_from_brightest_and_syncs() {
  fairy->set_brightness(30);
  wall->set_brightness(75);
  TEST_ASSERT_TRUE(render().has_text("75%"));
  press(Button::BOTTOM_RIGHT);
  TEST_ASSERT_EQUAL(Screen::BRIGHTNESS, ui->screen());
  press(Button::BOTTOM_RIGHT);  // up
  TEST_ASSERT_EQUAL(80, fairy->brightness());
  TEST_ASSERT_EQUAL(80, wall->brightness());
  press(Button::BOTTOM_LEFT);  // down
  TEST_ASSERT_EQUAL(70, fairy->brightness());
  TEST_ASSERT_EQUAL(70, wall->brightness());
}

void test_brightness_steps_and_limits() {
  press(Button::TOP_LEFT);  // Fairy lights
  press(Button::BOTTOM_RIGHT);
  fairy->set_brightness(63);
  press(Button::BOTTOM_RIGHT);
  TEST_ASSERT_EQUAL(70, fairy->brightness());
  fairy->set_brightness(63);
  press(Button::BOTTOM_LEFT);
  TEST_ASSERT_EQUAL(60, fairy->brightness());
  fairy->set_brightness(100);
  press(Button::BOTTOM_RIGHT);
  TEST_ASSERT_EQUAL(100, fairy->brightness());
  fairy->set_brightness(10);
  press(Button::BOTTOM_LEFT);
  TEST_ASSERT_EQUAL(10, fairy->brightness());
  fairy->set_brightness(5);  // set below the minimum elsewhere: down leaves it alone
  press(Button::BOTTOM_LEFT);
  TEST_ASSERT_EQUAL(5, fairy->brightness());
  wall->set_brightness(42);
  TEST_ASSERT_EQUAL(5, fairy->brightness());  // only the selected light changes
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
  press(Button::TOP_LEFT);  // Fairy lights
  press(Button::BOTTOM_RIGHT);
  uint32_t t = now;
  ui->press(Button::BOTTOM_RIGHT, t);  // 60 -> 70
  TEST_ASSERT_EQUAL(70, fairy->brightness());
  TEST_ASSERT_FALSE(ui->tick(t + 499));
  TEST_ASSERT_TRUE(ui->tick(t + 500));  // -> 80
  TEST_ASSERT_EQUAL(80, fairy->brightness());
  TEST_ASSERT_FALSE(ui->tick(t + 600));
  TEST_ASSERT_TRUE(ui->tick(t + 650));  // -> 90
  TEST_ASSERT_EQUAL(90, fairy->brightness());
  ui->release(Button::BOTTOM_RIGHT, t + 700);
  TEST_ASSERT_FALSE(ui->tick(t + 2000));
  TEST_ASSERT_EQUAL(90, fairy->brightness());
}

void test_brightness_b1_returns_home() {
  press(Button::BOTTOM_RIGHT);
  auto c = render();
  TEST_ASSERT_TRUE(c.has_bitmap(ic::arrowLeftLarge));
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
  TEST_ASSERT_TRUE(render().has_text("1 COLOUR"));
  press(Button::TOP_RIGHT);
  TEST_ASSERT_EQUAL(Mode::DUAL_COLOUR, wall->mode());
  TEST_ASSERT_TRUE(render().has_text("2 COLOUR"));
  press(Button::TOP_RIGHT);
  TEST_ASSERT_EQUAL(Mode::EFFECT, wall->mode());
  TEST_ASSERT_TRUE(render().has_text("EFFECTS"));
  press(Button::TOP_RIGHT);
  TEST_ASSERT_EQUAL(Mode::COLOUR, wall->mode());
}

void test_mode_cycle_hbridge_light() {
  press(Button::TOP_LEFT);  // Fairy lights
  press(Button::TOP_RIGHT);
  auto c = render();
  TEST_ASSERT_TRUE(c.has_text("STATIC"));
  TEST_ASSERT_TRUE(c.has_bitmap(ic::unblocked));
  TEST_ASSERT_FALSE(press(Button::BOTTOM_RIGHT));  // STATIC: B2/B3 unused
  press(Button::TOP_RIGHT);
  TEST_ASSERT_EQUAL(Mode::EFFECT, fairy->mode());
  c = render();
  TEST_ASSERT_TRUE(c.has_text("EFFECTS"));
  TEST_ASSERT_TRUE(c.has_text("FADE"));
  TEST_ASSERT_TRUE(c.has_text("1 / 4"));
  press(Button::TOP_RIGHT);
  TEST_ASSERT_EQUAL(Mode::STATIC, fairy->mode());
}

void test_colour_prev_next_wraps() {
  select_wall();
  press(Button::TOP_RIGHT);
  press(Button::BOTTOM_LEFT);  // previous from the first -> last
  auto c = render();
  TEST_ASSERT_TRUE(c.has_text("NEUTRAL WHITE"));
  TEST_ASSERT_TRUE(c.has_text("12 / 12"));
  TEST_ASSERT_TRUE(c.has_bitmap(ic::chevronLeft));
  press(Button::BOTTOM_RIGHT);
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
  TEST_ASSERT_TRUE(c.has_text("2 / 12"));
  TEST_ASSERT_TRUE(c.has_text("3 / 12"));
}

void test_effect_prev_next_wraps_and_resumes() {
  select_wall();
  press(Button::TOP_RIGHT);
  press(Button::TOP_RIGHT);
  press(Button::TOP_RIGHT);  // EFFECTS
  press(Button::BOTTOM_RIGHT);
  auto c = render();
  TEST_ASSERT_TRUE(c.has_text("RAINBOW HORIZONTAL"));  // fits between the chevrons
  TEST_ASSERT_TRUE(c.has_text("2 / 10"));
  press(Button::BOTTOM_LEFT);
  press(Button::BOTTOM_LEFT);
  TEST_ASSERT_EQUAL(9, wall->effect_index());
  press(Button::TOP_RIGHT);  // 1 COLOUR
  press(Button::TOP_RIGHT);  // 2 COLOUR
  press(Button::TOP_RIGHT);  // EFFECTS again: resumes Candlelight
  TEST_ASSERT_TRUE(render().has_text("CANDLELIGHT"));
}

void test_mode_b1_returns_home() {
  select_wall();
  press(Button::TOP_RIGHT);
  press(Button::TOP_LEFT);
  TEST_ASSERT_EQUAL(Screen::HOME, ui->screen());
  TEST_ASSERT_TRUE(render().has_text("Wall lights"));
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_home_starts_on_all_lights);
  RUN_TEST(test_b1_cycles_lights_and_wraps);
  RUN_TEST(test_single_light_has_no_all_lights_entry);
  RUN_TEST(test_power_all_lights_turns_all_off_then_on);
  RUN_TEST(test_power_all_lights_any_on_counts_as_on);
  RUN_TEST(test_power_single_light);
  RUN_TEST(test_b4_unused_on_all_lights);
  RUN_TEST(test_home_b4_shows_mode);
  RUN_TEST(test_long_b4_label_moves_to_next_line);
  RUN_TEST(test_dual_colour_names_share_the_line);
  RUN_TEST(test_custom_colour_shows_custom);
  RUN_TEST(test_brightness_all_lights_starts_from_brightest_and_syncs);
  RUN_TEST(test_brightness_steps_and_limits);
  RUN_TEST(test_brightness_bar_fill);
  RUN_TEST(test_brightness_hold_repeats);
  RUN_TEST(test_brightness_b1_returns_home);
  RUN_TEST(test_submenu_timeout_returns_home);
  RUN_TEST(test_mode_cycle_colour_light);
  RUN_TEST(test_mode_cycle_hbridge_light);
  RUN_TEST(test_colour_prev_next_wraps);
  RUN_TEST(test_dual_colour_buttons_step_each_colour);
  RUN_TEST(test_effect_prev_next_wraps_and_resumes);
  RUN_TEST(test_mode_b1_returns_home);
  return UNITY_END();
}
