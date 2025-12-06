#include <Arduino.h>
#include "NodeHandler.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1351.h>
#include <SPI.h>

#define MOSI_PIN 23 //DIN-BLUE
#define SCLK_PIN 22 //CLK-YELLOW
#define CS_PIN   21 //CS-ORANGE
#define DC_PIN   20 //DC-GREEN
#define RST_PIN  19 //RST-WHITE

#define	BLACK           0x0000
#define	BLUE            0x001F
#define	RED             0xF800
#define	GREEN           0x07E0
#define CYAN            0x07FF
#define MAGENTA         0xF81F
#define YELLOW          0xFFE0  
#define WHITE           0xFFFF

Adafruit_SSD1351 display = Adafruit_SSD1351(128, 128, CS_PIN, DC_PIN, MOSI_PIN, SCLK_PIN, RST_PIN);

NodeHandler::message_t outgoingMessage{};

void printActualText(const char *text) {
    if (!text) {
        Serial.println("(null)");
        return;
    }

    Serial.print("actualText: \"");
    Serial.print(text);
    Serial.println("\"");

    display.fillRect(0,0,128,128, BLACK);
    display.setTextColor(WHITE);
    display.setTextSize(2);
    display.setCursor(0,0);
    display.println(text);
    display.setTextWrap(true);
}

bool ledState = false;
void onPacketReachedGoal(const uint8_t *fromMac, const uint8_t *data, int len) {
  printActualText(((const NodeHandler::message_t*)data)->payload);
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(12, OUTPUT);
  digitalWrite(12, LOW);
  delay(1000);

  display.begin();
  display.fillRect(0,0,128,128, BLACK);

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
  NodeHandler::instance().tick();
}