#pragma once

#include <Arduino.h>

#include "Crc16Arc.h"

namespace interlink {

constexpr uint8_t kSync1 = 0xAA;
constexpr uint8_t kSync2 = 0x55;
constexpr uint8_t kVersion = 0x01;

constexpr uint8_t kFlagAckReq = 0x01;
constexpr uint8_t kFlagIsAck = 0x02;
constexpr uint8_t kFlagIsResp = 0x04;
constexpr uint8_t kFlagIsErr = 0x08;

#ifndef INTERLINK_MAX_PAYLOAD
#define INTERLINK_MAX_PAYLOAD 64
#endif

#ifndef INTERLINK_RX_QUEUE
#define INTERLINK_RX_QUEUE 4
#endif

#ifndef INTERLINK_MAX_PENDING
#define INTERLINK_MAX_PENDING 4
#endif

struct Packet {
  uint8_t ver = kVersion;
  uint8_t flags = 0;
  uint16_t cmd = 0;
  uint8_t seq = 0;
  uint8_t len = 0;
  uint8_t body[INTERLINK_MAX_PAYLOAD] = {0};
  uint16_t crc = 0;
};

constexpr uint16_t kCmdPower = 0x0001;
constexpr uint16_t kCmdPage = 0x0002;
constexpr uint16_t kCmdMsg = 0x0003;
constexpr uint16_t kCmdWarn = 0x0004;
constexpr uint16_t kCmdErr = 0x0005;
constexpr uint16_t kCmdInit = 0x0006;
constexpr uint16_t kCmdInitComp = 0x0007;
constexpr uint16_t kCmdMoveLeft = 0x0008;
constexpr uint16_t kCmdMoveRight = 0x0009;
constexpr uint16_t kCmdMoveUp = 0x000A;
constexpr uint16_t kCmdMoveDown = 0x000B;
constexpr uint16_t kCmdBack = 0x000C;
constexpr uint16_t kCmdEnter = 0x000D;

struct ParsedCommand {
  enum Type : uint8_t {
    kUnknown = 0,
    kPower,
    kPage,
    kMsg,
    kWarn,
    kErr,
    kInit,
    kInitComp,
    kMoveLeft,
    kMoveRight,
    kMoveUp,
    kMoveDown,
    kBack,
    kEnter,
  };

  struct Power {
    uint8_t instruction = 0;
  };

  struct Page {
    uint8_t page = 0;
  };

  struct Message {
    char text[64] = {0};
  };

  struct Init {
    uint8_t percent = 0;
    char message[16] = {0};
  };

  uint8_t ver = kVersion;
  uint8_t flags = 0;
  uint16_t cmd = 0;
  uint8_t seq = 0;
  uint8_t len = 0;
  Type type = kUnknown;

  union Payload {
    Power power;
    Page page;
    Message message;
    Init init;
    Payload() : power() {}
  } payload;
};

bool parseCommand(const Packet &packet, ParsedCommand &command);

struct DropStats {
  uint32_t syncMisses = 0;
  uint32_t crcFailures = 0;
  uint32_t invalidVersion = 0;
  uint32_t lengthOverflow = 0;
  uint32_t packetsAccepted = 0;
  uint32_t queueOverflow = 0;
};

enum RequestStatus : uint8_t {
  kRequestPending = 0,
  kRequestAck = 1,
  kRequestResponse = 2,
  kRequestTimeout = 3,
  kRequestUnexpected = 4,
};

struct RequestResult {
  RequestStatus status = kRequestPending;
  uint16_t cmd = 0;
  uint8_t seq = 0;
  Packet response;
};

class InterLink {
 public:
  typedef void (*PacketCallback)(const Packet &packet, void *userData);

  /// Create an InterLink instance using the provided stream.
  explicit InterLink(Stream &stream);

  /// Optional begin hook (configures DE pin if enabled).
  void begin();
  /// Configure RS-485 direction control pin (DE/RE).
  void setDirectionPin(int8_t dePin, bool activeHigh = true, uint16_t turnaroundDelayMicros = 0);

  /// Register a packet callback invoked on each valid packet.
  void setPacketCallback(PacketCallback callback, void *userData = nullptr);

  /// Non-blocking parser entry point; call frequently.
  void poll();

  /// Returns true if a decoded packet is available.
  bool availablePacket() const;
  /// Read the next decoded packet.
  bool readPacket(Packet &packet);

  /// Send a packet with the provided fields.
  size_t send(uint16_t cmd, uint8_t flags, uint8_t seq, const uint8_t *body, uint8_t len);
  /// Send an ACK for the provided command and sequence.
  size_t sendAck(uint16_t cmd, uint8_t seq, bool isError = false, uint8_t errorCode = 0);
  /// Send a response for the provided command and sequence.
  size_t sendResponse(uint16_t cmd, uint8_t seq, const uint8_t *body, uint8_t len, bool isError = false);

  /// Send a request that expects an ACK/response with retry support.
  bool sendRequest(uint16_t cmd, uint8_t seq, const uint8_t *body, uint8_t len, uint32_t timeoutMs,
                   uint8_t retries);
  /// Blocking convenience call that waits for a response or timeout.
  bool sendRequestAndWait(uint16_t cmd, uint8_t seq, const uint8_t *body, uint8_t len, uint32_t timeoutMs,
                          uint8_t retries, RequestResult &result);

  /// Process retries/timeouts for pending requests.
  void tick(uint32_t nowMs);
  /// Retrieve completed request results.
  bool pollRequestResult(RequestResult &result);

  /// Access drop statistics.
  const DropStats &stats() const { return dropStats_; }

 private:
  enum ParserState : uint8_t {
    kSeekSync1,
    kSeekSync2,
    kReadFixedHeader,
    kReadBody,
    kReadCrc,
  };

  struct PendingRequest {
    bool active = false;
    uint16_t cmd = 0;
    uint8_t seq = 0;
    uint8_t retriesLeft = 0;
    uint32_t timeoutMs = 0;
    uint32_t lastSendMs = 0;
    uint8_t payloadLen = 0;
    uint8_t payload[INTERLINK_MAX_PAYLOAD] = {0};
  };

  struct ResultQueue {
    RequestResult results[INTERLINK_MAX_PENDING];
    uint8_t head = 0;
    uint8_t tail = 0;
    uint8_t count = 0;
  };

  Stream &stream_;
  int8_t dePin_ = -1;
  bool deActiveHigh_ = true;
  uint16_t turnaroundDelayMicros_ = 0;

  PacketCallback callback_ = nullptr;
  void *callbackUserData_ = nullptr;

  Packet rxQueue_[INTERLINK_RX_QUEUE];
  uint8_t rxHead_ = 0;
  uint8_t rxTail_ = 0;
  uint8_t rxCount_ = 0;

  ParserState state_ = kSeekSync1;
  uint8_t fixedIndex_ = 0;
  uint8_t bodyIndex_ = 0;
  uint8_t crcIndex_ = 0;
  Packet current_;
  uint8_t crcBytes_[2] = {0};

  DropStats dropStats_;

  PendingRequest pending_[INTERLINK_MAX_PENDING];
  ResultQueue resultQueue_;

  void resetParser();
  void emitPacket(const Packet &packet);
  uint16_t computeCrc(const Packet &packet) const;

  bool enqueueResult(const RequestResult &result);
  void handleIncomingPacket(const Packet &packet);
  PendingRequest *findPending(uint16_t cmd, uint8_t seq);
  bool queueHasSpace() const;

  size_t writeBytes(const uint8_t *data, size_t length);
};

}  // namespace interlink
