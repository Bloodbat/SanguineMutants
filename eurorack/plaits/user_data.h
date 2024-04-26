// Copyright 2021 Emilie Gillet.
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
// User data manager.

#ifndef PLAITS_USER_DATA_H_
#define PLAITS_USER_DATA_H_

#include "stmlib/stmlib.h"

#include <cstring>

#include "plugin.hpp"

namespace plaits {

class UserData {
public:
  static const size_t MAX_USER_DATA_SIZE = 4096; // 0x1000

  UserData() {
    for (size_t i = 0; i < MAX_USER_DATA_SIZE; ++i) {
      m_buffer[i] = 0;
    }
  }
  ~UserData() { }

  inline void setBuffer(const uint8_t* buffer) {
    memcpy(m_buffer, buffer, MAX_USER_DATA_SIZE * sizeof(uint8_t));
  }

  inline const uint8_t* getBuffer() {
    if (m_buffer[MAX_USER_DATA_SIZE - 2] == 'U') {
      return m_buffer;
    } else {
      return NULL;
    }
  }

  inline const uint8_t* ptr(int slot) const {
    if (m_buffer[MAX_USER_DATA_SIZE - 2] == 'U' && m_buffer[MAX_USER_DATA_SIZE - 1] == (' ' + slot)) {
      return m_buffer;
    } else {
      return NULL;
    }
  }
  
  inline bool Save(uint8_t* rx_buffer, int slot) {
    if (rx_buffer == NULL) {
      for (size_t i = 0; i < MAX_USER_DATA_SIZE; ++i) {
        m_buffer[i] = 0;
      }
    } else {
      if (slot < rx_buffer[MAX_USER_DATA_SIZE - 2] || slot > rx_buffer[MAX_USER_DATA_SIZE - 1]) {
        return false;
      }
      memcpy(m_buffer, rx_buffer, MAX_USER_DATA_SIZE * sizeof(uint8_t));
      m_buffer[MAX_USER_DATA_SIZE - 2] = 'U';
      m_buffer[MAX_USER_DATA_SIZE - 1] = ' ' + slot;
    }

    return true;
  }

private:
  uint8_t m_buffer[MAX_USER_DATA_SIZE];

};

}  // namespace plaits

#endif  // PLAITS_USER_DATA_H_
