#include "NodeHandler.h"

NodeHandler* NodeHandler::_self = nullptr;

const uint8_t NodeHandler::TINYPICOADDR[6] = {0xD4, 0xD4, 0xDA, 0x83, 0x9F, 0xEC};
const uint8_t NodeHandler::C6M1ADDR[6]    = {0x54, 0x32, 0x04, 0x08, 0x49, 0x40};

const uint8_t* NodeHandler::ALL_MACS[] = {
    NodeHandler::TINYPICOADDR,
    NodeHandler::C6M1ADDR
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

  Serial.print("RECV | From: ");
  EspNowManager::printMac(fromMac, Serial);
  Serial.print(" | Goal: ");
  EspNowManager::printMac(msg.goalMac, Serial);
  Serial.print(" | Self: ");
  EspNowManager::printMac(NodeHandler::instance()._selfMac, Serial);
  Serial.print(" | Msg ID: ");
  Serial.print(msg.msgId); 
  Serial.print(" | At Destination: ");
  Serial.print(isSelfGoalMac);
  Serial.println();

  if (isSelfGoalMac) NodeHandler::instance().onPacketReachedGoal(fromMac, data, len);
}