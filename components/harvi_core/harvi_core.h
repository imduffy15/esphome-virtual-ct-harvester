#pragma once
// Hardware-independent commissioning and virtual CT logic.
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

// Zappi status broadcasts heard over RF (the same records as its LAN
// multicast). Offsets were validated against captures on 2026-10-21. Byte 1 is
// a per-installation device byte. Neighboring myenergi systems share the
// channel, so use only frames that carry this installation's Zappi byte. The
// byte is learned from a 0x3730 record that lists this virtual Harvi's serial.
struct ZappiStatus {
  uint8_t device{0};
  uint16_t type{0};
  uint32_t zappi_serial{0};                  // 0x3730
  int32_t grid_w{0}, generation_w{0};        // 0x3601
  uint16_t voltage_dv{0}, frequency_chz{0};  // 0x5a5a
  uint32_t harvi_serial{0};                  // 0x3730
  std::array<int16_t, 3> ct_power_w{}, ct_current_ca{};
};

inline std::vector<uint8_t> unmask(const std::vector<uint8_t> &payload) {
  std::vector<uint8_t> u(payload.size() + 1);
  u[0] = payload.size();
  for (size_t i = 1; i < u.size(); ++i) u[i] = payload[i - 1] ^ (0x7f - i);
  return u;
}

inline bool parse_zappi_status(const std::vector<uint8_t> &payload, ZappiStatus &s) {
  if (payload.size() < 8 || payload.size() > 63) return false;
  const auto u = unmask(payload);
  auto u16 = [&](size_t i) { return uint16_t(u[i] | (u[i + 1] << 8)); };
  auto u32 = [&](size_t i) { return uint32_t(u16(i)) | (uint32_t(u16(i + 2)) << 16); };
  s = ZappiStatus{};
  s.device = u[1];
  s.type = u16(2);
  if (s.type == 0x3601 && u.size() >= 14) {
    s.grid_w = int32_t(u32(6));
    s.generation_w = int32_t(u32(10));
    return true;
  }
  if (s.type == 0x5a5a && u.size() >= 44) {
    s.voltage_dv = u16(36);
    s.frequency_chz = u16(42);
    return s.voltage_dv > 1000 && s.frequency_chz > 4000;
  }
  if (s.type == 0x3730 && u.size() >= 38) {
    s.zappi_serial = u32(6);
    s.harvi_serial = u32(10);
    for (size_t i = 0; i < 3; ++i) {
      s.ct_power_w[i] = int16_t(u16(18 + 8 * i));
      s.ct_current_ca[i] = int16_t(u16(20 + 8 * i));
    }
    return true;
  }
  return false;
}

constexpr float RECEIVER_SCALE = 1.19f;  // Zappi: current field = |phasor| / 1.19

// Zappi computes P = V_dV * floor(|phasor| / 1.19) * cos(angle) / 1000.
// Returns the phasor magnitude for `watts` at voltage_dv, inflated by gain.
inline int32_t phasor_magnitude(float watts, uint16_t voltage_dv, float gain = 1.0f) {
  if (!std::isfinite(watts) || voltage_dv == 0) return -1;
  const float m = std::fabs(watts) * 1000.0f * RECEIVER_SCALE * gain / voltage_dv;
  return m > 20000.0f ? -1 : int32_t(std::lround(m));
}
// The power magnitude the Zappi will compute for a given phasor magnitude.
inline float reported_magnitude_w(int32_t magnitude, uint16_t voltage_dv) {
  return float(voltage_dv) * float(int32_t(magnitude / RECEIVER_SCALE)) / 1000.0f;
}

inline CT rotated_ct(int32_t magnitude, double angle_deg) {
  const double a = angle_deg * M_PI / 180.0;
  return {true, 1000, int16_t(std::lround(magnitude * std::cos(a))),
          int16_t(std::lround(magnitude * std::sin(a)))};
}

