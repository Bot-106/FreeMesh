#include <Arduino.h>
#include "NodeHandler.h"

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
  uint8_t selfMac[6];
  NodeHandler::instance().getSelfMac(selfMac);
  EspNowManager::instance().printMac(selfMac, Serial);
  delay(1000);
}

void loop() {
  // put your main code here, to run repeatedly:
  NodeHandler::instance().tick();
}