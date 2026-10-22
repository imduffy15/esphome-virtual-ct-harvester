#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/sx127x/sx127x.h"

namespace esphome::sx127x {

template<typename... Ts> class RunImageCalAction final : public Action<Ts...>, public Parented<SX127x> {
 public:
  void play(const Ts &...x) override { this->parent_->run_image_cal(); }
};

template<typename... Ts> class SendPacketAction final : public Action<Ts...>, public Parented<SX127x> {
 public:
  void set_data_template(std::vector<uint8_t> (*func)(Ts...)) {
    this->data_.func = func;
    this->len_ = -1;  // A sentinel value marks template mode.
  }

  void set_data_static(const uint8_t *data, size_t len) {
    this->data_.data = data;
    this->len_ = len;  // A length of 0 or more marks static mode.
  }

  void play(const Ts &...x) override {
    std::vector<uint8_t> data;
    if (this->len_ >= 0) {
      // Static mode: copy the data from flash to a vector.
      data.assign(this->data_.data, this->data_.data + this->len_);
    } else {
      // Template mode: call the function.
      data = this->data_.func(x...);
    }
    this->parent_->transmit_packet(data);
  }

 protected:
  ssize_t len_{-1};  // -1 for template mode, or the data length (0 or more) for static mode
  union Data {
    std::vector<uint8_t> (*func)(Ts...);  // Function pointer for stateless lambdas
    const uint8_t *data;                  // Pointer to static data in flash
  } data_;
};

template<typename... Ts> class SetModeTxAction final : public Action<Ts...>, public Parented<SX127x> {
 public:
  void play(const Ts &...x) override { this->parent_->set_mode_tx(); }
};

template<typename... Ts> class SetModeRxAction final : public Action<Ts...>, public Parented<SX127x> {
 public:
  void play(const Ts &...x) override { this->parent_->set_mode_rx(); }
};

template<typename... Ts> class SetModeSleepAction final : public Action<Ts...>, public Parented<SX127x> {
 public:
  void play(const Ts &...x) override { this->parent_->set_mode_sleep(); }
};

template<typename... Ts> class SetModeStandbyAction final : public Action<Ts...>, public Parented<SX127x> {
 public:
  void play(const Ts &...x) override { this->parent_->set_mode_standby(); }
};

}  // namespace esphome::sx127x