// Model: the Zappi reports r = M cos(a - phi(t)), where a is the sent phasor
// angle, M is the magnitude the Zappi computes, and
// phi(t) = phi0 + 360 * F * (t - t_ref) is its mains phase in this MCU's
// microsecond clock. phi0 absorbs the constant radio latency and the Zappi's
// fixed correction. The fit is a grid search over F, with linear least squares
// for phi0 at each candidate.
class PhaseTracker {
 public:
  struct Obs { uint32_t t_us; float angle_deg, magnitude_w, reported_w; };
  struct Fit {
    bool valid{false};
    double frequency_hz{0}, phase_deg{0};  // phase at t_ref
    uint32_t t_ref{0};
    float gain{0}, rms_w{0}, rms_rel{0};
    uint8_t n{0};
  };
  static constexpr size_t MAX_OBS = 24;
  static constexpr uint32_t WINDOW_US = 30000000;
  void reset() { count_ = 0; fit_ = Fit{}; tracking_ = false; }
  void add(const Obs &o) {
    if (!(o.magnitude_w >= MIN_MAGNITUDE_W) || !std::isfinite(o.reported_w)) return;
    if (count_ == MAX_OBS) {
      for (size_t i = 1; i < MAX_OBS; ++i) obs_[i - 1] = obs_[i];
      --count_;
    }
    obs_[count_++] = o;
  }
  // nominal_hz: the Zappi-reported frequency, used to center the search.
  const Fit &solve(double nominal_hz) {
    fit_ = Fit{};
    // Drop observations older than the window.
    size_t first = 0;
    const uint32_t t_ref = count_ ? obs_[count_ - 1].t_us : 0;
    while (first < count_ && uint32_t(t_ref - obs_[first].t_us) > WINDOW_US) ++first;
    if (first) {
      for (size_t i = first; i < count_; ++i) obs_[i - first] = obs_[i];
      count_ -= first;
    }
    if (count_ < 4) return fit_;
    double best_cost = 1e300;
    Fit best;
    // Sums for r ~ ca*M*cos(x) + cb*M*sin(x); returns residual sum of squares.
    auto score = [&](double f, const float *c, const float *s, Fit &out) {
      double saa = 0, sab = 0, sbb = 0, sar = 0, sbr = 0, srr = 0;
      for (size_t i = 0; i < count_; ++i) {
        const double m = obs_[i].magnitude_w, r = obs_[i].reported_w;
        const double a = m * c[i], b = m * s[i];
        saa += a * a; sab += a * b; sbb += b * b;
        sar += a * r; sbr += b * r; srr += r * r;
      }
      const double det = saa * sbb - sab * sab;
      if (std::fabs(det) < 1e-6 * saa * sbb + 1e-12) return 1e300;
      const double ca = (sar * sbb - sbr * sab) / det, cb = (sbr * saa - sar * sab) / det;
      out.frequency_hz = f;
      out.phase_deg = std::atan2(cb, ca) * 180.0 / M_PI;
      out.gain = std::hypot(ca, cb);
      const double cost = std::max(0.0, srr - ca * sar - cb * sbr);
      out.rms_w = std::sqrt(cost / count_);
      return cost;
    };
    std::array<float, MAX_OBS> c{}, s{}, dc{}, ds{};
    auto angles_at = [&](double f) {
      for (size_t i = 0; i < count_; ++i) {
        const double dt = double(int32_t(obs_[i].t_us - t_ref)) * 1e-6;
        const double x = std::fmod(double(obs_[i].angle_deg) - 360.0 * f * dt, 360.0) * M_PI / 180.0;
        c[i] = std::cos(x); s[i] = std::sin(x);
      }
    };
    // Coarse search: per-observation rotation recurrence, no trig per step.
    constexpr int STEPS = 160;
    constexpr double STEP_HZ = 0.001;
    const double f0 = nominal_hz - STEPS * STEP_HZ;
    angles_at(f0);
    for (size_t i = 0; i < count_; ++i) {
      const double dt = double(int32_t(obs_[i].t_us - t_ref)) * 1e-6;
      const double d = -360.0 * STEP_HZ * dt * M_PI / 180.0;
      dc[i] = std::cos(d); ds[i] = std::sin(d);
    }
    for (int k = 0; k <= 2 * STEPS; ++k) {
      Fit f;
      const double cost = score(f0 + k * STEP_HZ, c.data(), s.data(), f);
      if (cost < best_cost) { best_cost = cost; best = f; }
      for (size_t i = 0; i < count_; ++i) {
        const float nc = c[i] * dc[i] - s[i] * ds[i];
        s[i] = s[i] * dc[i] + c[i] * ds[i];
        c[i] = nc;
      }
    }
    if (best_cost >= 1e299) return fit_;
    // Golden-section refinement within +/- one coarse step, exact angles.
    double lo = best.frequency_hz - STEP_HZ, hi = best.frequency_hz + STEP_HZ;
    for (int it = 0; it < 20; ++it) {
      const double m1 = lo + (hi - lo) * 0.381966, m2 = lo + (hi - lo) * 0.618034;
      Fit f1, f2;
      angles_at(m1);
      const double c1 = score(m1, c.data(), s.data(), f1);
      angles_at(m2);
      const double c2 = score(m2, c.data(), s.data(), f2);
      if (c1 < best_cost) { best_cost = c1; best = f1; }
      if (c2 < best_cost) { best_cost = c2; best = f2; }
      if (c1 < c2) hi = m2; else lo = m1;
    }
    double mean_m = 0;
    for (size_t i = 0; i < count_; ++i) mean_m += obs_[i].magnitude_w;
    mean_m /= count_;
    best.valid = best_cost < 1e299;
    best.t_ref = t_ref;
    best.n = count_;
    best.rms_rel = mean_m > 0 ? best.rms_w / mean_m : 1;
    fit_ = best;
    return fit_;
  }
  // Predicted Zappi phase at t_us, degrees in [0, 360): EKF when tracking,
  // otherwise the batch fit.
  double predict(uint32_t t_us) const {
    const double dt = double(int32_t(t_us - (tracking_ ? t_ekf_ : fit_.t_ref))) * 1e-6;
    const double p0 = tracking_ ? phi_ : fit_.phase_deg, f = tracking_ ? freq_ : fit_.frequency_hz;
    double p = std::fmod(p0 + 360.0 * f * dt, 360.0);
    return p < 0 ? p + 360.0 : p;
  }

