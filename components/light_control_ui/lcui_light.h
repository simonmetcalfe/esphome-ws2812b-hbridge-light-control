#pragma once

#include <stdint.h>

namespace lcui {

/**
 * -------------------------------------------------------------------------------------------------
 * LIGHT PORT
 *
 * One light as the UI sees it: a name, what it can do, and its current state. The UI never sees
 * pins or hardware types; it works out the available modes from has_colour().
 *
 *   Colour lights (WS2812B):  COLOUR -> DUAL_COLOUR -> EFFECT
 *   Other lights (H-bridge):  STATIC -> EFFECT
 *
 * The UI reads state through the port on every frame instead of keeping a copy, so changes made
 * elsewhere (e.g. Home Assistant) show up on the next frame.
 *
 * Implementations:
 *   - ESPHome:  HBridgePort / StripPort on a light::LightState (light_control_ui.h)
 *   - UI test firmware and host tests:  FakeLight, state held in RAM (lcui_fake_light.h)
 * -------------------------------------------------------------------------------------------------
 */

enum class Mode : uint8_t {
  STATIC,       // no effect (lights without colour)
  EFFECT,       // a named effect
  COLOUR,       // one colour from the colour list
  DUAL_COLOUR,  // two colours from the colour list
};

class LightPort {
 public:
  virtual ~LightPort() = default;

  virtual const char *name() const = 0;
  virtual bool has_colour() const = 0;

  virtual bool is_on() const = 0;
  virtual void set_on(bool on) = 0;

  /** Brightness 0-100 %. Kept while the light is off. */
  virtual uint8_t brightness() const = 0;
  virtual void set_brightness(uint8_t percent) = 0;

  virtual Mode mode() const = 0;
  /** Switch mode (turns the light on). EFFECT resumes the last effect used. */
  virtual void set_mode(Mode mode) = 0;

  virtual uint8_t effect_count() const = 0;
  virtual const char *effect_name(uint8_t index) const = 0;
  /** The running effect, or the one EFFECT mode would resume. -1 if there are no effects. */
  virtual int effect_index() const = 0;
  /** Start an effect (turns the light on and switches to EFFECT mode). */
  virtual void set_effect(uint8_t index) = 0;

  // Colour lights only. slot 0 = colour 1, slot 1 = colour 2 (DUAL_COLOUR).
  virtual uint8_t colour_count() const { return 0; }
  virtual const char *colour_name(uint8_t index) const { return ""; }
  /** Index into the colour list, or -1 if the colour isn't one of them. */
  virtual int colour_index(uint8_t slot) const { return -1; }
  virtual void set_colour(uint8_t slot, uint8_t index) {}
};

}  // namespace lcui
