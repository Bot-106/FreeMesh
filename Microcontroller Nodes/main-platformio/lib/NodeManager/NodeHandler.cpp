#include "NodeHandler.h"

NodeHandler* NodeHandler::_self = nullptr;

const uint8_t NodeHandler::TINYPICOADDR[6] = {0xD4, 0xD4, 0xDA, 0x83, 0x9F, 0xEC};
const uint8_t NodeHandler::C6M1ADDR[6]    = {0x54, 0x32, 0x04, 0x08, 0x49, 0x40};
const uint8_t NodeHandler::S3C1ADDR[6]    = {0x34, 0x85, 0x18, 0x6C, 0xF6, 0x9C};

const uint8_t* NodeHandler::ALL_MACS[] = {
    NodeHandler::TINYPICOADDR,
    NodeHandler::C6M1ADDR,
    NodeHandler::S3C1ADDR
};
const uint8_t NodeHandler::NUM_MACS = sizeof(NodeHandler::ALL_MACS) / sizeof(NodeHandler::ALL_MACS[0]);

bool NodeHandler::parseMacString(const String &macStr, uint8_t out[6]) {
  if (macStr.length() != 17) return false; // must be "AA:BB:CC:DD:EE:FF"

  for (int i = 0; i < 6; i++) {
    char high = macStr[3 * i];
    char low  = macStr[3 * i + 1];

    uint8_t hiVal = strtoul(String(high).c_str(), nullptr, 16);
    uint8_t loVal = strtoul(String(low).c_str(),  nullptr, 16);

    out[i] = (hiVal << 4) | loVal;
  }
  return true;
}

NodeHandler::NodeHandler()
    : _ready(false)
{
    memset(_selfMac, 0, sizeof(_selfMac));
}

NodeHandler& NodeHandler::instance() {
    static NodeHandler handler;
    return handler;
}

bool NodeHandler::begin() {
    if (_ready) return true;

    _self = this;

    auto &espNow = EspNowManager::instance();
    if (!espNow.begin()) {
        _ready = false;
        return false;
    }
    delay(1000); // Allow some time for ESP-NOW to initialize

    _self->parseMacString(WiFi.macAddress(), _selfMac);
    espNow.setReceiveCallback(&NodeHandler::onEspNowReceive);

    _ready = true;
    return true;
}

bool NodeHandler::addAllNeighborPeers() {
    if (!_ready) return false;

    auto &espNow = EspNowManager::instance();

    for (size_t i = 0; i < NUM_MACS; i++) {
        const uint8_t* peerMac = ALL_MACS[i];
        if (memcmp(peerMac, _selfMac, 6) == 0) {
            continue; // skip self
        }
        espNow.addPeer(peerMac);
    }
    return true;
}

bool NodeHandler::sendPayloadToMac(const uint8_t goalMac[6],
                         const uint8_t *payload,
                         uint16_t len)
{
    if (!_ready) {
        return false;
    }

    auto &espNow = EspNowManager::instance();
    return espNow.send(goalMac, payload, len);
}

bool NodeHandler::sendMessageToAll(const message_t &msg) {
    if (!_ready) {
        return false;
    }
    bool allSent = true;

    for (size_t i = 0; i < NUM_MACS; i++) {
        const uint8_t* peerMac = ALL_MACS[i];
        if (memcmp(peerMac, _selfMac, 6) == 0) {
            continue; // skip self
        }
        bool sent = NodeHandler::sendPayloadToMac(peerMac, reinterpret_cast<const uint8_t*>(&msg), sizeof(message_t));
        if (!sent) {
            allSent = false;
        }
    }
    return allSent;
}

void NodeHandler::getSelfMac(uint8_t out[6]) const {
  if (!out) return;
  memcpy(out, _selfMac, 6);
}

static inline bool macEqual(const uint8_t *a, const uint8_t *b) {
    for (int i = 0; i < 6; ++i) {
        if (a[i] != b[i]) {
            Serial.println("mismatch at byte " + String(i) + ": " + String(a[i], HEX) + " != " + String(b[i], HEX));
            return false;
        }
    }
    return true;
}