  // Extended Kalman filter on [phase deg, frequency Hz]; the frequency is a
  // random walk (FREQ_NOISE Hz/sqrt(s)). Measurement z = reported / magnitude
  // = cos(angle - phase). Started from a good batch fit.
#ifndef HARVI_FREQ_NOISE
#define HARVI_FREQ_NOISE 0.0016  // measured mains wander in the ESP32 clock
#define HARVI_PHASE_NOISE 1.0
#define HARVI_MEAS_SIGMA 0.03
#define HARVI_PILOT_SKEW 3.0f
#endif
  static constexpr double FREQ_NOISE = HARVI_FREQ_NOISE, PHASE_NOISE = HARVI_PHASE_NOISE,
                          MEAS_SIGMA = HARVI_MEAS_SIGMA;
  bool tracking() const { return tracking_; }
  double phase_sigma_deg() const { return std::sqrt(p_[0][0]); }
  // Phase uncertainty propagated (without measurements) to t_us.
  double phase_sigma_at(uint32_t t_us) const {
    if (!tracking_) return 1e9;
    const double dt = std::max(0.0, double(int32_t(t_us - t_ekf_)) * 1e-6);
    const double q = FREQ_NOISE * FREQ_NOISE;
    const double v = p_[0][0] + 2 * 360.0 * dt * p_[0][1] + 360.0 * 360.0 * dt * dt * p_[1][1] +
                     q * 360.0 * 360.0 * dt * dt * dt / 3 + PHASE_NOISE * PHASE_NOISE * dt;
    return std::sqrt(std::max(0.0, v));
  }
  double frequency_hz() const { return tracking_ ? freq_ : fit_.frequency_hz; }
  void start_tracking() {
    tracking_ = true;
    phi_ = fit_.phase_deg; freq_ = fit_.frequency_hz; t_ekf_ = fit_.t_ref;
    p_[0][0] = 100.0; p_[0][1] = p_[1][0] = 0.0; p_[1][1] = 1e-4;
  }
  void stop_tracking() { tracking_ = false; }
  // Returns the normalized innovation squared, or a negative value if gated out.
  double track(const Obs &o) {
    if (!tracking_ || !(o.magnitude_w >= MIN_MAGNITUDE_W)) return -1;
    const double dt = double(int32_t(o.t_us - t_ekf_)) * 1e-6;
    if (dt < 0) return -1;
    // Predict.
    phi_ = std::fmod(phi_ + 360.0 * freq_ * dt, 360.0);
    const double a = p_[0][0] + 2 * 360.0 * dt * p_[0][1] + 360.0 * 360.0 * dt * dt * p_[1][1];
    const double b = p_[0][1] + 360.0 * dt * p_[1][1];
    const double q = FREQ_NOISE * FREQ_NOISE;
    p_[0][0] = a + q * 360.0 * 360.0 * dt * dt * dt / 3 + PHASE_NOISE * PHASE_NOISE * dt;
    p_[0][1] = p_[1][0] = b + q * 360.0 * dt * dt / 2;
    p_[1][1] += q * dt;
    t_ekf_ = o.t_us;
    // Update.
    const double x = (o.angle_deg - phi_) * M_PI / 180.0;
    const double z = std::max(-1.3, std::min(1.3, double(o.reported_w) / o.magnitude_w));
    const double y = z - std::cos(x), hd = std::sin(x) * M_PI / 180.0;
    const double s = hd * hd * p_[0][0] + MEAS_SIGMA * MEAS_SIGMA;
    const double nis = y * y / s;
    if (nis > 16.0) return -nis;
    const double k0 = p_[0][0] * hd / s, k1 = p_[1][0] * hd / s;
    phi_ = std::fmod(phi_ + k0 * y + 360.0, 360.0);
    freq_ += k1 * y;
    const double p00 = p_[0][0], p01 = p_[0][1];
    p_[0][0] -= k0 * hd * p00;
    p_[0][1] -= k0 * hd * p01;
    p_[1][0] = p_[0][1];
    p_[1][1] -= k1 * hd * p01;
    return nis;
  }
  const Fit &fit() const { return fit_; }
  size_t size() const { return count_; }
  static constexpr float MIN_MAGNITUDE_W = 60.0f;

