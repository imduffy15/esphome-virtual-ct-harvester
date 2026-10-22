#include "harvester_ui.h"
#include "esphome/core/log.h"
#ifdef USE_CAPTIVE_PORTAL
#include "esphome/components/captive_portal/captive_portal.h"
#endif
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>

namespace esphome::harvester_ui {

static const char *const TAG = "harvester_ui";
static constexpr size_t MAX_BODY = 2048;
static constexpr uint32_t JOB_TIMEOUT_MS = 3000;

CommandTrigger::CommandTrigger(HarvesterUI *parent) { parent->add_command_trigger(this); }

static bool captive_portal_active() {
#ifdef USE_CAPTIVE_PORTAL
  return captive_portal::global_captive_portal != nullptr && captive_portal::global_captive_portal->is_active();
#else
  return false;
#endif
}

bool Handler::canHandle(AsyncWebServerRequest *request) const {
  if (captive_portal_active()) return false;  // Wi-Fi onboarding owns the server
  char buf[AsyncWebServerRequest::URL_BUF_SIZE];
  const StringRef url = request->url_to(buf);
  const bool power = url == "/api/power";
  if (this->push_) return power;
  return !power && (url == "/" || url == "/index.html" || strncmp(url.c_str(), "/api/", 5) == 0);
}

void Handler::handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  if (index == 0) this->body_.clear();
  if (total > MAX_BODY) return;
  this->body_.append(reinterpret_cast<const char *>(data), len);
}

static void send_json(AsyncWebServerRequest *request, int code, const std::string &json) {
  auto *response = request->beginResponse(code, "application/json", json);
  response->addHeader("Cache-Control", "no-store");
  request->send(response);
}

static void send_error(AsyncWebServerRequest *request, int code, const char *message) {
  send_json(request, code, json::build_json([message](JsonObject root) {
              root["ok"] = false;
              root["error"] = message;
            }));
}

// Browsers send Origin on cross-site POSTs, and may attach cached digest
// credentials to them; scripts and devices send none.
static bool same_origin(AsyncWebServerRequest *request) {
  const auto origin = request->get_header("Origin");
  if (!origin.has_value()) return true;
  const auto host = request->get_header("Host");
  return host.has_value() && *origin == "http://" + *host;
}

void Handler::handleRequest(AsyncWebServerRequest *request) {
  std::string body = std::move(this->body_);
  this->body_.clear();
  if (body.size() > MAX_BODY) return send_error(request, 413, "Request too large");
  if (request->method() != HTTP_GET && !same_origin(request)) return send_error(request, 403, "Cross-site request");
  char buf[AsyncWebServerRequest::URL_BUF_SIZE];
  const std::string url = request->url_to(buf).str();
  std::string response;

  if (this->push_) {
    // Readings from another device or script. Form, query or JSON fields
    // grid/solar/battery in W. Authenticated by digest or the `key` field.
    // Digest only on POST: a cross-site GET (an <img>, say) can carry a
    // browser's cached credentials but never the key.
    if (!this->parent_->check_key_(request)) {
      if (request->method() != HTTP_POST) return send_error(request, 401, "Pass the device password as key");
      if (!request->authenticate(this->parent_->username_, this->parent_->password_))
        return request->requestAuthentication();
    }
    if (body.empty()) {
      body = json::build_json([request](JsonObject root) {
        for (const char *name : {"grid", "solar", "battery"}) {
          if (!request->hasArg(name)) continue;
          char *end = nullptr;
          const std::string text = request->arg(name);
          const float value = strtof(text.c_str(), &end);
          if (end != text.c_str() && *end == 0) root[name] = value; else root[name] = text;
        }
      });
    }
    if (!this->parent_->run_("power", std::move(body), response))
      return send_error(request, 503, "Device busy; try again");
    return send_json(request, 200, response);
  }

  if (url == "/" || url == "/index.html") {
    if (request->method() != HTTP_GET) return send_error(request, 405, "Method not allowed");
    return this->parent_->serve_page_(request);
  }
  if (url == "/api/status") {
    if (!this->parent_->run_("", {}, response)) return send_error(request, 503, "Device busy; try again");
    return send_json(request, 200, response);
  }
  if (request->method() != HTTP_POST) return send_error(request, 405, "Use POST");
  if (!this->parent_->run_(url.substr(5), std::move(body), response))
    return send_error(request, 503, "Device busy; try again");
  send_json(request, 200, response);
}

bool HarvesterUI::check_key_(AsyncWebServerRequest *request) const {
  if (!request->hasArg("key")) return false;
  const std::string key = request->arg("key");
  const size_t n = strlen(this->password_);
  // Constant time over the expected length; a length mismatch also fails.
  volatile uint8_t diff = key.size() != n;
  for (size_t i = 0; i < n; ++i) diff |= uint8_t(this->password_[i] ^ (i < key.size() ? key[i] : 0));
  return diff == 0;
}

void HarvesterUI::serve_page_(AsyncWebServerRequest *request) {
  auto *response = request->beginResponse(200, "text/html", this->page_, this->page_size_);
  response->addHeader("Content-Encoding", "gzip");
  response->addHeader("Cache-Control", "no-cache");
  request->send(response);
}

bool HarvesterUI::run_(const std::string &command, std::string &&args, std::string &response) {
  // The HTTP server handles one request at a time, so the job slot is free
  // unless an earlier request timed out while its job was running.
  for (uint32_t waited = 0; this->job_.load() != IDLE; waited += 10) {
    if (waited >= JOB_TIMEOUT_MS) return false;
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  this->command_ = command;
  this->args_ = std::move(args);
  this->job_.store(PENDING);
  for (uint32_t waited = 0; this->job_.load() != DONE; waited += 5) {
    if (waited >= JOB_TIMEOUT_MS) {
      uint8_t expected = PENDING;
      if (this->job_.compare_exchange_strong(expected, IDLE)) return false;
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
  response = std::move(this->response_);
  this->job_.store(IDLE);
  return true;
}

void HarvesterUI::setup() {
  this->base_->init();
  this->base_->add_handler(&this->ui_);
  this->base_->add_handler_without_auth(&this->push_);
}

void HarvesterUI::loop() {
  uint8_t expected = PENDING;
  if (!this->job_.compare_exchange_strong(expected, RUNNING)) return;
  if (this->command_.empty()) {
    this->response_ = json::build_json([this](JsonObject root) { this->status_(root); });
  } else {
    this->error_.clear();
    this->restarting_ = false;
    bool known = false;
    JsonDocument doc = json::parse_json(this->args_.empty() ? std::string("{}") : this->args_);
    if (doc.isNull() || !doc.is<JsonObject>()) {
      this->error_ = "Expected a JSON object";
    } else {
      for (auto *trigger : this->triggers_) {
        trigger->trigger(this->command_, doc.as<JsonObjectConst>());
        known = true;
      }
    }
    if (!known && this->error_.empty()) this->error_ = "Unknown command";
    this->response_ = json::build_json([this](JsonObject root) {
      root["ok"] = this->error_.empty();
      if (!this->error_.empty()) root["error"] = this->error_;
      if (this->restarting_) root["restarting"] = true;
    });
    ESP_LOGD(TAG, "Command %s: %s", this->command_.c_str(), this->error_.empty() ? "ok" : this->error_.c_str());
  }
  this->job_.store(DONE);
}

void HarvesterUI::dump_config() { ESP_LOGCONFIG(TAG, "Harvester web app on port %u", this->base_->get_port()); }

}  // namespace esphome::harvester_ui
