#include <Arduino.h>
#include "NodeHandler.h"

NodeHandler::message_t outgoingMessage{};

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

}

void loop() {
  // put your main code here, to run repeatedly:
  outgoingMessage.msgId = esp_random();
  NodeHandler::instance().sendMessageToAll(outgoingMessage);

  delay(1000);
}