 private:
  std::array<Obs, MAX_OBS> obs_{};
  size_t count_{0};
  Fit fit_{};
  bool tracking_{false};
  double phi_{0}, freq_{50}, p_[2][2]{};
  uint32_t t_ekf_{0};
};

// Virtual Harvi measurement transmitter with closed-loop phase tracking.
// Measured on air (2026-10-21): a 0x3601 total reflects the latest packet from
// this device that arrived at least ~200 ms before it. When the Zappi hasn't
// taken up a newer packet, the total repeats exactly, so exact repeats are
// discarded. Packets are sent shortly after each status, ~550 ms before the
// next one. Until the phase fit is good, the phasor is sent unrotated: the
// mains phase then sweeps past it, which the fit needs, and golden-angle steps
// help. After lock, the phasor is rotated by the predicted Zappi phase.
class VirtualCT {
 public:
  // The Zappi's totals go out on a 1 s tick. Measured airtime around it: the
  // Zappi's own records at +0/+50/+125/+200 ms, and the genuine Harvi mostly
  // at +225..350 and +750..850 ms. One packet is sent in the quiet slot 450 ms
  // after each heard total, ~550 ms before the next tick (take-up >= 200 ms).
  // With one packet per heard total, attribution is unambiguous: if the Zappi
  // missed this device's packet, the next total repeats the one that triggered
  // it and is dropped as stale. The tick-aligned keep-alive covers only long
  // gaps (the Zappi holds a value for >= 34 s).
  static constexpr uint32_t REPORT_DELAY_US = 450000, FALLBACK_US = 7995000;
  static constexpr uint32_t LEAD_US = 4000, MIN_REPORT_AGE_US = 250000;
  // Quadrature pilot on the feedback CT: keeps the loop observable at night
  // and gives first-order error sensitivity. Alternating +/-3 deg about 90 deg
  // makes consecutive fresh totals differ (+/-16 W), so exact repeats remain
  // a reliable stale-report test despite the Zappi's 1-degree quantization.
  static constexpr double LOCK_SIGMA_DEG = 12.0, RESET_SIGMA_DEG = 60.0;
  // Ride-through: after losing lock, keep sending grid and battery rotated by a
  // snapshot of the last healthy lock for up to RIDE_US, while the snapshot's
  // propagated uncertainty stays under RIDE_SIGMA_DEG. Then withhold them.
  static constexpr uint32_t RIDE_US = 15000000;
  static constexpr double RIDE_SIGMA_DEG = 25.0;
  static constexpr float PILOT_W = 300.0f, PILOT_SKEW_DEG = HARVI_PILOT_SKEW;
  struct Sent {
    uint32_t t_us{0};
    std::array<float, 3> angle_deg{}, magnitude_w{};
    bool valid{false}, rotated{false}, used{false}, output{false}, riding{false};
  };
  bool active{false}, tracking_enabled{true};
  // When true, CTs other than the pilot CT are omitted (Zappi reads 0) until
  // the phase is locked, so no random-phase value can reach charger control.
  bool gate_outputs{true};
  uint32_t sent{0}, failed{0}, late{0}, observations{0}, ambiguous{0}, ride_packets{0};
  uint32_t window_ms{0};  // 0: run until stopped
  uint32_t input_gaps{0};
  // Which CT (0-based) each 0x3601 total reflects alone, or -1 when the
  // total includes devices other than this virtual Harvi.
  int8_t generation_ct{1}, grid_ct{-1};
  // Local mains voltage estimate (dV), used until the Zappi's own 0x5a5a is
  // heard. Waiting for 0x5a5a would deadlock, because the Zappi can stay quiet
  // on RF until it hears its Harvi.
  uint16_t fallback_voltage_dv{2300};

