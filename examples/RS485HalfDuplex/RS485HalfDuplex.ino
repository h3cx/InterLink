#include <InterLink.h>

#if defined(HAVE_HWSERIAL1)
#define LINK_SERIAL Serial1
#else
#define LINK_SERIAL Serial
#endif

constexpr int8_t kDePin = 2;

interlink::InterLink link(LINK_SERIAL);

void setup() {
  Serial.begin(115200);
  LINK_SERIAL.begin(9600);

  link.setDirectionPin(kDePin, true, 100);
  link.begin();

  const uint8_t payload[] = {0xAA, 0x55};
  link.send(0x0200, 0x00, 0x01, payload, sizeof(payload));
}

void loop() {
  link.poll();
}
