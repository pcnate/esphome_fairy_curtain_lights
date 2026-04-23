// esphome/components/fairy_curtain_lights/fairy_curtain_lights.h
#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/output/float_output.h"
#include "driver/ledc.h"

namespace esphome {
namespace fairy_curtain_lights {

class FairyCurtainLights : public Component {
 public:
  class ShiftedPWM : public output::FloatOutput {
   public:
    ShiftedPWM( int pin, ledc_channel_t channel, uint32_t hpoint );
    void setup();
    void write_state( float state ) override;

   protected:
    int pin_;
    ledc_channel_t channel_;
    uint32_t hpoint_;
  };

  FairyCurtainLights( int pin_a, int pin_b, int frequency );
  void setup() override;

  output::FloatOutput *get_channel_a() { return channel_a_; }
  output::FloatOutput *get_channel_b() { return channel_b_; }

 protected:
  static constexpr int MAX_DUTY = 8191;
  ShiftedPWM *channel_a_{ nullptr };
  ShiftedPWM *channel_b_{ nullptr };
};

}  // namespace fairy_curtain_lights
}  // namespace esphome