  void start(uint32_t now_ms) {
    active = true; started_ms_ = now_ms; invalid_input_ = false;
    last_ = Sent{}; report_us_ = 0; have_report_ = false; locked_ = false;
    history_ = {}; have_totals_ = false; have_hold_ = false;
    tracker_.reset(); rejected_ = 0; nis_avg_ = 0;
  }
  void stop() { active = false; }
  uint32_t seconds_left(uint32_t now_ms) const {
    if (!active) return 0;
    if (window_ms == 0) return UINT32_MAX;
    if (uint32_t(now_ms - started_ms_) >= window_ms) return 0;
    return (window_ms - uint32_t(now_ms - started_ms_) + 999) / 1000;
  }
  bool invalid_input() const { return invalid_input_; }
  // True when the last packet carried live (phase-predicted) measurements.
  bool locked() const { return last_.valid && last_.output; }
  // True when the last packet was a ride-through (held phase, values sent).
  bool riding() const { return last_.valid && last_.riding; }
  uint16_t voltage_dv() const { return voltage_dv_; }
  const PhaseTracker::Fit &fit() const { return tracker_.fit(); }
  const Sent &last_sent() const { return last_; }
  const PhaseTracker &tracker() const { return tracker_; }
  double nis_avg() const { return nis_avg_; }

