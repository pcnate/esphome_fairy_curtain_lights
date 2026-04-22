#include "esphome.h"
#include "driver/ledc.h"

namespace esphome {
namespace fairy_curtain_lights {

static constexpr int MAX_DUTY = 8191;

class FairyCurtainLights : public Component {
 public:
  class ShiftedPWM : public output::FloatOutput {
   public:
    ShiftedPWM(int pin, ledc_channel_t channel, uint32_t hpoint)
        : pin_(pin), channel_(channel), hpoint_(hpoint) {}

    void setup() {
      pinMode(pin_, OUTPUT);
      ledc_channel_config_t config = {
          .gpio_num = pin_,
          .speed_mode = LEDC_LOW_SPEED_MODE,
          .channel = channel_,
          .timer_sel = LEDC_TIMER_0,
          .duty = 0,
          .hpoint = hpoint_,
      };
      ledc_channel_config(&config);
    }

    void write_state(float state) override {
      uint32_t duty = state * (MAX_DUTY / 2);
      ledc_set_duty(LEDC_LOW_SPEED_MODE, channel_, duty);
      ledc_update_duty(LEDC_LOW_SPEED_MODE, channel_);
    }

   protected:
    int pin_;
    ledc_channel_t channel_;
    uint32_t hpoint_;
  };

  FairyCurtainLights(int pin_a, int pin_b, int frequency) {
    channel_a_ = new ShiftedPWM(pin_a, LEDC_CHANNEL_0, 0);
    channel_b_ = new ShiftedPWM(pin_b, LEDC_CHANNEL_1, MAX_DUTY / 2);

    ledc_timer_config_t timer_config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = frequency,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer_config);
  }

  void setup() override {
    channel_a_->setup();
    channel_b_->setup();
  }

  output::FloatOutput* get_channel_a() { return channel_a_; }
  output::FloatOutput* get_channel_b() { return channel_b_; }

 protected:
  ShiftedPWM *channel_a_;
  ShiftedPWM *channel_b_;
};

}  // namespace fairy_curtain_lights
}  // namespace esphome
