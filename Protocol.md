# InterLink Protocol

## Packet layout

All multi-byte fields are little-endian.

| Field | Size | Description |
| --- | --- | --- |
| SYNC1 | 1 | `0xAA` |
| SYNC2 | 1 | `0x55` |
| VER | 1 | Protocol version (`0x01`) |
| FLAGS | 1 | Flag bitfield |
| CMD | 2 | Command (`uint16`, little-endian) |
| SEQ | 1 | Sequence (`uint8`, wraps 0..255) |
| LEN | 1 | Payload length (`0..MAX_PAYLOAD`) |
| BODY | `LEN` | Payload bytes |
| CRC16 | 2 | CRC-16/ARC over `VER..BODY` (little-endian) |

Minimum packet length (LEN=0): 10 bytes.

## Flags

| Flag | Value | Meaning |
| --- | --- | --- |
| ACK_REQ | `0x01` | Sender requests ACK/response |
| IS_ACK | `0x02` | Frame is an ACK |
| IS_RESP | `0x04` | Frame is a response |
| IS_ERR | `0x08` | ACK/response indicates error |

Reserved bits `0x10..0x80` must be zero on transmit and are ignored on receive.

## CRC (CRC-16/ARC)

Parameters:

- Width: 16
- Poly: `0x8005` (reflected `0xA001`)
- Init: `0x0000`
- RefIn: true
- RefOut: true
- XorOut: `0x0000`

Coverage: `VER..BODY` inclusive (SYNC bytes excluded). CRC appended as little-endian.

### Example CRC vector

Input (ASCII `"123456789"`):

```
31 32 33 34 35 36 37 38 39
```

Expected CRC-16/ARC: `0xBB3D` (little-endian bytes `3D BB`).

## Validation rules

A packet is valid only if:

- SYNC bytes match
- VER is supported (`0x01`)
- LEN ≤ `MAX_PAYLOAD`
- CRC matches

If any check fails: discard bytes and resynchronize by searching for the next `0xAA 0x55` sequence.

## ACK/response behavior

- If `ACK_REQ` is set on a frame, the receiver should reply with either:
  - ACK (`IS_ACK`) or
  - Response (`IS_RESP`)
- Reply must echo `CMD` and `SEQ`.
- On error, set `IS_ERR` and optionally include an error code in the payload.

## Endianness rules

- Multi-byte fields are little-endian (CMD, CRC).
- CRC is appended as little-endian.