  // Returns true when a new observation was added (fit updated).
  bool on_status(const ZappiStatus &s, uint32_t rx_us) {
    if (s.type == 0x5a5a) {
      voltage_dv_ = s.voltage_dv;
      nominal_hz_ = s.frequency_chz / 100.0;
      return false;
    }
    if (s.type != 0x3601) return false;
    bool added = false;
    // Candidate packets: unused and old enough to be reflected in this total.
    // Normally there's exactly one. If a report was missed and the Zappi also
    // missed this device's newer packet, the total might reflect an older
    // candidate. Then choose the one the current phase estimate explains
    // clearly better, or skip.
    Sent *packet = nullptr;
    size_t candidates = 0;
    for (auto &p : history_) {
      if (!p.valid || p.used || int32_t(rx_us - p.t_us) < int32_t(MIN_REPORT_AGE_US)) continue;
      ++candidates;
      if (packet == nullptr || int32_t(p.t_us - packet->t_us) > 0) packet = &p;
    }
    if (candidates > 1) {
      Sent *best = nullptr;
      double best_err = 1e9, next_err = 1e9;
      const int8_t ct = generation_ct;
      if (tracker_.tracking() && ct >= 0 && ct <= 2) {
        for (auto &p : history_) {
          if (!p.valid || p.used || int32_t(rx_us - p.t_us) < int32_t(MIN_REPORT_AGE_US)) continue;
          const double x = (p.angle_deg[ct] - tracker_.predict(p.t_us)) * M_PI / 180.0;
          const double err = std::fabs(s.generation_w - p.magnitude_w[ct] * std::cos(x));
          if (err < best_err) { next_err = best_err; best_err = err; best = &p; }
          else if (err < next_err) next_err = err;
        }
      }
      const bool clear = best != nullptr && next_err > 3 * best_err + 0.1 * PILOT_W;
      if (!clear) {
        for (auto &p : history_) if (p.valid && int32_t(rx_us - p.t_us) >= int32_t(MIN_REPORT_AGE_US)) p.used = true;
        ++ambiguous;
        packet = nullptr;
      } else {
        packet = best;
      }
    }
    if (active && packet != nullptr) {
      auto observe = [&](int8_t ct, int32_t reported, int32_t previous) {
        if (ct < 0 || ct > 2 || packet->magnitude_w[ct] < PhaseTracker::MIN_MAGNITUDE_W) return;
        if (have_totals_ && reported == previous) return;  // stale: not updated
        const PhaseTracker::Obs o{packet->t_us, packet->angle_deg[ct], packet->magnitude_w[ct], float(reported)};
        tracker_.add(o);
        if (tracker_.tracking()) {
          const double nis = tracker_.track(o);
          if (nis < 0) ++rejected_; else rejected_ = 0;
          nis_avg_ = 0.8 * nis_avg_ + 0.2 * std::min(std::fabs(nis), 25.0);
        }
        added = true;
      };
      observe(generation_ct, s.generation_w, last_generation_w_);
      observe(grid_ct, s.grid_w, last_grid_w_);
      if (added) {
        // Older packets can no longer be the one reflected.
        for (auto &p : history_) if (p.valid && int32_t(p.t_us - packet->t_us) <= 0) p.used = true;
        ++observations;
        last_obs_us_ = rx_us;
        update_lock_();
      }
    }
    // Whatever the outcome (fresh, stale or ambiguous), packets this old can
    // no longer become the one a later total reflects.
    for (auto &p : history_)
      if (p.valid && int32_t(rx_us - p.t_us) >= int32_t(MIN_REPORT_AGE_US)) p.used = true;
    last_generation_w_ = s.generation_w;
    last_grid_w_ = s.grid_w;
    have_totals_ = true;
    report_us_ = rx_us;
    have_report_ = true;
    return added;
  }

