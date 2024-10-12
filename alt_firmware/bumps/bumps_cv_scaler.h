// Copyright 2013 Emilie Gillet.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
// 
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// -----------------------------------------------------------------------------
//
// CV scaling functions.

#ifndef BUMPS_CV_SCALER_H_
#define BUMPS_CV_SCALER_H_

#include "parasites_stmlib/parasites_stmlib.h"
#include "parasites_stmlib/utils/parasites_dsp.h"

#include "bumps/bumps_resources.h"

namespace bumps {

const int16_t kOctave = 12 * 128;

#define SE * 128

const uint16_t quantize_lut[7][12] = {
  /* semitones */
  {0, 1 SE, 2 SE, 3 SE, 4 SE, 5 SE, 6 SE, 7 SE, 8 SE, 9 SE, 10 SE, 11 SE},
  /* ionian */
  {0, 0, 2 SE, 2 SE, 4 SE, 5 SE, 5 SE, 7 SE, 7 SE, 9 SE, 9 SE, 11 SE},
  /* aeolian */
  {0, 0, 2 SE, 3 SE, 3 SE, 5 SE, 5 SE, 7 SE, 8 SE, 8 SE, 10 SE, 10 SE},
  /* whole tones */
  {0, 0, 2 SE, 2 SE, 4 SE, 4 SE, 6 SE, 6 SE, 8 SE, 8 SE, 10 SE, 10 SE},
  /* pentatonic minor */
  {0, 0, 3 SE, 3 SE, 3 SE, 5 SE, 5 SE, 7 SE, 7 SE, 10 SE, 10 SE, 10 SE},
  /* pent-3 */
  {0, 0, 0, 0, 7 SE, 7 SE, 7 SE, 7 SE, 10 SE, 10 SE, 10 SE, 10 SE},
  /* fifths */
  {0, 0, 0, 0, 0, 0, 7 SE, 7 SE, 7 SE, 7 SE, 7 SE, 7 SE},
};

enum AdcChannel {
  ADC_CHANNEL_LEVEL,
  ADC_CHANNEL_V_OCT,
  ADC_CHANNEL_FM,
  ADC_CHANNEL_FM_ATTENUVERTER,
  ADC_CHANNEL_SHAPE,
  ADC_CHANNEL_SLOPE,
  ADC_CHANNEL_SMOOTHNESS,
  ADC_CHANNEL_LAST
};

class CvScaler {
 public:
  struct CalibrationData {
    int32_t level_offset;
    int32_t v_oct_offset;
    int32_t v_oct_scale;
    int32_t fm_offset;
    int32_t fm_scale;
    int32_t padding[3];
  };
   
  CvScaler() { }
  ~CvScaler() { }
  
  void Init();
  
  inline void ProcessSampleRate(const uint16_t* raw_adc_data) {
    level_raw_ = (level_raw_ * 15 + raw_adc_data[ADC_CHANNEL_LEVEL]) >> 4;
    level_ = -level_raw_ + calibration_data_.level_offset;
    if (level_ < 32) {  // 2 LSB of ADC noise.
      level_ = 0;
    }
  }
  
  inline void ProcessControlRate(const uint16_t* raw_adc_data) {
    int32_t scaled_value;
    
    scaled_value = 32767 - static_cast<int32_t>(
        raw_adc_data[ADC_CHANNEL_SHAPE]);
    shape_ = (shape_ * 7 + scaled_value) >> 3;
    
    scaled_value = 32767 - static_cast<int32_t>(
        raw_adc_data[ADC_CHANNEL_SLOPE]);
    slope_ = (slope_ * 7 + scaled_value) >> 3;

    scaled_value = 32767 - static_cast<int32_t>(
        raw_adc_data[ADC_CHANNEL_SMOOTHNESS]);
    smoothness_ = (smoothness_ * 7 + scaled_value) >> 3;

    scaled_value = static_cast<int32_t>(raw_adc_data[ADC_CHANNEL_V_OCT]);
    v_oct_ = (v_oct_ * 3 + scaled_value) >> 2;
    
    scaled_value = static_cast<int32_t>(raw_adc_data[ADC_CHANNEL_FM]);
    fm_ = (fm_ * 3 + scaled_value) >> 2;

    scaled_value = static_cast<int32_t>(
        raw_adc_data[ADC_CHANNEL_FM_ATTENUVERTER]);
    attenuverter_ = (attenuverter_ * 15 + scaled_value) >> 4;
  }
  
  inline uint16_t level() const { return level_; }
  inline int16_t shape() const { return shape_; }
  inline int16_t slope() const { return slope_; }
  inline int16_t smoothness() const { return smoothness_; }
  inline int16_t pitch() {
    if (quantize_) {
      // Apply hysteresis and filtering to ADC reading to prevent
      // jittery quantization.
      if ((v_oct_ > previous_v_oct_ + 16) ||
          (v_oct_ < previous_v_oct_ - 16)) {
        previous_v_oct_ = v_oct_;
      } else {
        previous_v_oct_ += (v_oct_ - previous_v_oct_) >> 5;
        v_oct_ = previous_v_oct_;
      }
    }

    int32_t pitch = (v_oct_ - calibration_data_.v_oct_offset) * \
      calibration_data_.v_oct_scale >> 15;
    pitch += 60 << 7;

    if (quantize_) {
      uint16_t semi = pitch >> 7;
      uint16_t octaves = semi / 12 ;
      semi -= octaves * 12;
      pitch = octaves * kOctave +
        quantize_lut[quantize_ - 1][semi];
    }

    return pitch;
  }

  inline int16_t fm() {
    int32_t attenuverter_value = attenuverter_ - 32768;
    int32_t attenuverter_sign = 1;
    if (attenuverter_value < 0) {
      attenuverter_value = -attenuverter_value - 1;
      attenuverter_sign = - 1;
    }
    attenuverter_value = attenuverter_sign * static_cast<int32_t>(
        parasites_stmlib::Interpolate88(lut_attenuverter_curve, attenuverter_value << 1));
    int32_t fm = (fm_ - calibration_data_.fm_offset) *  \
      calibration_data_.fm_scale >> 15;
    fm = fm * attenuverter_value >> 16;

    return fm;
  }


  inline uint16_t raw_attenuverter() const { return attenuverter_; }

  void CaptureCalibrationValues() {
    v_oct_c2_ = v_oct_;
  }
  
  void Calibrate();
  
  inline bool can_enter_calibration() const {
    return level_raw_ >= 49152;
  }

  uint8_t quantize_;

 private:
  void SaveCalibrationData();

  CalibrationData calibration_data_;
  
  int32_t level_raw_;
  int32_t level_;
  int32_t v_oct_;
  int32_t previous_v_oct_;
  int32_t fm_;
  int32_t attenuverter_;
  int32_t shape_;
  int32_t slope_;
  int32_t smoothness_;
  
  int32_t v_oct_c2_;
  
  static const CalibrationData init_calibration_data_;
  uint16_t version_token_;
  
  DISALLOW_COPY_AND_ASSIGN(CvScaler);
};

}  // namespace bumps

#endif  // BUMPS_CV_SCALER_H_
