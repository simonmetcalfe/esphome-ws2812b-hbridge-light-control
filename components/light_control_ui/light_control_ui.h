#pragma once

#include <string>
#include <vector>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/display/display.h"
#include "esphome/components/light/light_state.h"
#include "esphome/components/select/select.h"
#include "esphome/core/component.h"

#include "lcui_ui.h"

namespace esphome::light_control_ui {

/**
 * -------------------------------------------------------------------------------------------------
 * ESPHOME ADAPTER
 *
 * Connects the plain C++ UI (lcui::Ui) to ESPHome:
 *   - buttons:  the four binary sensors' state callbacks call Ui::press() / Ui::release()
 *   - drawing:  the component becomes the display's writer and draws through DisplayCanvas
 *   - lights:   HBridgePort / StripPort read and change existing light entities the same way Home
 *               Assistant does (light calls, the colour preset selects), so the YAML's own
 *               automations keep working
 * -------------------------------------------------------------------------------------------------
 */

/** lcui::Canvas on an ESPHome display, with the two fonts from YAML. */
class DisplayCanvas : public lcui::Canvas {
 public:
  DisplayCanvas(display::Display &it, display::BaseFont *small, display::BaseFont *large)
      : it_(it), small_(small), large_(large) {}

  void text(int x, int baseline, lcui::Font font, const char *str) override;
  int text_width(lcui::Font font, const char *str) override;
  void bitmap(int x, int y, const uint8_t *bmp, int w, int h) override;
  void fill_rect(int x, int y, int w, int h, bool on) override;
  void rect(int x, int y, int w, int h) override;

 protected:
  display::BaseFont *font_(lcui::Font font) const { return font == lcui::Font::LARGE ? this->large_ : this->small_; }

  display::Display &it_;
  display::BaseFont *small_;
  display::BaseFont *large_;
};

/** Behaviour shared by both light types: on/off, brightness and the effect list. */
class LightPortBase : public lcui::LightPort {
 public:
  explicit LightPortBase(light::LightState *light) : light_(light) {}

  /** Effects left out of the EFFECT list (e.g. ones the WS2812B uses for colour modes). */
  void add_hidden_effect(const std::string &name) { this->hidden_effects_.push_back(name); }
  /** Build the effect list. Call once, after codegen has added the effects (Component::setup). */
  virtual void setup();

  const char *name() const override { return this->light_->get_name().c_str(); }
  bool is_on() const override;
  void set_on(bool on) override;
  uint8_t brightness() const override;
  void set_brightness(uint8_t percent) override;

  uint8_t effect_count() const override { return (uint8_t) this->effects_.size(); }
  const char *effect_name(uint8_t index) const override;
  int effect_index() const override;
  void set_effect(uint8_t index) override;

 protected:
  /** ESPHome effect number (1-based, 0 = none) for a name, or 0 if the light has no such effect. */
  uint32_t find_effect_(const std::string &name) const;
  /** Position of the running effect in effects_, or -1 if none of them is running. */
  int running_effect_() const;
  /** Turn on with an ESPHome effect number (0 = none). */
  void start_effect_(uint32_t effect_number);

  light::LightState *light_;
  std::vector<std::string> hidden_effects_;
  std::vector<uint32_t> effects_;  // ESPHome effect numbers shown in the EFFECT list
  int last_effect_{0};             // index into effects_ that EFFECT mode resumes
};

/** H-bridge (2-wire) fairy lights: STATIC (no effect) or EFFECT. */
class HBridgePort : public LightPortBase {
 public:
  using LightPortBase::LightPortBase;

  bool has_colour() const override { return false; }
  lcui::Mode mode() const override;
  void set_mode(lcui::Mode mode) override;
};

/**
 * WS2812B strip with colour presets, as in balconylights.yaml:
 *   COLOUR       effect "None" (or a hidden effect such as "Warm White"); colour 1 = colour1_select
 *   DUAL_COLOUR  the dual colour effect; colour 1/2 = colour1_select / colour2_select
 *   EFFECT       any other effect
 * Colours are changed by setting the selects, whose set_action applies them to the strip.
 */
class StripPort : public LightPortBase {
 public:
  StripPort(light::LightState *light, select::Select *colour1, select::Select *colour2)
      : LightPortBase(light), colour_selects_{colour1, colour2} {}

  void set_dual_colour_effect(const std::string &name) { this->dual_colour_effect_name_ = name; }
  void setup() override;

  bool has_colour() const override { return true; }
  lcui::Mode mode() const override;
  void set_mode(lcui::Mode mode) override;

  uint8_t colour_count() const override;
  const char *colour_name(uint8_t index) const override;
  int colour_index(uint8_t slot) const override;
  void set_colour(uint8_t slot, uint8_t index) override;

 protected:
  select::Select *colour_selects_[2];
  std::string dual_colour_effect_name_{"Dual Colour"};
  uint32_t dual_colour_effect_{0};
};

class LightControlUi : public Component {
 public:
  void set_display(display::Display *display) { this->display_ = display; }
  void set_fonts(display::BaseFont *small, display::BaseFont *large) {
    this->font_small_ = small;
    this->font_large_ = large;
  }
  /** index 0-3 = B1 (top-left), B2 (bottom-left), B3 (bottom-right), B4 (top-right). */
  void set_button(uint8_t index, binary_sensor::BinarySensor *button) { this->buttons_[index] = button; }
  void add_light(LightPortBase *light) { this->lights_.push_back(light); }
  void set_submenu_timeout(uint32_t ms) { this->ui_.set_submenu_timeout(ms); }

  void setup() override;
  void loop() override;
  void dump_config() override;
  // After the display and lights, so the effects are registered and the writer isn't replaced.
  float get_setup_priority() const override { return setup_priority::LATE; }

 protected:
  lcui::Ui ui_;
  display::Display *display_{nullptr};
  display::BaseFont *font_small_{nullptr};
  display::BaseFont *font_large_{nullptr};
  binary_sensor::BinarySensor *buttons_[4]{};
  std::vector<LightPortBase *> lights_;
  bool redraw_{false};
};

}  // namespace esphome::light_control_ui
