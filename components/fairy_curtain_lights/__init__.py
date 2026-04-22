import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output
from esphome.const import CONF_ID, CONF_FREQUENCY, CONF_OUTPUT_ID, CONF_PIN

fairy_ns = cg.esphome_ns.namespace("fairy_curtain_lights")
FairyCurtainLights = fairy_ns.class_("FairyCurtainLights", cg.Component)

FloatOutput = output.FloatOutput

CONF_GPIO_A = "gpio_a"
CONF_GPIO_B = "gpio_b"
CONF_ID_A = "id_a"
CONF_ID_B = "id_b"

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(FairyCurtainLights),
    cv.Required(CONF_GPIO_A): cv.int_,
    cv.Required(CONF_GPIO_B): cv.int_,
    cv.Optional(CONF_FREQUENCY, default=1000): cv.int_range(min=1),
    cv.Required(CONF_ID_A): cv.declare_id(output.FloatOutput),
    cv.Required(CONF_ID_B): cv.declare_id(output.FloatOutput),
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_GPIO_A],
        config[CONF_GPIO_B],
        config[CONF_FREQUENCY]
    )
    yield cg.register_component(var, config)

    # These are outputs exposed by the component
    yield output.register_output( var.get_channel_a(), { CONF_ID: config[CONF_ID_A] } )
    yield output.register_output( var.get_channel_b(), { CONF_ID: config[CONF_ID_B] } )

