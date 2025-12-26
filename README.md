# InterLink

InterLink is an Arduino/ESP32-ready packet protocol library for byte streams (UART today, RS-485 later). It provides a resynchronizing parser, CRC-16/ARC validation, ACK/response helpers, retries/timeouts, and optional RS-485 direction control.

## Features

- Robust packet encoder/decoder with CRC-16/ARC
- Resynchronizing parser for noisy byte streams
- ACK/response helpers with retries/timeouts
- Non-blocking `poll()` API (FreeRTOS-friendly)
- Optional RS-485 direction control (DE/RE)
- AVR (Arduino Mega/Uno) and ESP32 compatible

## Packet overview

```
SYNC1  SYNC2  VER  FLAGS  CMD(LE)  SEQ  LEN  BODY  CRC16(LE)
0xAA   0x55   0x01  bbb   0x0000   0x00 0x00  ...   0x0000
```

For full details, see [Protocol.md](Protocol.md).

## Wiring notes

### UART

- Connect TX↔RX and common GND.
- Use `HardwareSerial` (e.g., `Serial1`) or `SoftwareSerial` where appropriate.

### RS-485 (half-duplex)

- Connect the transceiver DE/RE pin to a GPIO.
- Configure the DE pin in code using `setDirectionPin()`.
- Ensure the transceiver is powered and properly terminated.

## Usage

```cpp
#include <InterLink.h>

interlink::InterLink link(Serial1);

void setup() {
  Serial1.begin(115200);
  link.begin();
}

void loop() {
  link.poll();

  interlink::Packet packet;
  if (link.readPacket(packet)) {
    // handle packet
  }
}
```

## Parsing commands

Use `parseCommand` to decode a received `Packet` into a typed command payload. The parsed
payload is stored in a compact union, so only the active command data is present.

```cpp
interlink::Packet packet;
if (link.readPacket(packet)) {
  interlink::ParsedCommand cmd;
  if (interlink::parseCommand(packet, cmd)) {
    if (cmd.type == interlink::ParsedCommand::kPower) {
      uint8_t instruction = cmd.payload.power.instruction;
      // handle power command
    } else if (cmd.type == interlink::ParsedCommand::kMsg) {
      const char *message = cmd.payload.message.text;
      // handle informational message
    }
  }
}
```

## Examples

- `BasicSendReceive` — basic send/receive (no ACK)
- `PageChangeCommand` — sample command with payload
- `RequestRetry` — ACK request with retry handling
- `RS485HalfDuplex` — RS-485 direction control
- `ESP32FreeRTOSTask` — FreeRTOS-friendly polling loop
- `SelfTest` — CRC and round-trip checks

## Compatibility

- **AVR (Arduino Mega/Uno)**: supported.
- **ESP32**: supported with Arduino core; FreeRTOS-friendly non-blocking polling.

## Notes

- Default `INTERLINK_MAX_PAYLOAD` is 64 bytes. Override with a build flag or `#define` before including `InterLink.h`.
- Reserved flag bits (0x10–0x80) are cleared on transmit and ignored on receive.
