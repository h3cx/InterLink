#include <InterLink.h>

#if defined(HAVE_HWSERIAL1)
#define LINK_SERIAL Serial1
#else
#define LINK_SERIAL Serial
#endif

interlink::InterLink link(LINK_SERIAL);

void setup() {
  Serial.begin(115200);
  LINK_SERIAL.begin(115200);
  link.begin();

  uint8_t page = 3;
  link.send(0x0001, 0x00, 0x05, &page, 1);
}

void loop() {
  link.poll();
}
