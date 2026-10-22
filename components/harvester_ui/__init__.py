"""Built-in web app: setup, live status and settings, without Home Assistant."""
import gzip
from pathlib import Path

from esphome import automation
import esphome.codegen as cg
from esphome.components import web_server_base
from esphome.components.web_server_base import CONF_WEB_SERVER_BASE_ID
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PASSWORD, CONF_TRIGGER_ID, CONF_USERNAME

AUTO_LOAD = ['web_server_base', 'json']
DEPENDENCIES = ['network']

CONF_STATUS = 'status'
CONF_ON_COMMAND = 'on_command'
CONF_PAGE_ID = 'page_id'

harvester_ui_ns = cg.esphome_ns.namespace('harvester_ui')
HarvesterUI = harvester_ui_ns.class_('HarvesterUI', cg.Component)
CommandTrigger = harvester_ui_ns.class_(
    'CommandTrigger', automation.Trigger.template(cg.std_string, cg.JsonObjectConst))

PAGE = Path(__file__).parent / 'index.html'

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(HarvesterUI),
    cv.GenerateID(CONF_WEB_SERVER_BASE_ID): cv.use_id(web_server_base.WebServerBase),
    cv.GenerateID(CONF_PAGE_ID): cv.declare_id(cg.uint8),
    cv.Required(CONF_USERNAME): cv.string_strict,
    cv.Required(CONF_PASSWORD): cv.string_strict,
    cv.Required(CONF_STATUS): cv.lambda_,
    cv.Optional(CONF_ON_COMMAND): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(CommandTrigger),
    }),
}).extend(cv.COMPONENT_SCHEMA).add_extra(cv.only_on_esp32)


async def to_code(config):
    base = await cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])
    var = cg.new_Pvariable(config[CONF_ID], base)
    await cg.register_component(var, config)

    page = gzip.compress(PAGE.read_bytes(), compresslevel=9, mtime=0)
    data = cg.progmem_array(config[CONF_PAGE_ID], list(page))
    cg.add(var.set_page(data, len(page)))

    # The device's own pages use digest auth; the captive portal stays open.
    # Readings pushers may instead pass the password as `key`.
    cg.add_define('USE_WEBSERVER_AUTH')
    cg.add_define('USE_WEBSERVER_AUTH_DIGEST')
    cg.add(base.set_auth_username(config[CONF_USERNAME]))
    cg.add(base.set_auth_password(config[CONF_PASSWORD]))
    cg.add(var.set_credentials(config[CONF_USERNAME], config[CONF_PASSWORD]))

    status = await cg.process_lambda(
        config[CONF_STATUS], [(cg.JsonObject, 'root')], return_type=cg.void)
    cg.add(var.set_status(status))

    for conf in config.get(CONF_ON_COMMAND, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger, [(cg.std_string, 'command'), (cg.JsonObjectConst, 'args')], conf)
