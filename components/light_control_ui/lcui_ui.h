#pragma once

#include <stdint.h>

#include "lcui_canvas.h"
#include "lcui_light.h"

namespace lcui {

/**
 * -------------------------------------------------------------------------------------------------
 * LIGHT CONTROL UI
 *
 * The OLED menu for up to MAX_LIGHTS lights on a 128x64 screen with four buttons, one at each
 * corner. Each button's icon and label are drawn next to it, and change per screen.
 *
 *   B1 top-left      select / back
 *   B2 bottom-left
 *   B3 bottom-right
 *   B4 top-right
 *
 * Screens:
 *   HOME        B1 next light (All lights first when there are 2+ lights), B2 on/off,
 *               B3 brightness screen, B4 mode screen (single lights only). B3 and B4 are hidden
 *               and do nothing while the light (or every light) is off
 *   BRIGHTNESS  B1 back, B2 down, B3 up (hold to repeat), in 5 % steps; 0 turns the light off.
 *               All lights start from the brightest light that's on, and the lights that are on
 *               are all set to the same value on the first change
 *   MODE        B1 back, B4 next mode, B2/B3 depend on the mode:
 *                 COLOUR       previous / next colour
 *                 DUAL_COLOUR  next colour 1 / next colour 2
 *                 EFFECT       previous / next effect
 *                 STATIC       unused
 *
 * Plain C++: no ESPHome, Arduino or display library. The platform passes in button events and the
 * time, and draws with render() when press(), release() or tick() return true (and on its own
 * refresh interval, so changes made elsewhere appear).
 * -------------------------------------------------------------------------------------------------
 */

// Named by corner: Arduino's binary.h defines B1, B10 ... as macros.
enum class Button : uint8_t {
  TOP_LEFT,      // B1
  BOTTOM_LEFT,   // B2
  BOTTOM_RIGHT,  // B3
  TOP_RIGHT,     // B4
};

enum class Screen : uint8_t { HOME, BRIGHTNESS, MODE };

class Ui {
 public:
  static constexpr uint8_t MAX_LIGHTS = 4;

  /** Add a light. Lights are listed in the order added. The port must outlive the Ui. */
  void add_light(LightPort *light);

  /** Return to HOME after this long without a button press. 0 disables it. Default 30 s. */
  void set_submenu_timeout(uint32_t ms) { this->submenu_timeout_ms_ = ms; }

  /** Button pressed / released. Returns true if the screen needs redrawing. */
  bool press(Button button, uint32_t now_ms);
  bool release(Button button, uint32_t now_ms);

  /** Hold-to-repeat and submenu timeout. Call every loop. Returns true if the screen needs redrawing. */
  bool tick(uint32_t now_ms);

  /** Draw the current screen onto a cleared canvas. */
  void render(Canvas &canvas) const;

  Screen screen() const { return this->screen_; }
  /** Selected entry: 0 is All lights when there are 2+ lights. */
  uint8_t selected() const { return this->selected_; }
  uint8_t entry_count() const;

 private:
  LightPort *selected_light() const;  // nullptr = All lights
  bool any_on() const;
  uint8_t brightness() const;
  void step_brightness(bool up);

  bool press_home(Button button);
  bool press_brightness(Button button, uint32_t now_ms);
  bool press_mode(Button button);

  void render_home(Canvas &canvas) const;
  void render_brightness(Canvas &canvas) const;
  void render_mode(Canvas &canvas) const;

  LightPort *lights_[MAX_LIGHTS]{};
  uint8_t light_count_{0};
  uint8_t selected_{0};
  Screen screen_{Screen::HOME};

  uint32_t submenu_timeout_ms_{30000};
  uint32_t last_input_ms_{0};

  int8_t held_{-1};  // button held for repeat, -1 = none
  uint32_t next_repeat_ms_{0};
};

}  // namespace lcui