  // Called frequently. When a packet is due, returns its payload and sets
  // at_us: the caller must transmit with transmit_packet_at(payload, at_us).
  // inputs_ok=false (stale or unavailable source) keeps the pilot running so
  // the phase stays locked, but omits every measurement. The Zappi then reads 0
  // instead of a frozen value.
  std::vector<uint8_t> next(uint32_t now_ms, uint32_t now_us, float grid_w, float solar_w,
                            float battery_w, bool inputs_ok, uint32_t &at_us) {
    if (!seconds_left(now_ms)) { stop(); return {}; }
    if (!inputs_ok || !std::isfinite(grid_w) || !std::isfinite(solar_w) || !std::isfinite(battery_w)) {
      invalid_input_ = true;
      grid_w = solar_w = battery_w = NAN;
    } else {
      invalid_input_ = false;
    }
    const uint16_t v_dv = voltage_dv_ ? voltage_dv_ : fallback_voltage_dv;
    if (v_dv == 0) return {};
    const bool after_report = have_report_ && (!last_.valid || int32_t(last_.t_us - report_us_) < 0) &&
        int32_t(now_us - report_us_) >= int32_t(REPORT_DELAY_US);
    const bool fallback = !last_.valid || int32_t(now_us - last_.t_us) >= int32_t(FALLBACK_US);
    if (!after_report && !fallback) return {};
    at_us = now_us + LEAD_US;
    // Coasting without feedback: the propagated uncertainty decides. Output is
    // withheld beyond LOCK_SIGMA_DEG. The filter keeps its state and
    // re-converges when feedback returns (the pilot's error signal is
    // unambiguous within +/-90 deg). Start over only beyond RESET_SIGMA_DEG.
    const double sigma = tracker_.phase_sigma_at(at_us);
    if (tracker_.tracking() && sigma > RESET_SIGMA_DEG) { tracker_.stop_tracking(); tracker_.reset(); }
    const bool output = locked_ && tracker_.tracking() && sigma < LOCK_SIGMA_DEG;
    const bool rotate = tracking_enabled && tracker_.tracking();
    // Acquisition: golden-angle steps guarantee phase diversity even when send
    // times are commensurate with the mains cycle. Send times follow the
    // Zappi's 1 s status ticks, and 50 Hz has an integer number of cycles per
    // second.
    const double base = rotate ? tracker_.predict(at_us) : std::fmod(sequence_ * 137.50776, 360.0);
    if (output && nis_avg_ < 2.0) { hold_ = tracker_; hold_us_ = at_us; have_hold_ = true; }
    // Ride only through quiet gaps (tracker still consistent). After the
    // feedback has shown a phase jump, the held phase is wrong. In an overnight
    // soak test, holding then flipped the battery sign in 58 of 195 records,
    // compared with 1 of 93 when the tracker was still consistent.
    const bool ride = !output && have_hold_ && tracker_.tracking() && nis_avg_ < 4.0 &&
                      int32_t(at_us - hold_us_) < int32_t(RIDE_US) &&
                      hold_.phase_sigma_at(at_us) < RIDE_SIGMA_DEG;
    const bool live = output || ride;
    const double value_base = output ? base : ride ? hold_.predict(at_us) : base;
    const float skew = (sequence_ & 1) ? PILOT_SKEW_DEG : -PILOT_SKEW_DEG;
    Packet p;
    p.serial = SERIAL;
    p.sequence = sequence_++;
    pending_ = Sent{};
    pending_.t_us = at_us;
    pending_.rotated = rotate;
    pending_.output = output;
    pending_.riding = ride;
    const float watts[3] = {grid_w, solar_w, battery_w};
    if (invalid_input_) ++input_gaps;
    for (size_t i = 0; i < 3; ++i) {
      // The measurement is in phase with value_base; the pilot CT adds a
      // near-quadrature PILOT_W component on base (which drives acquisition).
      const bool known = std::isfinite(watts[i]);
      // While acquiring, the pilot CT carries only the pilot so that its
      // meaningless reading stays within +/-PILOT_W.
      double re = 0, im = 0;
      if (known && live) {
        re += watts[i] * std::cos(value_base * M_PI / 180.0);
        im += watts[i] * std::sin(value_base * M_PI / 180.0);
      }
      if (int8_t(i) == generation_ct) {
        const double q = (base + 90.0 + skew) * M_PI / 180.0;
        re += PILOT_W * std::cos(q);
        im += PILOT_W * std::sin(q);
      } else if (!known || (gate_outputs && !live)) {
        continue;  // omitted: the Zappi clears this CT to 0
      }
      const int32_t m = phasor_magnitude(float(std::hypot(re, im)), v_dv);
      if (m < 0) continue;  // beyond the field range (> ~40 kW): omit
      double angle = std::fmod(std::atan2(im, re) * 180.0 / M_PI + 360.0, 360.0);
      p.ct[i] = rotated_ct(m, angle);
      pending_.angle_deg[i] = float(angle);
      pending_.magnitude_w[i] = reported_magnitude_w(m, v_dv);
    }
    auto fifo = encode_fifo(p);
    return {fifo.begin() + 1, fifo.end()};
  }
  // Report the transmit result; actual_us is the driver's trigger timestamp.
  void complete(bool ok, bool was_late, uint32_t actual_us) {
    if (ok) {
      ++sent;
      if (pending_.riding) ++ride_packets;
      last_ = pending_;
      last_.t_us = actual_us;
      last_.valid = true;
      history_[history_next_++ % history_.size()] = last_;
    } else if (was_late) {
      ++late;  // nothing sent; retry on the next call
    } else {
      ++failed; stop();
    }
  }

