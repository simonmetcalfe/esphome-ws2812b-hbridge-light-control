"""ESPHome external component for the light control OLED menu.

The UI itself is plain C++ (lcui_*.cpp/h, shared with the PlatformIO UI test build). This component
wires it to ESPHome: the display (the component becomes its writer, so no display lambda is
needed), two fonts, the four corner buttons, and the lights it controls.
"""

import esphome.codegen as cg
from esphome.components import binary_sensor, display, font, light, select
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TYPE

CODEOWNERS = ["@simonmetcalfe"]
DEPENDENCIES = ["display"]
AUTO_LOAD = ["oli_icons"]

CONF_DISPLAY_ID = "display_id"
CONF_FONT_SMALL = "font_small"
CONF_FONT_LARGE = "font_large"
CONF_BUTTONS = "buttons"
CONF_LIGHTS = "lights"
CONF_LIGHT_ID = "light_id"
CONF_COLOUR1_SELECT = "colour1_select"
CONF_COLOUR2_SELECT = "colour2_select"
CONF_DUAL_COLOUR_EFFECT = "dual_colour_effect"
CONF_HIDDEN_EFFECTS = "hidden_effects"
CONF_SUBMENU_TIMEOUT = "submenu_timeout"

TYPE_HBRIDGE = "hbridge"
TYPE_WS2812B = "ws2812b"

light_control_ui_ns = cg.esphome_ns.namespace("light_control_ui")
LightControlUi = light_control_ui_ns.class_("LightControlUi", cg.Component)
HBridgePort = light_control_ui_ns.class_("HBridgePort")
StripPort = light_control_ui_ns.class_("StripPort")

HBRIDGE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(HBridgePort),
        cv.Required(CONF_LIGHT_ID): cv.use_id(light.LightState),
        cv.Optional(CONF_HIDDEN_EFFECTS, default=[]): cv.ensure_list(cv.string),
    }
)

WS2812B_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(StripPort),
        cv.Required(CONF_LIGHT_ID): cv.use_id(light.LightState),
        cv.Required(CONF_COLOUR1_SELECT): cv.use_id(select.Select),
        cv.Required(CONF_COLOUR2_SELECT): cv.use_id(select.Select),
        cv.Optional(CONF_DUAL_COLOUR_EFFECT, default="Dual Colour"): cv.string,
        # Effects that are colours rather than animations; shown as 1 COLOUR, not in the effect list.
        cv.Optional(CONF_HIDDEN_EFFECTS, default=["Warm White"]): cv.ensure_list(
            cv.string
        ),
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(LightControlUi),
        cv.Required(CONF_DISPLAY_ID): cv.use_id(display.Display),
        cv.Required(CONF_FONT_SMALL): cv.use_id(font.Font),
        cv.Required(CONF_FONT_LARGE): cv.use_id(font.Font),
        # B1 top-left, B2 bottom-left, B3 bottom-right, B4 top-right
        cv.Required(CONF_BUTTONS): cv.All(
            cv.ensure_list(cv.use_id(binary_sensor.BinarySensor)),
            cv.Length(min=4, max=4),
        ),
        cv.Required(CONF_LIGHTS): cv.All(
            cv.ensure_list(
                cv.typed_schema(
                    {TYPE_HBRIDGE: HBRIDGE_SCHEMA, TYPE_WS2812B: WS2812B_SCHEMA},
                    lower=True,
                )
            ),
            cv.Length(min=1, max=4),
        ),
        cv.Optional(
            CONF_SUBMENU_TIMEOUT, default="30s"
        ): cv.positive_time_period_milliseconds,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_display(await cg.get_variable(config[CONF_DISPLAY_ID])))
    cg.add(
        var.set_fonts(
            await cg.get_variable(config[CONF_FONT_SMALL]),
            await cg.get_variable(config[CONF_FONT_LARGE]),
        )
    )
    for index, button_id in enumerate(config[CONF_BUTTONS]):
        cg.add(var.set_button(index, await cg.get_variable(button_id)))

    for conf in config[CONF_LIGHTS]:
        light_var = await cg.get_variable(conf[CONF_LIGHT_ID])
        if conf[CONF_TYPE] == TYPE_WS2812B:
            port = cg.new_Pvariable(
                conf[CONF_ID],
                light_var,
                await cg.get_variable(conf[CONF_COLOUR1_SELECT]),
                await cg.get_variable(conf[CONF_COLOUR2_SELECT]),
            )
            cg.add(port.set_dual_colour_effect(conf[CONF_DUAL_COLOUR_EFFECT]))
        else:
            port = cg.new_Pvariable(conf[CONF_ID], light_var)
        for name in conf[CONF_HIDDEN_EFFECTS]:
            cg.add(port.add_hidden_effect(name))
        cg.add(var.add_light(port))

    cg.add(var.set_submenu_timeout(config[CONF_SUBMENU_TIMEOUT]))
