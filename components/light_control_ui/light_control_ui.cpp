#include "light_control_ui.h"

#include <cmath>
#include <strings.h>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::light_control_ui {

static const char *const TAG = "light_control_ui";

/**
 * -------------------------------------------------------------------------------------------------
 * DISPLAY CANVAS
 * -------------------------------------------------------------------------------------------------
 */

void DisplayCanvas::text(int x, int baseline, lcui::Font font, const char *str) {
  this->it_.print(x, baseline, this->font_(font), display::COLOR_ON, display::TextAlign::BASELINE_LEFT, str);
}

int DisplayCanvas::text_width(lcui::Font font, const char *str) {
  int x1, y1, width, height;
  this->it_.get_text_bounds(0, 0, str, this->font_(font), display::TextAlign::BASELINE_LEFT, &x1, &y1, &width,
                            &height);
  return width;
}

void DisplayCanvas::bitmap(int x, int y, const uint8_t *bmp, int w, int h) {
  const int bytes_per_row = (w + 7) / 8;
  for (int row = 0; row < h; row++) {
    for (int col = 0; col < w; col++) {
      if (bmp[row * bytes_per_row + col / 8] & (0x80 >> (col % 8)))
        this->it_.draw_pixel_at(x + col, y + row, display::COLOR_ON);
    }
  }
}

void DisplayCanvas::fill_rect(int x, int y, int w, int h, bool on) {
  this->it_.filled_rectangle(x, y, w, h, on ? display::COLOR_ON : display::COLOR_OFF);
}

void DisplayCanvas::rect(int x, int y, int w, int h) { this->it_.rectangle(x, y, w, h, display::COLOR_ON); }

/**
 * -------------------------------------------------------------------------------------------------
 * LIGHT PORTS
 * -------------------------------------------------------------------------------------------------
 */

void LightPortBase::setup() {
  this->effects_.clear();
  const size_t count = this->light_->get_effect_count();
  for (uint32_t number = 1; number <= count; number++) {
    std::string name = this->light_->get_effect_name_by_index(number);
    bool hidden = false;
    for (const auto &h : this->hidden_effects_) {
      if (strcasecmp(h.c_str(), name.c_str()) == 0) hidden = true;
    }
    if (!hidden) this->effects_.push_back(number);
  }
  int running = this->running_effect_();
  if (running >= 0) this->last_effect_ = running;
}

bool LightPortBase::is_on() const { return this->light_->remote_values.is_on(); }

void LightPortBase::set_on(bool on) {
  auto call = this->light_->make_call();
  call.set_state(on);
  call.perform();
}

uint8_t LightPortBase::brightness() const {
  return (uint8_t) lroundf(this->light_->remote_values.get_brightness() * 100.0f);
}

/**
 * As Home Assistant sends it: brightness alone while on, turn on with brightness while off, and turn
 * off for 0. (A brightness of 0 would also turn the light off, but ESPHome then resets it to 100 %.)
 */
void LightPortBase::set_brightness(uint8_t percent) {
  auto call = this->light_->make_call();
  if (percent == 0) {
    call.set_state(false);
  } else {
    if (!this->is_on()) call.set_state(true);
    call.set_brightness(percent / 100.0f);
  }
  call.perform();
}

const char *LightPortBase::effect_name(uint8_t index) const {
  if (index >= this->effects_.size()) return "";
  return this->light_->get_effects()[this->effects_[index] - 1]->get_name().c_str();
}

int LightPortBase::effect_index() const {
  if (this->effects_.empty()) return -1;
  int running = this->running_effect_();
  return running >= 0 ? running : this->last_effect_;
}

void LightPortBase::set_effect(uint8_t index) {
  if (index >= this->effects_.size()) return;
  this->last_effect_ = index;
  this->start_effect_(this->effects_[index]);
}

uint32_t LightPortBase::find_effect_(const std::string &name) const {
  const size_t count = this->light_->get_effect_count();
  for (uint32_t number = 1; number <= count; number++) {
    if (strcasecmp(this->light_->get_effect_name_by_index(number).c_str(), name.c_str()) == 0) return number;
  }
  return 0;
}

int LightPortBase::running_effect_() const {
  uint32_t current = this->light_->get_current_effect_index();
  for (size_t i = 0; i < this->effects_.size(); i++) {
    if (this->effects_[i] == current) return (int) i;
  }
  return -1;
}

void LightPortBase::start_effect_(uint32_t effect_number) {
  auto call = this->light_->make_call();
  call.set_state(true);
  call.set_effect(effect_number);
  call.perform();
}

