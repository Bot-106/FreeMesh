#include <Arduino.h>
#include "NodeHandler.h"

NodeHandler::message_t outgoingMessage{};

bool ledState = false;
void onPacketReachedGoal(const uint8_t *fromMac, const uint8_t *data, int len) {
  digitalWrite(12, ledState ? HIGH : LOW);
  ledState = !ledState;
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(12, OUTPUT);
  digitalWrite(12, LOW);
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
  nodeHandler.onPacketReachedGoal = onPacketReachedGoal;
  delay(1000);

  memcpy(outgoingMessage.startingMac, NodeHandler::C6M1ADDR, 6);
  memcpy(outgoingMessage.goalMac, NodeHandler::TINYPICOADDR, 6);

}

void loop() {
  // put your main code here, to run repeatedly:
  
}