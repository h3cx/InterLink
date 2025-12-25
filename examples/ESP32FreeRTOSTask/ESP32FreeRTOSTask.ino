#include <InterLink.h>

#if defined(ESP32)

interlink::InterLink link(Serial2);

void pollTask(void *params) {
  while (true) {
    link.poll();
    link.tick(millis());
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, 16, 17);
  link.begin();

  xTaskCreatePinnedToCore(pollTask, "interlink-poll", 4096, nullptr, 1, nullptr, 1);
}

void loop() {
  interlink::Packet packet;
  if (link.readPacket(packet)) {
    Serial.println("Packet received");
  }
  delay(10);
}

#else

void setup() {}
void loop() {}

#endif
