#include <InterLink.h>

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  const uint8_t vector[] = {'1','2','3','4','5','6','7','8','9'};
  uint16_t crc = interlink::Crc16Arc::compute(vector, sizeof(vector));

  Serial.println("InterLink SelfTest");
  Serial.print("CRC16/ARC 123456789 = 0x");
  Serial.println(crc, HEX);

  if (crc == 0xBB3D) {
    Serial.println("CRC test PASS");
  } else {
    Serial.println("CRC test FAIL");
  }

  uint16_t cmd = 0x0001;
  uint8_t seq = 0x42;
  uint8_t payload[] = {0x10, 0x20};

  uint16_t packetCrc = 0x0000;
  packetCrc = interlink::Crc16Arc::update(packetCrc, interlink::kVersion);
  packetCrc = interlink::Crc16Arc::update(packetCrc, 0x00);
  packetCrc = interlink::Crc16Arc::update(packetCrc, static_cast<uint8_t>(cmd & 0xFF));
  packetCrc = interlink::Crc16Arc::update(packetCrc, static_cast<uint8_t>((cmd >> 8) & 0xFF));
  packetCrc = interlink::Crc16Arc::update(packetCrc, seq);
  packetCrc = interlink::Crc16Arc::update(packetCrc, sizeof(payload));
  for (uint8_t i = 0; i < sizeof(payload); ++i) {
    packetCrc = interlink::Crc16Arc::update(packetCrc, payload[i]);
  }

  Serial.print("Sample packet CRC = 0x");
  Serial.println(packetCrc, HEX);
}

void loop() {}