lcui::Mode HBridgePort::mode() const {
  return this->running_effect_() >= 0 ? lcui::Mode::EFFECT : lcui::Mode::STATIC;
}

void HBridgePort::set_mode(lcui::Mode mode) {
  if (mode == lcui::Mode::EFFECT && !this->effects_.empty()) {
    this->set_effect((uint8_t) this->last_effect_);
  } else {
    this->start_effect_(0);
  }
}

void StripPort::setup() {
  this->dual_colour_effect_ = this->find_effect_(this->dual_colour_effect_name_);
  if (this->dual_colour_effect_ == 0) {
    ESP_LOGW(TAG, "'%s' has no effect '%s'; 2 COLOUR mode won't work", this->name(),
             this->dual_colour_effect_name_.c_str());
  }
  // The dual colour effect is a mode of its own, not an entry in the EFFECT list.
  this->add_hidden_effect(this->dual_colour_effect_name_);
  LightPortBase::setup();
}

lcui::Mode StripPort::mode() const {
  uint32_t current = this->light_->get_current_effect_index();
  if (current != 0 && current == this->dual_colour_effect_) return lcui::Mode::DUAL_COLOUR;
  if (this->running_effect_() >= 0) return lcui::Mode::EFFECT;
  return lcui::Mode::COLOUR;  // no effect, or a hidden one such as "Warm White"
}

/**
 * Switch by effect, as choosing an effect in Home Assistant does; the strip's on_state handler
 * (capture_strip_control in balconylights.yaml) then sets display_mode and re-applies the colours.
 */
void StripPort::set_mode(lcui::Mode mode) {
  switch (mode) {
    case lcui::Mode::DUAL_COLOUR:
      if (this->dual_colour_effect_ != 0) this->start_effect_(this->dual_colour_effect_);
      break;
    case lcui::Mode::EFFECT:
      if (!this->effects_.empty()) this->set_effect((uint8_t) this->last_effect_);
      break;
    case lcui::Mode::COLOUR:
    case lcui::Mode::STATIC:
      this->start_effect_(0);
      break;
  }
}

uint8_t StripPort::colour_count() const { return (uint8_t) this->colour_selects_[0]->size(); }

const char *StripPort::colour_name(uint8_t index) const {
  if (index >= this->colour_selects_[0]->size()) return "";
  return this->colour_selects_[0]->option_at(index);
}

/**
 * The preset select's current option. A colour picked on the colour wheel in Home Assistant
 * doesn't change the select, so it isn't detected yet.
 */
int StripPort::colour_index(uint8_t slot) const {
  if (slot > 1) return -1;
  auto index = this->colour_selects_[slot]->active_index();
  return index.has_value() ? (int) index.value() : -1;
}

/** Sets the preset select; its set_action stores the colour and re-applies the strip. */
void StripPort::set_colour(uint8_t slot, uint8_t index) {
  if (slot > 1 || index >= this->colour_selects_[slot]->size()) return;
  auto call = this->colour_selects_[slot]->make_call();
  call.set_index(index);
  call.perform();
}

/**
 * -------------------------------------------------------------------------------------------------
 * COMPONENT
 * -------------------------------------------------------------------------------------------------
 */

void LightControlUi::setup() {
  for (auto *light : this->lights_) {
    light->setup();
    this->ui_.add_light(light);
  }

  for (uint8_t i = 0; i < 4; i++) {
    if (this->buttons_[i] == nullptr) continue;
    this->buttons_[i]->add_on_state_callback([this, i](bool state) {
      auto button = (lcui::Button) i;
      bool changed = state ? this->ui_.press(button, millis()) : this->ui_.release(button, millis());
      if (changed) this->redraw_ = true;
    });
  }

  this->display_->set_writer([this](display::Display &it) {
    DisplayCanvas canvas(it, this->font_small_, this->font_large_);
    this->ui_.render(canvas);
  });
  this->redraw_ = true;
}

void LightControlUi::loop() {
  if (this->ui_.tick(millis())) this->redraw_ = true;
  if (this->redraw_) {
    this->redraw_ = false;
    this->display_->update();
  }
}

void LightControlUi::dump_config() {
  ESP_LOGCONFIG(TAG, "Light control UI:");
  for (auto *light : this->lights_) {
    ESP_LOGCONFIG(TAG, "  %s: %s, %u effects", light->name(), light->has_colour() ? "colour" : "no colour",
                  light->effect_count());
  }
}

}  // namespace esphome::light_control_ui
