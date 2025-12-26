# InterLink Command Specification

This document defines all **CMD values**, their semantic meaning, payload layout, and expected `LEN` values for the **InterLink packet protocol**.

This file is **authoritative** and aligned exactly with the InterLink packet framing specification:

```
SYNC1  SYNC2  VER  FLAGS  CMD  SEQ  LEN  BODY  CRC16
```
- `CMD` is a `uint16` (little-endian)
- `LEN` is a `uint8` and must match the payload size defined below
- `BODY` layout is command-specific
- CRC is validated over `VER .. BODY`

---

## General Rules

- All packets must use `VER = 0x01`
- All multi-byte values are **little-endian**
- Commands with `LEN = 0` **must still include the LEN field**
- Commands that define `char[N]` payloads use **fixed-size, null-terminated C strings**
- If a string is shorter than `N`, remaining bytes must be padded with `\0`
- Receivers **must validate `LEN`** before accessing `BODY`

---

## Command Table

| CMD (hex) | Name       | Purpose                     | BODY Layout                                      | LEN |
|-----------|------------|-----------------------------|--------------------------------------------------|-----|
| `0x0001`  | Power      | Power control               | `uint8 instruction`                              | 1   |
| `0x0002`  | Page       | Change active UI page       | `uint8 page`                                     | 1   |
| `0x0003`  | Msg        | Informational message       | `char[64]` (null-terminated)                     | 64  |
| `0x0004`  | Warn       | Warning message             | `char[64]` (null-terminated)                     | 64  |
| `0x0005`  | Err        | Error message               | `char[64]` (null-terminated)                     | 64  |
| `0x0006`  | Init       | Init progress update        | `uint8 percent` + `char[16]` message             | 17  |
| `0x0007`  | InitComp   | Init complete               | *(no body)*                                      | 0   |
| `0x0008`  | MoveLeft  | UI navigation (left)        | *(no body)*                                      | 0   |
| `0x0009`  | MoveRight | UI navigation (right)       | *(no body)*                                      | 0   |
| `0x000A`  | MoveUp    | UI navigation (up)          | *(no body)*                                      | 0   |
| `0x000B`  | MoveDown  | UI navigation (down)        | *(no body)*                                      | 0   |
| `0x000C`  | Back      | UI navigation (back)        | *(no body)*                                      | 0   |
| `0x000D`  | Enter     | UI navigation (enter)       | *(no body)*                                      | 0   |

---

## Command Definitions

### CMD `0x0001` — Power
**LEN:** 1  
**BODY:**

```
uint8 instruction
```

| Value | Meaning     |
|-------|-------------|
| `1`   | Power On    |
| `2`   | Power Off   |
| `3`   | Restart    |

---

### CMD `0x0002` — Page
**LEN:** 1  
**BODY:**

```
uint8 page
```

| Value | Page          |
|-------|---------------|
| `0`   | Default       |
| `1`   | Pit Interlock |
| `2`   | Console       |

---

### CMD `0x0003` — Msg
**LEN:** 64  
**BODY:**

```
char message[64]
```

General informational message.

---

### CMD `0x0004` — Warn
**LEN:** 64  
**BODY:**

```
char message[64]
```

Warning-level message.

---

### CMD `0x0005` — Err
**LEN:** 64  
**BODY:**

```
char message[64]
```

Error-level message.

---

### CMD `0x0006` — Init
**LEN:** 17  
**BODY:**

```
uint8 percent
char   message[16]
```

- `percent` range: 0–100
- `message` is a short status string

---

### CMD `0x0007` — InitComp
**LEN:** 0  
**BODY:** none  
Signals completion of initialization.

---

## Navigation Commands (LEN = 0)

These commands have no payload and represent discrete UI input events.

| CMD (hex) | Action     |
|-----------|------------|
| `0x0008`  | Move Left  |
| `0x0009`  | Move Right |
| `0x000A`  | Move Up    |
| `0x000B`  | Move Down  |
| `0x000C`  | Back       |
| `0x000D`  | Enter     |

---

## FLAGS Usage (Reference)

This command table does not mandate specific FLAGS usage, but the following conventions are recommended:

- `ACK_REQ` should be set for:
  - Power commands
  - Page changes
- `ACK_REQ` should be unset for:
  - Messages
  - Init progress updates
  - Navigation commands

---

## Versioning

- This document applies to `VER = 0x01`
- Any changes to BODY layout or LEN values **require a new VER**

---

End of specification.
