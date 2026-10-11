#include "lcui_ui.h"

#include <stdio.h>
#include <string.h>

#include "lcui_icons.h"

namespace lcui {

namespace {

namespace ic = oli::icons_12x12;

/**
 * -------------------------------------------------------------------------------------------------
 * LAYOUT
 *
 * 128x64, tuned for 12x12 icons (glyph area 11x11, column 11 and row 11 blank) with LARGE = 6x13B
 * and SMALL = 5x7. Both fonts leave a blank column on the right of each glyph, so right-aligned
 * text is anchored one column further right than the icons. Everything keeps 2 px from the edge.
 * -------------------------------------------------------------------------------------------------
 */

constexpr int ICON_LEFT_X = 2;      // columns 2-12
constexpr int ICON_RIGHT_X = 115;   // columns 115-125
constexpr int ICON_CENTRE_X = 58;   // columns 58-68
constexpr int ICON_TOP_Y = 1;       // rows 1-11
constexpr int ICON_BOTTOM_Y = 51;   // rows 51-61
constexpr int ICON_CENTRE_Y = 25;   // rows 25-35

constexpr int TEXT_LEFT_X = 2;
constexpr int TEXT_RIGHT_X = 127;   // right-aligned text ends on column 125
constexpr int TEXT_CENTRE_X = 64;
constexpr int TITLE_X = 17;         // 4 px after the B1 icon

constexpr int TITLE_BASELINE = 11;         // LARGE capitals on rows 2-10, level with the B1 icon
constexpr int SUBTITLE_BASELINE = 20;      // SMALL capitals on rows 14-19
constexpr int SUBTITLE2_BASELINE = 28;     // second line under the B4 icon (two colours)
constexpr int BOTTOM_LABEL_BASELINE = 48;  // directly above the bottom icons
constexpr int BOTTOM_LABEL2_BASELINE = 40; // the line above that
constexpr int CENTRE_TEXT_BASELINE = 53;   // between the chevrons
constexpr int CENTRE_TEXT2_BASELINE = 61;

constexpr int TITLE_MAX_W = ICON_RIGHT_X - 4 - TITLE_X;
constexpr int GAP = 4;

// Brightness bar between the minus and plus icons, level with them.
constexpr int BAR_X = 17;
constexpr int BAR_Y = 51;
constexpr int BAR_W = 94;
constexpr int BAR_H = 11;

constexpr uint8_t BRIGHTNESS_STEP = 10;
constexpr uint32_t REPEAT_DELAY_MS = 500;
constexpr uint32_t REPEAT_INTERVAL_MS = 150;

constexpr size_t TEXT_MAX = 32;

const char *const ALL_LIGHTS = "All lights";

enum class Align : uint8_t { LEFT, CENTRE, RIGHT };

/** Copy str into buf (TEXT_MAX bytes), upper-cased if upper. Returns the length. */
size_t copy_text(char *buf, const char *str, bool upper) {
  size_t n = 0;
  for (; str[n] != '\0' && n < TEXT_MAX - 1; n++) {
    char ch = str[n];
    buf[n] = (upper && ch >= 'a' && ch <= 'z') ? (char) (ch - 'a' + 'A') : ch;
  }
  buf[n] = '\0';
  return n;
}

/**
 * Draw text anchored at x (left edge, centre, or one past the right edge), dropping characters
 * from the end until it fits in max_w. upper = convert to upper case (all SMALL labels).
 */
void draw_text(Canvas &c, int x, int baseline, Font font, const char *str, Align align, int max_w,
               bool upper) {
  char buf[TEXT_MAX];
  size_t n = copy_text(buf, str, upper);
  int w = c.text_width(font, buf);
  while (n > 0 && w > max_w) {
    buf[--n] = '\0';
    w = c.text_width(font, buf);
  }
  int left = x;
  if (align == Align::CENTRE) left = x - w / 2;
  if (align == Align::RIGHT) left = x - w;
  c.text(left, baseline, font, buf);
}

void label(Canvas &c, int x, int baseline, const char *str, Align align, int max_w = 124) {
  draw_text(c, x, baseline, Font::SMALL, str, align, max_w, true);
}

int label_width(Canvas &c, const char *str) {
  char buf[TEXT_MAX];
  copy_text(buf, str, true);
  return c.text_width(Font::SMALL, buf);
}

void title(Canvas &c, const char *str) {
  draw_text(c, TITLE_X, TITLE_BASELINE, Font::LARGE, str, Align::LEFT, TITLE_MAX_W, false);
}

void icon(Canvas &c, int x, int y, const uint8_t *bmp) { c.bitmap(x, y, bmp, ic::W, ic::H); }

const uint8_t *mode_icon(Mode mode) {
  switch (mode) {
    case Mode::COLOUR:
      return ic::colour;
    case Mode::DUAL_COLOUR:
      return ic::contrast;
    case Mode::EFFECT:
      return ic::pattern;
    case Mode::STATIC:
    default:
      return ic::unblocked;
  }
}

const char *mode_label(Mode mode) {
  switch (mode) {
    case Mode::COLOUR:
      return "1 COLOUR";
    case Mode::DUAL_COLOUR:
      return "2 COLOUR";
    case Mode::EFFECT:
      return "EFFECTS";
    case Mode::STATIC:
    default:
      return "STATIC";
  }
}

Mode next_mode(const LightPort &light) {
  Mode mode = light.mode();
  if (light.has_colour()) {
    if (mode == Mode::COLOUR) return Mode::DUAL_COLOUR;
    if (mode == Mode::DUAL_COLOUR) return Mode::EFFECT;
    return Mode::COLOUR;
  }
  return mode == Mode::EFFECT ? Mode::STATIC : Mode::EFFECT;
}

/** Next/previous index in a list of n, wrapping. current -1 (not in the list) goes to an end. */
int cycle(int current, int n, int dir) {
  if (current < 0 || current >= n) return dir > 0 ? 0 : n - 1;
  return (current + dir + n) % n;
}

void cycle_colour(LightPort &light, uint8_t slot, int dir) {
  int n = light.colour_count();
  if (n > 0) light.set_colour(slot, (uint8_t) cycle(light.colour_index(slot), n, dir));
}

void cycle_effect(LightPort &light, int dir) {
  int n = light.effect_count();
  if (n > 0) light.set_effect((uint8_t) cycle(light.effect_index(), n, dir));
}

/** "3 / 12", or "- / 12" when the index isn't in the list. */
void format_count(char *buf, size_t size, int index, int n) {
  if (index < 0)
    snprintf(buf, size, "- / %d", n);
  else
    snprintf(buf, size, "%d / %d", index + 1, n);
}

const char *colour_label(const LightPort &light, uint8_t slot) {
  int i = light.colour_index(slot);
  return i < 0 ? "CUSTOM" : light.colour_name((uint8_t) i);
}

const char *effect_label(const LightPort &light) {
  int i = light.effect_index();
  return i < 0 ? "" : light.effect_name((uint8_t) i);
}

}  // namespace

/**
 * -------------------------------------------------------------------------------------------------
 * LIGHTS
 * -------------------------------------------------------------------------------------------------
 */

void Ui::add_light(LightPort *light) {
  if (light != nullptr && this->light_count_ < MAX_LIGHTS) this->lights_[this->light_count_++] = light;
}

uint8_t Ui::entry_count() const { return this->light_count_ >= 2 ? this->light_count_ + 1 : this->light_count_; }

LightPort *Ui::selected_light() const {
  if (this->light_count_ == 0) return nullptr;
  if (this->light_count_ == 1) return this->lights_[0];
  return this->selected_ == 0 ? nullptr : this->lights_[this->selected_ - 1];
}

bool Ui::any_on() const {
  LightPort *light = this->selected_light();
  if (light != nullptr) return light->is_on();
  for (uint8_t i = 0; i < this->light_count_; i++) {
    if (this->lights_[i]->is_on()) return true;
  }
  return false;
}

/** The selected light's brightness; for All lights, the brightest light's. */
uint8_t Ui::brightness() const {
  LightPort *light = this->selected_light();
  if (light != nullptr) return light->brightness();
  uint8_t max = 0;
  for (uint8_t i = 0; i < this->light_count_; i++) {
    uint8_t b = this->lights_[i]->brightness();
    if (b > max) max = b;
  }
  return max;
}

/**
 * Move to the next multiple of BRIGHTNESS_STEP up or down, between BRIGHTNESS_STEP and 100 %.
 * All lights are all set to the new value, so they match after the first press.
 */
void Ui::step_brightness(bool up) {
  int value = this->brightness();
  int next;
  if (up) {
    next = (value / BRIGHTNESS_STEP + 1) * BRIGHTNESS_STEP;
    if (next > 100) next = 100;
  } else {
    next = ((value + BRIGHTNESS_STEP - 1) / BRIGHTNESS_STEP - 1) * BRIGHTNESS_STEP;
    if (next < BRIGHTNESS_STEP) next = value < BRIGHTNESS_STEP ? value : BRIGHTNESS_STEP;
  }
  LightPort *light = this->selected_light();
  if (light != nullptr) {
    light->set_brightness((uint8_t) next);
    return;
  }
  for (uint8_t i = 0; i < this->light_count_; i++) this->lights_[i]->set_brightness((uint8_t) next);
}

/**
 * -------------------------------------------------------------------------------------------------
 * INPUT
 * -------------------------------------------------------------------------------------------------
 */

bool Ui::press(Button button, uint32_t now_ms) {
  this->last_input_ms_ = now_ms;
  if (this->light_count_ == 0) return false;
  switch (this->screen_) {
    case Screen::HOME:
      return this->press_home(button);
    case Screen::BRIGHTNESS:
      return this->press_brightness(button, now_ms);
    case Screen::MODE:
      return this->press_mode(button);
  }
  return false;
}

bool Ui::release(Button button, uint32_t now_ms) {
  if (this->held_ == (int8_t) button) this->held_ = -1;
  return false;
}

bool Ui::tick(uint32_t now_ms) {
  if (this->screen_ != Screen::HOME && this->submenu_timeout_ms_ > 0 &&
      now_ms - this->last_input_ms_ >= this->submenu_timeout_ms_) {
    this->screen_ = Screen::HOME;
    this->held_ = -1;
    return true;
  }
  if (this->held_ >= 0 && this->screen_ == Screen::BRIGHTNESS && (int32_t) (now_ms - this->next_repeat_ms_) >= 0) {
    this->step_brightness(this->held_ == (int8_t) Button::BOTTOM_RIGHT);
    this->next_repeat_ms_ = now_ms + REPEAT_INTERVAL_MS;
    this->last_input_ms_ = now_ms;
    return true;
  }
  return false;
}

bool Ui::press_home(Button button) {
  LightPort *light = this->selected_light();
  switch (button) {
    case Button::TOP_LEFT:
      this->selected_ = (uint8_t) ((this->selected_ + 1) % this->entry_count());
      return true;
    case Button::BOTTOM_LEFT: {
      // All lights: if any light is on, turn them all off; otherwise turn them all on.
      bool on = !this->any_on();
      if (light != nullptr) {
        light->set_on(on);
      } else {
        for (uint8_t i = 0; i < this->light_count_; i++) this->lights_[i]->set_on(on);
      }
      return true;
    }
    case Button::BOTTOM_RIGHT:
      this->screen_ = Screen::BRIGHTNESS;
      return true;
    case Button::TOP_RIGHT:
      if (light == nullptr) return false;
      this->screen_ = Screen::MODE;
      return true;
  }
  return false;
}

bool Ui::press_brightness(Button button, uint32_t now_ms) {
  switch (button) {
    case Button::TOP_LEFT:
      this->screen_ = Screen::HOME;
      this->held_ = -1;
      return true;
    case Button::BOTTOM_LEFT:
    case Button::BOTTOM_RIGHT:
      this->step_brightness(button == Button::BOTTOM_RIGHT);
      this->held_ = (int8_t) button;
      this->next_repeat_ms_ = now_ms + REPEAT_DELAY_MS;
      return true;
    case Button::TOP_RIGHT:
      return false;
  }
  return false;
}

bool Ui::press_mode(Button button) {
  LightPort *light = this->selected_light();
  if (light == nullptr) {  // Not reachable from HOME; recover if the light list changed
    this->screen_ = Screen::HOME;
    return true;
  }
  if (button == Button::TOP_LEFT) {
    this->screen_ = Screen::HOME;
    return true;
  }
  if (button == Button::TOP_RIGHT) {
    light->set_mode(next_mode(*light));
    return true;
  }
  int dir = button == Button::BOTTOM_RIGHT ? 1 : -1;
  switch (light->mode()) {
    case Mode::COLOUR:
      cycle_colour(*light, 0, dir);
      return true;
    case Mode::DUAL_COLOUR:
      cycle_colour(*light, button == Button::BOTTOM_LEFT ? 0 : 1, 1);
      return true;
    case Mode::EFFECT:
      cycle_effect(*light, dir);
      return true;
    case Mode::STATIC:
      return false;
  }
  return false;
}

/**
 * -------------------------------------------------------------------------------------------------
 * RENDERING
 * -------------------------------------------------------------------------------------------------
 */

void Ui::render(Canvas &canvas) const {
  if (this->light_count_ == 0) return;
  switch (this->screen_) {
    case Screen::HOME:
      this->render_home(canvas);
      break;
    case Screen::BRIGHTNESS:
      this->render_brightness(canvas);
      break;
    case Screen::MODE:
      this->render_mode(canvas);
      break;
  }
}

void Ui::render_home(Canvas &c) const {
  LightPort *light = this->selected_light();
  char buf[16];

  // B1: next light
  icon(c, ICON_LEFT_X, ICON_TOP_Y, ic::swapL);
  title(c, light != nullptr ? light->name() : ALL_LIGHTS);
  snprintf(buf, sizeof(buf), "%d / %d", this->selected_ + 1, this->entry_count());
  label(c, TITLE_X, SUBTITLE_BASELINE, buf, Align::LEFT);
  int b4_max_w = TEXT_RIGHT_X - (TITLE_X + c.text_width(Font::SMALL, buf) + GAP);

  // B2: on/off
  icon(c, ICON_LEFT_X, ICON_BOTTOM_Y, ic::power);
  label(c, TEXT_LEFT_X, BOTTOM_LABEL_BASELINE, this->any_on() ? "ON" : "OFF", Align::LEFT);

  // B3: brightness
  icon(c, ICON_RIGHT_X, ICON_BOTTOM_Y, ic::brightness);
  snprintf(buf, sizeof(buf), "%d%%", this->brightness());
  label(c, TEXT_RIGHT_X, BOTTOM_LABEL_BASELINE, buf, Align::RIGHT);

  // B4: mode (single lights only)
  if (light == nullptr) return;
  Mode mode = light->mode();
  icon(c, ICON_RIGHT_X, ICON_TOP_Y, mode_icon(mode));
  if (mode == Mode::DUAL_COLOUR) {
    label(c, TEXT_RIGHT_X, SUBTITLE_BASELINE, colour_label(*light, 0), Align::RIGHT, b4_max_w);
    label(c, TEXT_RIGHT_X, SUBTITLE2_BASELINE, colour_label(*light, 1), Align::RIGHT, TEXT_RIGHT_X - TITLE_X);
    return;
  }
  const char *text = mode == Mode::COLOUR   ? colour_label(*light, 0)
                     : mode == Mode::EFFECT ? effect_label(*light)
                                            : "STATIC";
  // Beside the count if it fits, otherwise on the line below
  if (label_width(c, text) <= b4_max_w) {
    label(c, TEXT_RIGHT_X, SUBTITLE_BASELINE, text, Align::RIGHT);
  } else {
    label(c, TEXT_RIGHT_X, SUBTITLE2_BASELINE, text, Align::RIGHT, TEXT_RIGHT_X - TITLE_X);
  }
}

void Ui::render_brightness(Canvas &c) const {
  LightPort *light = this->selected_light();

  // B1: back
  icon(c, ICON_LEFT_X, ICON_TOP_Y, ic::arrowLeftLarge);
  title(c, light != nullptr ? light->name() : ALL_LIGHTS);

  icon(c, ICON_CENTRE_X, ICON_CENTRE_Y, ic::brightness);

  // B2 down, B3 up, with the bar between them
  icon(c, ICON_LEFT_X, ICON_BOTTOM_Y, ic::minus);
  icon(c, ICON_RIGHT_X, ICON_BOTTOM_Y, ic::plus);
  c.rect(BAR_X, BAR_Y, BAR_W, BAR_H);
  int fill = (BAR_W - 4) * this->brightness() / 100;
  if (fill > 0) c.fill_rect(BAR_X + 2, BAR_Y + 2, fill, BAR_H - 4, true);
}

void Ui::render_mode(Canvas &c) const {
  LightPort *light = this->selected_light();
  if (light == nullptr) return;
  char buf[16];

  // B1: back
  icon(c, ICON_LEFT_X, ICON_TOP_Y, ic::arrowLeftLarge);
  title(c, light->name());

  // B4: next mode
  Mode mode = light->mode();
  icon(c, ICON_RIGHT_X, ICON_TOP_Y, mode_icon(mode));
  label(c, TEXT_RIGHT_X, SUBTITLE_BASELINE, mode_label(mode), Align::RIGHT);

  // Centre text fits between the chevrons
  const int centre_max_w = (ICON_RIGHT_X - GAP) - (ICON_LEFT_X + ic::W + GAP);

  switch (mode) {
    case Mode::COLOUR:
    case Mode::EFFECT: {
      icon(c, ICON_LEFT_X, ICON_BOTTOM_Y, ic::chevronLeft);
      icon(c, ICON_RIGHT_X, ICON_BOTTOM_Y, ic::chevronRight);
      icon(c, ICON_CENTRE_X, ICON_CENTRE_Y, mode_icon(mode));
      bool colour = mode == Mode::COLOUR;
      int n = colour ? light->colour_count() : light->effect_count();
      int i = colour ? light->colour_index(0) : light->effect_index();
      label(c, TEXT_CENTRE_X, CENTRE_TEXT_BASELINE, colour ? colour_label(*light, 0) : effect_label(*light),
            Align::CENTRE, centre_max_w);
      format_count(buf, sizeof(buf), i, n);
      label(c, TEXT_CENTRE_X, CENTRE_TEXT2_BASELINE, buf, Align::CENTRE);
      break;
    }
    case Mode::DUAL_COLOUR: {
      // B2: colour 1 (bottom-left), B3: colour 2 (bottom-right), each with its name and position.
      // The names share one line: each gets half, or more if the other is short.
      const char *name0 = colour_label(*light, 0);
      const char *name1 = colour_label(*light, 1);
      const int line_w = TEXT_RIGHT_X - TEXT_LEFT_X - GAP;
      const int half_w = line_w / 2;
      int w0 = label_width(c, name0), w1 = label_width(c, name1);
      int n = light->colour_count();
      icon(c, ICON_LEFT_X, ICON_BOTTOM_Y, ic::colour);
      label(c, TEXT_LEFT_X, BOTTOM_LABEL2_BASELINE, name0, Align::LEFT, w1 < half_w ? line_w - w1 : half_w);
      format_count(buf, sizeof(buf), light->colour_index(0), n);
      label(c, TEXT_LEFT_X, BOTTOM_LABEL_BASELINE, buf, Align::LEFT);
      icon(c, ICON_RIGHT_X, ICON_BOTTOM_Y, ic::colour);
      label(c, TEXT_RIGHT_X, BOTTOM_LABEL2_BASELINE, name1, Align::RIGHT, w0 < half_w ? line_w - w0 : half_w);
      format_count(buf, sizeof(buf), light->colour_index(1), n);
      label(c, TEXT_RIGHT_X, BOTTOM_LABEL_BASELINE, buf, Align::RIGHT);
      break;
    }
    case Mode::STATIC:
      icon(c, ICON_CENTRE_X, ICON_CENTRE_Y, ic::unblocked);
      label(c, TEXT_CENTRE_X, CENTRE_TEXT_BASELINE, "STATIC", Align::CENTRE);
      break;
  }
}

}  // namespace lcui
