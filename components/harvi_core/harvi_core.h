#pragma once
// Hardware-independent commissioning logic.
#include "esphome/core/defines.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace harvi {
constexpr uint32_t SERIAL = HARVI_SERIAL;
constexpr std::array<uint32_t, 8> FREQUENCIES = {
    869534852, 869555000, 869795000, 869901063,
    868095000, 868231667, 868368333, 868505000};

struct CT {
  bool present{false};
  uint16_t timing{0};
  int16_t real{0}, imaginary{0};
};
struct Packet {
  uint32_t serial{0};
  uint8_t sequence{0}, flags{0};
  bool pairing{false};
  std::array<CT, 3> ct{};
};

inline std::vector<uint8_t> encode_fifo(const Packet &p) {
  if (p.serial == 0 || p.serial > 0xffffff) return {};
  std::vector<uint8_t> bytes = {0, uint8_t(p.serial), 0xa5, uint8_t(p.pairing ? 0xc0 : 0x40),
      uint8_t(p.serial >> 8), uint8_t(p.serial >> 16), p.sequence, uint8_t(p.flags & 0x80)};
  for (size_t i = 0; i < 3; ++i) {
    const auto &ct = p.ct[i];
    if (!ct.present) continue;
    bytes[7] |= 1 << (i + 1);
    for (uint16_t value : {ct.timing, uint16_t(ct.real), uint16_t(ct.imaginary)}) {
      bytes.push_back(uint8_t(value));
      bytes.push_back(uint8_t(value >> 8));
    }
  }
  bytes[0] = bytes.size() - 1;
  for (size_t i = 1; i < bytes.size(); ++i) bytes[i] ^= 0x7f - i;
  return bytes;
}

inline bool decode_payload(const std::vector<uint8_t> &payload, Packet &result) {
  if (payload.size() < 7 || payload.size() > 63) return false;
  auto value = [&](size_t i) { return uint8_t(payload[i-1] ^ (0x7f-i)); };
  if (value(2) != 0xa5 || (value(3) & 0x7f) != 0x40) return false;
  Packet p;
  p.serial = value(1) | (uint32_t(value(4)) << 8) | (uint32_t(value(5)) << 16);
  if (!p.serial) return false;
  p.sequence = value(6); p.flags = value(7); p.pairing = value(3) & 0x80;
  size_t offset = 8;
  auto word = [&](size_t i) { return uint16_t(value(i) | (uint16_t(value(i+1)) << 8)); };
  for (size_t i = 0; i < 3; ++i) {
    if (!(p.flags & (1 << (i+1)))) continue;
    if (offset + 6 > payload.size() + 1) return false;
    p.ct[i] = {true, word(offset), int16_t(word(offset+2)), int16_t(word(offset+4))};
    offset += 6;
  }
  // Unknown trailing data remains logged by the raw logger, never auto-locks scan.
  if (offset != payload.size()+1) return false;
  result = p;
  return true;
}

class Pairing {
 public:
  static constexpr uint32_t WINDOW_MS = 120000, INTERVAL_MS = 2000;
  bool active{false};
  uint32_t sent{0}, failed{0};
  void start(uint32_t now) {
    active = true; started_ = now; last_ = now - INTERVAL_MS;
  }
  void stop() { active = false; }
  uint32_t seconds_left(uint32_t now) const {
    if (!active || uint32_t(now-started_) >= WINDOW_MS) return 0;
    return active ? (WINDOW_MS - uint32_t(now-started_) + 999)/1000 : 0;
  }
  std::vector<uint8_t> next(uint32_t now) {
    if (!seconds_left(now)) { stop(); return {}; }
    if (uint32_t(now-last_) < INTERVAL_MS) return {};
    last_ = now;
    Packet p; p.serial = SERIAL; p.sequence = sequence_++; p.pairing = true;
    auto fifo = encode_fifo(p);
    return {fifo.begin()+1, fifo.end()};  // SX127x adds the length itself.
  }
  void complete(bool ok) {
    if (ok) ++sent;
    else { ++failed; stop(); }
  }
 private:
  uint32_t started_{0}, last_{0};
  uint8_t sequence_{0};
};

class Scan {
 public:
  bool active{false}, found{false};
  uint32_t reference_serial{0};
  uint8_t index{0};
  void start(uint32_t now) { active=true; found=false; index=0; entered_=now; matches_=0; }
  void stop() { active=false; }
  // Return true when a retune is due. One finite 8 x 12-second sweep.
  bool advance(uint32_t now) {
    if (!active || uint32_t(now-entered_) < 12000) return false;
    if (index==7) { active=false; return false; }
    ++index; entered_=now; matches_=0; return true;
  }
  bool observe(const Packet &packet) {
    if (!active || reference_serial == 0 || packet.serial != reference_serial) return false;
    // Distinct sequence values: repeated cached/duplicate packets cannot lock it.
    if (matches_ && packet.sequence == sequence_) return false;
    sequence_=packet.sequence;
    if (++matches_ >= 2) { found=true; active=false; return true; }
    return false;
  }
 private:
  uint32_t entered_{0};
  uint8_t matches_{0}, sequence_{0};
};

inline bool valid_entity_id(const std::string &value) {
  if (value.empty()) return true;  // An unused input.
  if (value.size() <= 7 || value.compare(0, 7, "sensor.") != 0) return false;
  for (size_t i=7; i<value.size(); ++i) {
    const char c=value[i];
    if (!(c=='_' || (c>='a' && c<='z') || (c>='0' && c<='9'))) return false;
  }
  return true;
}

// Receipt-age diagnostics only; not proof of source measurement freshness.
class InputFreshness {
 public:
  void set_enabled(size_t channel, bool enabled) {
    if (channel >= 3) return;
    enabled_[channel] = enabled;
    valid_[channel] = false;
  }
  void observe(size_t channel, uint32_t now, float value) {
    if (channel >= 3) return;
    valid_[channel] = std::isfinite(value);
    updated_[channel] = now;
  }
  bool fresh(uint32_t now, uint32_t timeout_ms) const {
    bool any=false;
    for (size_t i=0; i<3; ++i) {
      if (!enabled_[i]) continue;
      any=true;
      if (!valid_[i] || uint32_t(now-updated_[i]) >= timeout_ms) return false;
    }
    return any;
  }
  void reset() { valid_.fill(false); }
 private:
  std::array<uint32_t, 3> updated_{};
  std::array<bool, 3> valid_{}, enabled_{};
};
}  // namespace harvi
