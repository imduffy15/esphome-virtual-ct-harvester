"""Configure the per-device virtual identity."""
import esphome.codegen as cg
import esphome.config_validation as cv

CONFIG_SCHEMA = cv.Schema({
    cv.Required('serial'): cv.int_range(min=1, max=0xFFFFFF),
})

async def to_code(config):
    cg.add_define('HARVI_SERIAL', config['serial'])
