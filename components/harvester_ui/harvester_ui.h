#pragma once
// The device's own web app. Requests arrive on the HTTP server task; status
// and commands run on the main loop, one at a time, while the request waits.
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/components/json/json_util.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include <atomic>
#include <functional>
#include <string>

namespace esphome::harvester_ui {

class HarvesterUI;

class CommandTrigger : public Trigger<std::string, JsonObjectConst> {
 public:
  explicit CommandTrigger(HarvesterUI *parent);
};

class Handler : public AsyncWebHandler {
 public:
  Handler(HarvesterUI *parent, bool push) : parent_(parent), push_(push) {}
  bool canHandle(AsyncWebServerRequest *request) const override;
  void handleRequest(AsyncWebServerRequest *request) override;
  void handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) override;

 protected:
  HarvesterUI *parent_;
  bool push_;
  std::string body_;
};

class HarvesterUI : public Component {
 public:
  explicit HarvesterUI(web_server_base::WebServerBase *base) : base_(base), ui_(this, false), push_(this, true) {}
  void set_page(const uint8_t *data, size_t size) { page_ = data; page_size_ = size; }
  void set_credentials(const char *username, const char *password) { username_ = username; password_ = password; }
  void set_status(std::function<void(JsonObject)> &&f) { status_ = std::move(f); }
  void add_command_trigger(CommandTrigger *trigger) { triggers_.push_back(trigger); }

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::WIFI - 1.0f; }

  // From an on_command automation: reject the command with a message for the user.
  void fail(const std::string &message) { error_ = message; }
  // From an on_command automation: tell the page the device is about to restart.
  void restarting() { restarting_ = true; }

 protected:
  friend class Handler;
  // HTTP task: run a command (or the status, when command is empty) on the
  // main loop and return its JSON response; false on timeout.
  bool run_(const std::string &command, std::string &&args, std::string &response);
  bool check_key_(AsyncWebServerRequest *request) const;
  void serve_page_(AsyncWebServerRequest *request);

  enum Job : uint8_t { IDLE, PENDING, RUNNING, DONE };
  web_server_base::WebServerBase *base_;
  Handler ui_, push_;
  const uint8_t *page_{nullptr};
  size_t page_size_{0};
  const char *username_{""}, *password_{""};
  std::function<void(JsonObject)> status_;
  std::vector<CommandTrigger *> triggers_;
  std::atomic<uint8_t> job_{IDLE};
  std::string command_, args_, response_, error_;
  bool restarting_{false};
};

}  // namespace esphome::harvester_ui
