#include "adxl345.hpp"
#include "hardware/gpio.h"
#include "pico/time.h"
namespace guitar {
namespace {
// CYW43/BTstack IRQ work can interrupt polling. This is a failure bound, not a
// delay: a successful 400 kHz transaction still returns as soon as it completes.
constexpr uint32_t transaction_timeout_us=2000;
uint32_t sample_clock() { return to_ms_since_boot(get_absolute_time()); }
}
void Adxl345::stop() {
    if (bus_) {
        i2c_deinit(bus_);
        gpio_deinit(config_.sda); gpio_deinit(config_.scl);
    }
    bus_ = nullptr; ready_ = valid_ = attempted_ = false; sample_ = {};
}
void Adxl345::init(const TiltConfig& config) {
    stop(); config_ = config;
    if (config.source != TiltSource::Adxl345) return;
    bus_ = (config.sda & 2) ? i2c1 : i2c0;
    i2c_init(bus_, 400000);
    gpio_set_function(config.sda, GPIO_FUNC_I2C); gpio_set_function(config.scl, GPIO_FUNC_I2C);
    gpio_pull_up(config.sda); gpio_pull_up(config.scl);
}
bool Adxl345::read(uint8_t reg, uint8_t* data, size_t size) {
    return i2c_write_timeout_us(bus_, config_.address, &reg, 1, true, transaction_timeout_us) == 1 &&
           i2c_read_timeout_us(bus_, config_.address, data, size, false, transaction_timeout_us) == static_cast<int>(size);
}
bool Adxl345::write(uint8_t reg, uint8_t value) {
    const uint8_t bytes[] = {reg, value};
    return i2c_write_timeout_us(bus_, config_.address, bytes, 2, false, transaction_timeout_us) == 2;
}
bool Adxl345::configure() {
    uint8_t id = 0;
    // ADXL345 DEVID=E5, full resolution +/-16 g, 100 Hz, FIFO bypass.
    return read(0x00, &id, 1) && id == 0xe5 && write(0x2d, 0) && write(0x31, 0x0b) &&
           write(0x2c, 0x0a) && write(0x38, 0) && write(0x2d, 0x08);
}
void Adxl345::sample(uint32_t now) {
    if (!bus_) return;
    // Read the clock here, after possible radio work; callers may hold an older tick.
    now=sample_clock();
    valid_=sample_.fresh(now);
    if (attempted_ && uint32_t(now - last_) < (ready_ ? 10u : 1000u)) return;
    attempted_ = true; last_ = now;
    if (!ready_) {
        ready_=configure();last_=sample_clock();valid_=false;
        return;
    }
    std::array<uint8_t,6> bytes{};
    const bool read_ok=read(0x32,bytes.data(),bytes.size());
    const uint32_t completed=sample_clock();last_=completed;
    const bool accepted=read_ok && sample_.accept(bytes,completed);
    valid_=sample_.fresh(completed);
    if(!accepted && !valid_) {
        ready_=false;sample_={}; // Persistent fault: release Tilt and recover at 1 Hz.
    }
}
}
