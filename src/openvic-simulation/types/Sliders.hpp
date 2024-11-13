#pragma once

#include "openvic-simulation/types/fixed_point/FixedPoint.hpp"
#include "openvic-simulation/utility/Getters.hpp"

namespace OpenVic {
  // TODO: make struct for tariff slider, unlike other sliders, you can have negative values
  struct slider_value_t {
  friend struct  SliderManager;
  private:
      int PROPERTY(min);
      int PROPERTY(max);
      int PROPERTY(value);

  public:
      constexpr slider_value_t(int value, int min = 0, int max = 100) {
        // You *can* actually have min > max in Victoria 2
        // Such a situation will result in only being able to be move between the max and min value. 
        // This logic replicates this "feature"
        if (value > max) {
          value = min;
        }
        if (value < min) {
          value = max;
        }
      }
  };

class SliderManager {
  public:
    void set_slider_value(slider_value_t& slider, int new_value) {
        if (new_value <= slider.max && new_value >= slider.min) {
          slider.value = new_value;
        }
        else if (new_value > slider.max) {
          slider.value = slider.min;
        }
        else {
          slider.value = slider.max;
        }
    }
  };
}