 private:
  // Acquire with the batch fit, then track with the EKF. Output is "locked"
  // only while the EKF is confident and its innovations are consistent.
  void update_lock_() {
    if (!tracker_.tracking()) {
      const auto &f = tracker_.solve(nominal_hz_);
      // Eight observations, or five when the fit is very clean (fast relock
      // after a phase jump; on-air replays: 81 -> 85 % locked, same error).
      const bool enough = f.n >= 8 ? f.rms_rel < 0.12f : f.n >= 5 && f.rms_rel < 0.04f;
      if (f.valid && enough && f.gain > 0.85f && f.gain < 1.15f) {
        tracker_.start_tracking();
        rejected_ = 0; nis_avg_ = 1.0;
      }
    } else if (rejected_ >= 4 || nis_avg_ > 6.0) {
      tracker_.stop_tracking();
      tracker_.reset();
    }
    // Hysteresis on consistency: enter lock below 4, leave only above 6 (the
    // divergence limit). On-air replays: borderline NIS ~4 caused flapping.
    locked_ = tracker_.tracking() && tracker_.phase_sigma_deg() < LOCK_SIGMA_DEG && nis_avg_ < (locked_ ? 6.0 : 4.0);
  }
  PhaseTracker tracker_, hold_;
  bool have_hold_{false};
  uint32_t hold_us_{0};
  double nis_avg_{0};
  uint8_t rejected_{0};
  Sent last_, pending_;
  std::array<Sent, 4> history_{};
  size_t history_next_{0};
  int32_t last_generation_w_{0}, last_grid_w_{0};
  bool have_totals_{false};
  uint32_t started_ms_{0}, report_us_{0}, last_obs_us_{0};
  uint16_t voltage_dv_{0};
  double nominal_hz_{50.0};
  uint8_t sequence_{0};
  bool have_report_{false}, locked_{false}, invalid_input_{false};
};

class Scan {
 public:
  bool active{false}, found{false};
  uint32_t reference_serial{0};
  uint8_t index{0};
  void start(uint32_t now) { active=true; found=false; index=0; entered_=now; matches_=0; }
  void stop() { active=false; }
  // Returns true when a retune is due. Runs one 8 x 12-second sweep, then stops.
  bool advance(uint32_t now) {
    if (!active || uint32_t(now-entered_) < 12000) return false;
    if (index==7) { active=false; return false; }
    ++index; entered_=now; matches_=0; return true;
  }
  // Without a reference Harvi (none installed, or one retired and its serial
  // reused by this device), the channel is found from any Zappi's status
  // broadcasts.
  bool observe_zappi(const ZappiStatus &status) {
    if (!active || reference_serial != 0 || !status.type) return false;
    if (++matches_ >= 2) { found=true; active=false; return true; }
    return false;
  }
  bool observe(const Packet &packet) {
    if (!active || reference_serial == 0 || packet.serial != reference_serial) return false;
    // Require distinct sequence values, so cached or duplicate packets can't lock the scan.
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

// Receipt-age diagnostics only. They don't prove that the source measurement is fresh.
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
  // True when every configured input has a finite value received since the
  // API connection last started. Gates CT measurements. Unconfigured inputs
  // are ignored.
  bool enabled(size_t channel) const { return channel < 3 && enabled_[channel]; }
  bool usable() const {
    for (size_t i=0; i<3; ++i) if (enabled_[i] && !valid_[i]) return false;
    return true;
  }
  void reset() { valid_.fill(false); }
 private:
  std::array<uint32_t, 3> updated_{};
  std::array<bool, 3> valid_{}, enabled_{};
};
}  // namespace harvi
