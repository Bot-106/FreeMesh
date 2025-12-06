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

    static const uint8_t* ALL_MACS[];
    static const uint8_t NUM_MACS;

    void (*onPacketReachedGoal)(const uint8_t *fromMac, const uint8_t *data, int len) = nullptr;

  private:
    bool parseMacString(const String &macStr, uint8_t out[6]);

    NodeHandler();
    NodeHandler(const NodeHandler&) = delete;
    NodeHandler& operator=(const NodeHandler&) = delete;

    static NodeHandler* _self;
    bool               _ready;
    uint8_t            _selfMac[6];
};