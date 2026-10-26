#pragma once
#include "tilt_config.hpp"
#include "adxl_sample.hpp"
#include "hardware/i2c.h"
namespace guitar {
class Adxl345 {
public:
    void init(const TiltConfig& config);
    void stop();
    void sample(uint32_t now);
    bool valid() const { return valid_; }
    const std::array<int16_t,3>& axes() const { return sample_.axes; }
private:
    bool read(uint8_t reg, uint8_t* data, size_t size);
    bool write(uint8_t reg, uint8_t value);
    bool configure();
    i2c_inst_t* bus_ = nullptr;
    TiltConfig config_{};
    bool ready_ = false, valid_ = false, attempted_ = false;
    uint32_t last_ = 0;
    AdxlSample sample_{};
};
}
