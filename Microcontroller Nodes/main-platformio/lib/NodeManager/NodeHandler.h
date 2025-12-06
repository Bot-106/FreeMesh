#include <EspNowManager.h>

#pragma once

#include <Arduino.h>
#include "EspNowManager.h"
#include <esp_system.h>

class NodeHandler {
  public:
    // Singleton instance
    static NodeHandler& instance();

    struct message_t {
      uint8_t startingMac[6];
      uint8_t goalMac[6];
      uint32_t msgId;
      uint8_t msgType;
      char payload[100];
    };

    // Initialize NodeHandler + underlying EspNowManager.
    bool begin();

    bool addAllNeighborPeers();

    static void onEspNowReceive(const uint8_t *fromMac,
                                const uint8_t *data,
                                int len);

    // Send a payload to a specific target MAC.
    // The message is flooded to all known peers.
    bool sendPayloadToMac(const uint8_t goalMac[6],
              const uint8_t *payload,
              uint16_t len);
    bool sendMessageToAll(const message_t &msg);
              
    void getSelfMac(uint8_t out[6]) const;

    bool isReady() const { return _ready; }

    static const uint8_t TINYPICOADDR[6];
    static const uint8_t C6M1ADDR[6];
    static const uint8_t S3C1ADDR[6];

    static const uint8_t* ALL_MACS[];
    static const uint8_t NUM_MACS;

    void (*onPacketReachedGoal)(const uint8_t *fromMac, const uint8_t *data, int len) = nullptr;

    bool addMessageToSendQueue(const message_t &msg);
    void tick();

    enum MsgType : uint8_t {
      SCAN_PUSH = 0,
      SCAN_RESPONSE = 1,
      REGULAR_MESSAGE = 10
    };

  private:
    bool parseMacString(const String &macStr, uint8_t out[6]);

    NodeHandler();
    NodeHandler(const NodeHandler&) = delete;
    NodeHandler& operator=(const NodeHandler&) = delete;

    static NodeHandler* _self;
    bool               _ready;
    uint8_t            _selfMac[6];

    // --- Outgoing message queue ---
    static const uint8_t OUT_QUEUE_SIZE = 16;

    message_t _outQueue[OUT_QUEUE_SIZE];
    uint8_t   _outHead = 0;  // index of next message to send
    uint8_t   _outTail = 0;  // index of next free slot
    uint8_t   _outCount = 0; // how many entries in the queue

    // --- Recent message ID queue (simple dedup window) ---
    static const uint8_t MAX_PAST_IDS = 16;

    uint32_t _pastIds[MAX_PAST_IDS];
    uint8_t  _pastHead = 0;   // index of oldest entry
    uint8_t  _pastCount = 0;  // how many valid entries

    // Add an ID to the history (overwrites oldest if full)
    void rememberMessageId(uint32_t msgId);

    // Check if we've seen this ID in the last MAX_PAST_IDS entries
    bool hasSeenMessageId(uint32_t msgId) const;
};