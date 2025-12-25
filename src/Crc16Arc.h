#pragma once

#include <Arduino.h>

namespace interlink {

class Crc16Arc {
 public:
  static uint16_t update(uint16_t crc, uint8_t data);
  static uint16_t compute(const uint8_t *data, size_t length);
};

}  // namespace interlink
