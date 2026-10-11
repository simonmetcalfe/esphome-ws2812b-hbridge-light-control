#pragma once

#include <stdint.h>

#include "lcui_light.h"

namespace lcui {

/**
 * -------------------------------------------------------------------------------------------------
 * FAKE LIGHT
 *
 * A light whose state lives in RAM, for the UI test firmware and the host tests. It behaves like the
 * ESPHome ports: mode, effect and colour changes turn the light on, and EFFECT mode resumes the
 * last effect. Set FakeLight::log to print each change (e.g. to Serial in the UI test firmware).
 *
 * Pass colours to make a colour (WS2812B-style) light; without them it's an H-bridge-style light.
 * The name, effect and colour arrays must outlive the light.
 * -------------------------------------------------------------------------------------------------
 */

class FakeLight : public LightPort {
 public:
  using LogFn = void (*)(const FakeLight &light, const char *change);
  static inline LogFn log = nullptr;

  FakeLight(const char *name, const char *const *effects, uint8_t effect_count,
            const char *const *colours = nullptr, uint8_t colour_count = 0)
      : name_(name),
        effects_(effects),
        effect_count_(effect_count),
        colours_(colours),
        colour_count_(colour_count),
        mode_(colour_count > 0 ? Mode::COLOUR : Mode::STATIC),
        colour_{0, (int16_t) (colour_count > 1 ? 1 : 0)} {}

  const char *name() const override { return this->name_; }
  bool has_colour() const override { return this->colour_count_ > 0; }

  bool is_on() const override { return this->on_; }
  void set_on(bool on) override {
    this->on_ = on;
    this->changed_("on/off");
  }

  uint8_t brightness() const override { return this->brightness_; }
  void set_brightness(uint8_t percent) override {
    this->brightness_ = percent > 100 ? 100 : percent;
    this->changed_("brightness");
  }

  Mode mode() const override { return this->mode_; }
  void set_mode(Mode mode) override {
    this->on_ = true;
    this->mode_ = mode;
    this->changed_("mode");
  }

  uint8_t effect_count() const override { return this->effect_count_; }
  const char *effect_name(uint8_t index) const override { return index < this->effect_count_ ? this->effects_[index] : ""; }
  int effect_index() const override { return this->effect_count_ > 0 ? this->effect_ : -1; }
  void set_effect(uint8_t index) override {
    if (index >= this->effect_count_) return;
    this->on_ = true;
    this->effect_ = index;
    this->mode_ = Mode::EFFECT;
    this->changed_("effect");
  }

  uint8_t colour_count() const override { return this->colour_count_; }
  const char *colour_name(uint8_t index) const override { return index < this->colour_count_ ? this->colours_[index] : ""; }
  int colour_index(uint8_t slot) const override { return slot < 2 && this->colour_count_ > 0 ? this->colour_[slot] : -1; }
  void set_colour(uint8_t slot, uint8_t index) override {
    if (slot > 1 || index >= this->colour_count_) return;
    this->on_ = true;
    this->colour_[slot] = (int16_t) index;
    // As in the ESPHome config: colour 2 implies two-colour mode; colour 1 keeps two-colour mode.
    if (slot == 1) this->mode_ = Mode::DUAL_COLOUR;
    else if (this->mode_ != Mode::DUAL_COLOUR) this->mode_ = Mode::COLOUR;
    this->changed_("colour");
  }

  /** Test helper: a colour that isn't in the list (e.g. picked in Home Assistant). */
  void set_custom_colour(uint8_t slot) {
    if (slot < 2) this->colour_[slot] = -1;
  }

 private:
  void changed_(const char *change) const {
    if (log != nullptr) log(*this, change);
  }

  const char *name_;
  const char *const *effects_;
  uint8_t effect_count_;
  const char *const *colours_;
  uint8_t colour_count_;

  bool on_{true};
  uint8_t brightness_{60};
  Mode mode_;
  uint8_t effect_{0};
  int16_t colour_[2];
};

}  // namespace lcui
