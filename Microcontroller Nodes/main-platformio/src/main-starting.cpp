#include <Arduino.h>
#include "NodeHandler.h"

NodeHandler::message_t outgoingMessage{};

void fillRandomString(char *buffer, size_t maxLen) {
    if (!buffer || maxLen < 2) return;

    // Choose characters from: A-Z, a-z, 0-9
    const char charset[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789";

    const size_t charsetSize = sizeof(charset) - 1; // exclude null terminator

    size_t len = maxLen - 1; // leave room for '\0'

    for (size_t i = 0; i < len; ++i) {
        uint32_t r = esp_random();        // full 32-bit random
        buffer[i] = charset[r % charsetSize];
    }

    buffer[len] = '\0'; // null terminate
}

uint8_t hexToNibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return 0;
}

bool parseMacString(const String &s, uint8_t out[6]) {
  // Expect "AA:BB:CC:DD:EE:FF" (17 chars)
  if (s.length() != 17) return false;

  for (int i = 0; i < 6; ++i) {
    char c1 = s[3 * i];
    char c2 = s[3 * i + 1];

    uint8_t hi = hexToNibble(c1);
    uint8_t lo = hexToNibble(c2);

    out[i] = (hi << 4) | lo;
  }
  return true;
}

void handleSerialCommands() {
  if (!Serial.available()) return;

  // Read one line (ending in '\n')
  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.length() == 0) return;

  // --- Command: --scan-all ---
  if (line.equalsIgnoreCase("--scan-all")) {
    Serial.println("[SERIAL] Command: --scan-all");

    NodeHandler::message_t msg;
    memset(&msg, 0, sizeof(msg));

    // startingMac = self
    NodeHandler &node = NodeHandler::instance();
    node.getSelfMac(msg.startingMac);

    // goalMac: you can choose a special broadcast MAC or handle
    // "scan all" as a logical command at the receiving side.
    // Here we just put all 0xFF as an example:
    for (int i = 0; i < 6; ++i) msg.goalMac[i] = 0xFF;

    Serial.println("Scan Push Queued.");

    return;
  }

  // --- Command: --send-data [mac] [text...] ---
  if (line.startsWith("--send-data")) {
    // Expected: --send-data 54:32:04:08:49:40 Hello world here
    Serial.println("[SERIAL] Command: --send-data");

    // Find spaces
    int firstSpace  = line.indexOf(' ');
    if (firstSpace < 0) {
      Serial.println("[SERIAL] Usage: --send-data [mac] [text]");
      return;
    }

    int secondSpace = line.indexOf(' ', firstSpace + 1);
    if (secondSpace < 0) {
      Serial.println("[SERIAL] Usage: --send-data [mac] [text]");
      return;
    }

    String macStr   = line.substring(firstSpace + 1, secondSpace);
    String textStr  = line.substring(secondSpace + 1);
    textStr.trim();

    if (textStr.length() == 0) {
      Serial.println("[SERIAL] Missing text payload");
      return;
    }

    uint8_t destMac[6];
    if (!parseMacString(macStr, destMac)) {
      Serial.print("[SERIAL] Invalid MAC: ");
      Serial.println(macStr);
      return;
    }

    // Build message_t
    NodeHandler::message_t msg;
    memset(&msg, 0, sizeof(msg));

    NodeHandler &node = NodeHandler::instance();
    node.getSelfMac(msg.startingMac);
    memcpy(msg.goalMac, destMac, 6);

    msg.msgId   = esp_random();
    msg.msgType = NodeHandler::REGULAR_MESSAGE;

    // Copy text into actualText (truncate if needed)
    strncpy(msg.payload, textStr.c_str(), sizeof(msg.payload) - 1);
    msg.payload[sizeof(msg.payload) - 1] = '\0';

    if (node.addMessageToSendQueue(msg)) {
      Serial.print("[SERIAL] Enqueued message to ");
      EspNowManager::printMac(destMac, Serial);
      Serial.print(" | Text: ");
      Serial.println(msg.payload);
    } else {
      Serial.println("[SERIAL] Failed to enqueue send-data (queue full?)");
    }

    return;
  }

  // Unknown command
  Serial.print("[SERIAL] Unknown command: ");
  Serial.println(line);
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  delay(1000);

  auto& nodeHandler = NodeHandler::instance();
  if (!nodeHandler.begin()) {
    Serial.println("Failed to initialize NodeHandler!");
    return; 
  } else Serial.println("NodeHandler initialized successfully.");
  delay(1000);

  if (!nodeHandler.addAllNeighborPeers()) {
    Serial.println("Failed to add neighbor peers!");
  } else {
    Serial.println("Neighbor peers added successfully.");
  }
  delay(1000);

  memcpy(outgoingMessage.startingMac, NodeHandler::TINYPICOADDR, 6);
  memcpy(outgoingMessage.goalMac, NodeHandler::C6M1ADDR, 6);

  delay(2000);
 
}

void loop() {
  // put your main code here, to run repeatedly:
  
  // Process serial commands
  handleSerialCommands();

  // tick 
  NodeHandler::instance().tick();

  delay(1000);
}