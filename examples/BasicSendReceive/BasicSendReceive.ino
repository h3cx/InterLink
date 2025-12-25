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

  const uint8_t payload[] = {0x10, 0x20, 0x30};
  link.send(0x0002, 0x00, 0x01, payload, sizeof(payload));
}

void loop() {
  link.poll();

  interlink::Packet packet;
  if (link.readPacket(packet)) {
    Serial.print("RX cmd=0x");
    Serial.print(packet.cmd, HEX);
    Serial.print(" seq=");
    Serial.print(packet.seq);
    Serial.print(" len=");
    Serial.println(packet.len);
  }
}