void NodeHandler::onEspNowReceive(const uint8_t *fromMac,
                                 const uint8_t *data,
                                 int len)
{
  NodeHandler::message_t msg;
  bool isSelfGoalMac = macEqual(msg.goalMac, NodeHandler::instance()._selfMac);
  memcpy(&msg, data, sizeof(NodeHandler::message_t));

  if (NodeHandler::instance().hasSeenMessageId(msg.msgId)) {
      // Duplicate message, ignore
      Serial.print("DUPLICATE MSG ID ");
      Serial.println(msg.msgId);
      return;
  } else {
        NodeHandler::instance().rememberMessageId(msg.msgId);
  }

  if (msg.msgType == SCAN_PUSH) {
    if (macEqual(msg.goalMac, NodeHandler::instance()._selfMac)) {
        // This scan push is for us; respond with SCAN_RESPONSE
        NodeHandler::message_t responseMsg;
        memcpy(responseMsg.startingMac, NodeHandler::instance()._selfMac, 6);
        memcpy(responseMsg.goalMac, NodeHandler::TINYPICOADDR, 6);
        responseMsg.msgId = esp_random();
        responseMsg.msgType = SCAN_RESPONSE;
        memset(responseMsg.payload, 0, sizeof(responseMsg.payload));

        NodeHandler::instance().addMessageToSendQueue(responseMsg);
    } else {
        NodeHandler::message_t responseMsg;
        memcpy(responseMsg.startingMac, NodeHandler::instance()._selfMac, 6);
        memcpy(responseMsg.goalMac, msg.goalMac, 6);
        responseMsg.msgId = msg.msgId;
        responseMsg.msgType = SCAN_PUSH;
        memset(responseMsg.payload, 0, sizeof(responseMsg.payload));

        NodeHandler::instance().addMessageToSendQueue(responseMsg);
    }
  } else if (msg.msgType == SCAN_RESPONSE) {
    if (macEqual(msg.goalMac, NodeHandler::TINYPICOADDR) && macEqual(NodeHandler::TINYPICOADDR, NodeHandler::instance()._selfMac)) {
        // This scan response is for us
        Serial.print("--scan_response--");
        EspNowManager::instance().printMac(msg.startingMac, Serial);
        Serial.println();
    } else if (macEqual(msg.goalMac, NodeHandler::instance()._selfMac)) {
        return;
    } else {
        // Forward the SCAN_RESPONSE to its goal
        NodeHandler::message_t forwardMsg;
        memcpy(forwardMsg.startingMac, msg.startingMac, 6);
        memcpy(forwardMsg.goalMac, msg.goalMac, 6);
        forwardMsg.msgId = msg.msgId;
        forwardMsg.msgType = SCAN_RESPONSE;
        memset(forwardMsg.payload, 0, sizeof(forwardMsg.payload));

        NodeHandler::instance().addMessageToSendQueue(forwardMsg);
    }
  } else {
    // Regular message
    if (isSelfGoalMac) {
        // Message reached its goal
        if (NodeHandler::instance().onPacketReachedGoal) {
            NodeHandler::instance().onPacketReachedGoal(fromMac, data, len);
        }
    } else {
        // Forward the message to its goal
        NodeHandler::message_t forwardMsg;
        memcpy(forwardMsg.startingMac, msg.startingMac, 6);
        memcpy(forwardMsg.goalMac, msg.goalMac, 6);
        forwardMsg.msgId = msg.msgId;
        forwardMsg.msgType = REGULAR_MESSAGE;
        memcpy(forwardMsg.payload, msg.payload, sizeof(msg.payload));

        NodeHandler::instance().addMessageToSendQueue(forwardMsg);
    }
  }
}

bool NodeHandler::addMessageToSendQueue(const message_t &msg) {
    if (!_ready) return false;

    // Queue full?
    if (_outCount >= OUT_QUEUE_SIZE) {
        // You could drop or overwrite oldest; here we just fail
        return false;
    }

    // Copy message into queue at tail
    _outQueue[_outTail] = msg;  // struct copy
    _outTail = (_outTail + 1) % OUT_QUEUE_SIZE;
    _outCount++;

    return true;
}

void NodeHandler::tick() {
    if (!_ready) return;
    if (_outCount == 0) return;  // nothing to do

    // Only send one per tick to avoid spamming
    message_t &msg = _outQueue[_outHead];

    bool ok = sendMessageToAll(msg);

    // Whether it succeeds or fails, we pop it from the queue.
    // If you want retries, you could only pop on success instead.
    _outHead = (_outHead + 1) % OUT_QUEUE_SIZE;
    _outCount--;
}

void NodeHandler::rememberMessageId(uint32_t msgId) {
    // If we have space, put it at (head + count)
    if (_pastCount < MAX_PAST_IDS) {
        uint8_t idx = (_pastHead + _pastCount) % MAX_PAST_IDS;
        _pastIds[idx] = msgId;
        _pastCount++;
    } else {
        // Queue full: overwrite the oldest entry at _pastHead
        _pastIds[_pastHead] = msgId;
        _pastHead = (_pastHead + 1) % MAX_PAST_IDS;
    }
}

bool NodeHandler::hasSeenMessageId(uint32_t msgId) const {
    for (uint8_t i = 0; i < _pastCount; ++i) {
        uint8_t idx = (_pastHead + i) % MAX_PAST_IDS;
        if (_pastIds[idx] == msgId) {
            return true;
        }
    }
    return false;
}
