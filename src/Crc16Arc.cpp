#include "Crc16Arc.h"

namespace interlink {

uint16_t Crc16Arc::update(uint16_t crc, uint8_t data) {
  crc ^= data;
  for (uint8_t i = 0; i < 8; ++i) {
    if (crc & 0x0001) {
      crc = (crc >> 1) ^ 0xA001;
    } else {
      crc >>= 1;
    }
  }
  return crc;
}

uint16_t Crc16Arc::compute(const uint8_t *data, size_t length) {
  uint16_t crc = 0x0000;
  for (size_t i = 0; i < length; ++i) {
    crc = update(crc, data[i]);
  }
  return crc;
}

}  // namespace interlink
