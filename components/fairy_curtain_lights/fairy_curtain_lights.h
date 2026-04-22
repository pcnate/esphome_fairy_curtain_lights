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
    ShiftedPWM( int pin, ledc_channel_t channel, uint32_t hpoint )
      : pin_( pin ), channel_( channel ), hpoint_( hpoint ) {}

    void setup() override {
      pinMode( pin_, OUTPUT );
      ledc_channel_config_t cfg{};
      cfg.gpio_num   = pin_;
      cfg.speed_mode = LEDC_LOW_SPEED_MODE;
      cfg.channel    = channel_;
      cfg.timer_sel  = LEDC_TIMER_0;
      cfg.duty       = 0;
      cfg.hpoint     = hpoint_;
      ledc_channel_config( &cfg );
    }

    void write_state( float state ) override {
      uint32_t duty = static_cast<uint32_t>( state * ( MAX_DUTY / 2 ) );
      ledc_set_duty( LEDC_LOW_SPEED_MODE, channel_, duty );
      ledc_update_duty( LEDC_LOW_SPEED_MODE, channel_ );
    }

   protected:
    int pin_;
    ledc_channel_t channel_;
    uint32_t hpoint_;
  };

  FairyCurtainLights( int pin_a, int pin_b, int frequency ) {
    channel_a_ = new ShiftedPWM( pin_a, LEDC_CHANNEL_0, 0 );
    channel_b_ = new ShiftedPWM( pin_b, LEDC_CHANNEL_1, MAX_DUTY / 2 );

    ledc_timer_config_t t{};
    t.speed_mode      = LEDC_LOW_SPEED_MODE;
    t.duty_resolution = LEDC_TIMER_13_BIT;
    t.timer_num       = LEDC_TIMER_0;
    t.freq_hz         = frequency;
    t.clk_cfg         = LEDC_AUTO_CLK;
    ledc_timer_config( &t );
  }

  void setup() override {
    channel_a_->setup();
    channel_b_->setup();
  }

  output::FloatOutput *get_channel_a() { return channel_a_; }
  output::FloatOutput *get_channel_b() { return channel_b_; }

 protected:
  static constexpr int MAX_DUTY = 8191;
  ShiftedPWM *channel_a_{ nullptr };
  ShiftedPWM *channel_b_{ nullptr };
};

}  // namespace fairy_curtain_lights
}  // namespace esphome
