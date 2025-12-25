#include <InterLink.h>

#if defined(HAVE_HWSERIAL1)
#define LINK_SERIAL Serial1
#else
#define LINK_SERIAL Serial
#endif

interlink::InterLink link(LINK_SERIAL);

uint8_t nextSeq = 0;
uint32_t lastSendMs = 0;

void setup() {
  Serial.begin(115200);
  LINK_SERIAL.begin(115200);
  link.begin();
}

void loop() {
  link.poll();
  link.tick(millis());

  if (millis() - lastSendMs > 2000) {
    uint8_t payload[] = {0x42};
    link.sendRequest(0x0100, nextSeq++, payload, sizeof(payload), 250, 3);
    lastSendMs = millis();
  }

  interlink::RequestResult result;
  if (link.pollRequestResult(result)) {
    Serial.print("Request result seq=");
    Serial.print(result.seq);
    Serial.print(" status=");
    Serial.println(result.status);
  }
}
