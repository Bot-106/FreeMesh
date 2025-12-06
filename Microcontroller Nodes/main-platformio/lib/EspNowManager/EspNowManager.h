#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>

// Simple wrapper class for ESP-NOW peer-to-peer two-way communication.

class EspNowManager {
public:
    // User callback types
    // Still expose MAC + data to user code (like before)
    using ReceiveCallback    = void (*)(const uint8_t *mac, const uint8_t *data, int len);
    using SendStatusCallback = void (*)(const uint8_t *mac_addr, esp_now_send_status_t status);

    // Get singleton instance
    static EspNowManager& instance();

    // Initialize Wi-Fi (STA) + ESP-NOW and register internal callbacks.
    bool begin();

    bool addPeer(const uint8_t *peer_addr,
                 uint8_t channel = 0,
                 bool encrypt = false,
                 const uint8_t *lmk = nullptr,
                 size_t lmk_len = 0);

    bool removePeer(const uint8_t *peer_addr);
    bool hasPeer(const uint8_t *peer_addr) const;

    bool send(const uint8_t *peer_addr, const uint8_t *data, size_t len);

    void setReceiveCallback(ReceiveCallback cb);
    void setSendCallback(SendStatusCallback cb);

    bool isReady() const { return _ready; }

    static void printMac(const uint8_t *mac, Stream &out = Serial);

private:
    EspNowManager();
    EspNowManager(const EspNowManager&) = delete;
    EspNowManager& operator=(const EspNowManager&) = delete;

    // NEW: internal recv callback must use esp_now_recv_info_t
    static void onDataRecvInternal(const esp_now_recv_info_t *info,
                                   const uint8_t *incomingData,
                                   int len);

    static void onDataSentInternal(const uint8_t *mac_addr,
                                   esp_now_send_status_t status);

    ReceiveCallback    _userRecvCb;
    SendStatusCallback _userSendCb;
    bool               _ready;

    static EspNowManager* _self;
};
