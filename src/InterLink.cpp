#include "InterLink.h"

namespace interlink {

InterLink::InterLink(Stream &stream) : stream_(stream) {}

void InterLink::begin() {
  if (dePin_ >= 0) {
    pinMode(dePin_, OUTPUT);
    digitalWrite(dePin_, deActiveHigh_ ? LOW : HIGH);
  }
}

void InterLink::setDirectionPin(int8_t dePin, bool activeHigh, uint16_t turnaroundDelayMicros) {
  dePin_ = dePin;
  deActiveHigh_ = activeHigh;
  turnaroundDelayMicros_ = turnaroundDelayMicros;
  if (dePin_ >= 0) {
    pinMode(dePin_, OUTPUT);
    digitalWrite(dePin_, deActiveHigh_ ? LOW : HIGH);
  }
}

void InterLink::setPacketCallback(PacketCallback callback, void *userData) {
  callback_ = callback;
  callbackUserData_ = userData;
}

void InterLink::poll() {
  while (stream_.available() > 0) {
    int value = stream_.read();
    if (value < 0) {
      return;
    }
    uint8_t byte = static_cast<uint8_t>(value);

    switch (state_) {
      case kSeekSync1:
        if (byte == kSync1) {
          state_ = kSeekSync2;
        } else {
          dropStats_.syncMisses++;
        }
        break;
      case kSeekSync2:
        if (byte == kSync2) {
          state_ = kReadFixedHeader;
          fixedIndex_ = 0;
        } else if (byte == kSync1) {
          state_ = kSeekSync2;
        } else {
          dropStats_.syncMisses++;
          state_ = kSeekSync1;
        }
        break;
      case kReadFixedHeader:
        switch (fixedIndex_) {
          case 0:
            current_.ver = byte;
            break;
          case 1:
            current_.flags = byte;
            break;
          case 2:
            current_.cmd = byte;
            break;
          case 3:
            current_.cmd |= static_cast<uint16_t>(byte) << 8;
            break;
          case 4:
            current_.seq = byte;
            break;
          case 5:
            current_.len = byte;
            break;
          default:
            break;
        }
        fixedIndex_++;
        if (fixedIndex_ >= 6) {
          if (current_.ver != kVersion) {
            dropStats_.invalidVersion++;
            resetParser();
            break;
          }
          if (current_.len > INTERLINK_MAX_PAYLOAD) {
            dropStats_.lengthOverflow++;
            resetParser();
            break;
          }
          bodyIndex_ = 0;
          if (current_.len == 0) {
            state_ = kReadCrc;
            crcIndex_ = 0;
          } else {
            state_ = kReadBody;
          }
        }
        break;
      case kReadBody:
        current_.body[bodyIndex_++] = byte;
        if (bodyIndex_ >= current_.len) {
          state_ = kReadCrc;
          crcIndex_ = 0;
        }
        break;
      case kReadCrc:
        crcBytes_[crcIndex_++] = byte;
        if (crcIndex_ >= 2) {
          current_.crc = static_cast<uint16_t>(crcBytes_[0]) |
                         (static_cast<uint16_t>(crcBytes_[1]) << 8);
          uint16_t computed = computeCrc(current_);
          if (computed == current_.crc) {
            dropStats_.packetsAccepted++;
            emitPacket(current_);
          } else {
            dropStats_.crcFailures++;
          }
          resetParser();
        }
        break;
    }
  }
}

bool InterLink::availablePacket() const { return rxCount_ > 0; }

bool InterLink::readPacket(Packet &packet) {
  if (rxCount_ == 0) {
    return false;
  }
  packet = rxQueue_[rxTail_];
  rxTail_ = (rxTail_ + 1) % INTERLINK_RX_QUEUE;
  rxCount_--;
  return true;
}

size_t InterLink::send(uint16_t cmd, uint8_t flags, uint8_t seq, const uint8_t *body, uint8_t len) {
  if (len > INTERLINK_MAX_PAYLOAD) {
    return 0;
  }

  Packet packet;
  packet.ver = kVersion;
  packet.flags = flags & 0x0F;
  packet.cmd = cmd;
  packet.seq = seq;
  packet.len = len;
  if (len > 0 && body != nullptr) {
    memcpy(packet.body, body, len);
  }
  packet.crc = computeCrc(packet);

  uint8_t header[8];
  header[0] = kSync1;
  header[1] = kSync2;
  header[2] = packet.ver;
  header[3] = packet.flags;
  header[4] = static_cast<uint8_t>(packet.cmd & 0xFF);
  header[5] = static_cast<uint8_t>((packet.cmd >> 8) & 0xFF);
  header[6] = packet.seq;
  header[7] = packet.len;

  uint8_t crcBytes[2];
  crcBytes[0] = static_cast<uint8_t>(packet.crc & 0xFF);
  crcBytes[1] = static_cast<uint8_t>((packet.crc >> 8) & 0xFF);

  size_t written = 0;
  if (dePin_ >= 0) {
    digitalWrite(dePin_, deActiveHigh_ ? HIGH : LOW);
  }

  written += writeBytes(header, sizeof(header));
  if (packet.len > 0) {
    written += writeBytes(packet.body, packet.len);
  }
  written += writeBytes(crcBytes, sizeof(crcBytes));

  stream_.flush();
  if (turnaroundDelayMicros_ > 0) {
    delayMicroseconds(turnaroundDelayMicros_);
  }
  if (dePin_ >= 0) {
    digitalWrite(dePin_, deActiveHigh_ ? LOW : HIGH);
  }

  return written;
}

size_t InterLink::sendAck(uint16_t cmd, uint8_t seq, bool isError, uint8_t errorCode) {
  uint8_t flags = kFlagIsAck;
  uint8_t payload[1] = {errorCode};
  uint8_t len = 0;
  if (isError) {
    flags |= kFlagIsErr;
    len = 1;
  }
  return send(cmd, flags, seq, len ? payload : nullptr, len);
}

size_t InterLink::sendResponse(uint16_t cmd, uint8_t seq, const uint8_t *body, uint8_t len, bool isError) {
  uint8_t flags = kFlagIsResp;
  if (isError) {
    flags |= kFlagIsErr;
  }
  return send(cmd, flags, seq, body, len);
}

bool InterLink::sendRequest(uint16_t cmd, uint8_t seq, const uint8_t *body, uint8_t len, uint32_t timeoutMs,
                            uint8_t retries) {
  if (len > INTERLINK_MAX_PAYLOAD) {
    return false;
  }

  PendingRequest *slot = nullptr;
  for (uint8_t i = 0; i < INTERLINK_MAX_PENDING; ++i) {
    if (!pending_[i].active) {
      slot = &pending_[i];
      break;
    }
  }
  if (slot == nullptr) {
    return false;
  }

  slot->active = true;
  slot->cmd = cmd;
  slot->seq = seq;
  slot->retriesLeft = retries;
  slot->timeoutMs = timeoutMs;
  slot->lastSendMs = millis();
  slot->payloadLen = len;
  if (len > 0 && body != nullptr) {
    memcpy(slot->payload, body, len);
  }

  size_t written = send(cmd, kFlagAckReq, seq, body, len);
  if (written == 0) {
    slot->active = false;
    return false;
  }
  return true;
}

bool InterLink::sendRequestAndWait(uint16_t cmd, uint8_t seq, const uint8_t *body, uint8_t len, uint32_t timeoutMs,
                                   uint8_t retries, RequestResult &result) {
  if (!sendRequest(cmd, seq, body, len, timeoutMs, retries)) {
    return false;
  }

  uint32_t start = millis();
  while (true) {
    poll();
    tick(millis());
    if (pollRequestResult(result)) {
      if (result.seq == seq && result.cmd == cmd) {
        return true;
      }
      enqueueResult(result);
    }
    if (millis() - start >= timeoutMs * (static_cast<uint32_t>(retries) + 1)) {
      result.status = kRequestTimeout;
      result.cmd = cmd;
      result.seq = seq;
      return false;
    }
    delay(1);
  }
}

void InterLink::tick(uint32_t nowMs) {
  for (uint8_t i = 0; i < INTERLINK_MAX_PENDING; ++i) {
    PendingRequest &req = pending_[i];
    if (!req.active) {
      continue;
    }
    if (nowMs - req.lastSendMs >= req.timeoutMs) {
      if (req.retriesLeft > 0) {
        req.retriesLeft--;
        req.lastSendMs = nowMs;
        send(req.cmd, kFlagAckReq, req.seq, req.payload, req.payloadLen);
      } else {
        RequestResult result;
        result.status = kRequestTimeout;
        result.cmd = req.cmd;
        result.seq = req.seq;
        enqueueResult(result);
        req.active = false;
      }
    }
  }
}

bool InterLink::pollRequestResult(RequestResult &result) {
  if (resultQueue_.count == 0) {
    return false;
  }
  result = resultQueue_.results[resultQueue_.tail];
  resultQueue_.tail = (resultQueue_.tail + 1) % INTERLINK_MAX_PENDING;
  resultQueue_.count--;
  return true;
}

void InterLink::resetParser() {
  state_ = kSeekSync1;
  fixedIndex_ = 0;
  bodyIndex_ = 0;
  crcIndex_ = 0;
  current_ = Packet();
}

void InterLink::emitPacket(const Packet &packet) {
  handleIncomingPacket(packet);
  if (callback_) {
    callback_(packet, callbackUserData_);
    return;
  }
  if (queueHasSpace()) {
    rxQueue_[rxHead_] = packet;
    rxHead_ = (rxHead_ + 1) % INTERLINK_RX_QUEUE;
    rxCount_++;
  } else {
    dropStats_.queueOverflow++;
  }
}

uint16_t InterLink::computeCrc(const Packet &packet) const {
  uint16_t crc = 0x0000;
  crc = Crc16Arc::update(crc, packet.ver);
  crc = Crc16Arc::update(crc, packet.flags);
  crc = Crc16Arc::update(crc, static_cast<uint8_t>(packet.cmd & 0xFF));
  crc = Crc16Arc::update(crc, static_cast<uint8_t>((packet.cmd >> 8) & 0xFF));
  crc = Crc16Arc::update(crc, packet.seq);
  crc = Crc16Arc::update(crc, packet.len);
  for (uint8_t i = 0; i < packet.len; ++i) {
    crc = Crc16Arc::update(crc, packet.body[i]);
  }
  return crc;
}

bool InterLink::enqueueResult(const RequestResult &result) {
  if (resultQueue_.count >= INTERLINK_MAX_PENDING) {
    return false;
  }
  resultQueue_.results[resultQueue_.head] = result;
  resultQueue_.head = (resultQueue_.head + 1) % INTERLINK_MAX_PENDING;
  resultQueue_.count++;
  return true;
}

void InterLink::handleIncomingPacket(const Packet &packet) {
  if ((packet.flags & (kFlagIsAck | kFlagIsResp)) == 0) {
    return;
  }

  PendingRequest *req = findPending(packet.cmd, packet.seq);
  if (req == nullptr) {
    RequestResult result;
    result.status = kRequestUnexpected;
    result.cmd = packet.cmd;
    result.seq = packet.seq;
    result.response = packet;
    enqueueResult(result);
    return;
  }

  RequestResult result;
  result.cmd = packet.cmd;
  result.seq = packet.seq;
  result.response = packet;
  if (packet.flags & kFlagIsAck) {
    result.status = kRequestAck;
  } else if (packet.flags & kFlagIsResp) {
    result.status = kRequestResponse;
  }
  enqueueResult(result);
  req->active = false;
}

InterLink::PendingRequest *InterLink::findPending(uint16_t cmd, uint8_t seq) {
  for (uint8_t i = 0; i < INTERLINK_MAX_PENDING; ++i) {
    if (pending_[i].active && pending_[i].cmd == cmd && pending_[i].seq == seq) {
      return &pending_[i];
    }
  }
  return nullptr;
}

bool InterLink::queueHasSpace() const { return rxCount_ < INTERLINK_RX_QUEUE; }

size_t InterLink::writeBytes(const uint8_t *data, size_t length) { return stream_.write(data, length); }

}  // namespace interlink
