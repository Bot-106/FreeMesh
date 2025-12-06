#include "EspNowManager.h"

#ifndef ESP_NOW_MAX_DATA_LEN
#define ESP_NOW_MAX_DATA_LEN 250
#endif

EspNowManager* EspNowManager::_self = nullptr;

EspNowManager::EspNowManager()
    : _userRecvCb(nullptr),
      _userSendCb(nullptr),
      _ready(false)
{
}

EspNowManager& EspNowManager::instance() {
    static EspNowManager mgr;
    return mgr;
}

bool EspNowManager::begin() {
    if (_ready) {
        return true;
    }

    _self = this;
    WiFi.mode(WIFI_STA);

    if (esp_now_init() != ESP_OK) {
        Serial.println(F("[EspNowManager] Error initializing ESP-NOW"));
        _ready = false;
        return false;
    }

    esp_now_register_send_cb(onDataSentInternal);
    // NOTE: this now matches esp_now_recv_cb_t
    esp_now_register_recv_cb(onDataRecvInternal);

    _ready = true;
    Serial.println(F("[EspNowManager] ESP-NOW initialized"));
    return true;
}

bool EspNowManager::addPeer(const uint8_t *peer_addr,
                            uint8_t channel,
                            bool encrypt,
                            const uint8_t *lmk,
                            size_t lmk_len)
{
    if (!_ready || peer_addr == nullptr) {
        Serial.println(F("[EspNowManager] addPeer: Not ready or invalid MAC"));
        return false;
    }

    if (esp_now_is_peer_exist(peer_addr)) {
        return true;
    }

    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(peerInfo));

    memcpy(peerInfo.peer_addr, peer_addr, 6);
    peerInfo.channel = channel;
    peerInfo.encrypt = encrypt;

#if defined(WIFI_IF_STA)
    peerInfo.ifidx = WIFI_IF_STA;
#endif

#if defined(ESP_NOW_KEY_LEN)
    if (encrypt && lmk != nullptr && lmk_len == ESP_NOW_KEY_LEN) {
        memcpy(peerInfo.lmk, lmk, ESP_NOW_KEY_LEN);
    }
#else
    (void)lmk;
    (void)lmk_len;
#endif

    esp_err_t status = esp_now_add_peer(&peerInfo);
    if (status != ESP_OK) {
        Serial.print(F("[EspNowManager] Failed to add peer, error: "));
        Serial.println(status);
        return false;
    }

    Serial.print(F("[EspNowManager] Peer added: "));
    printMac(peer_addr);
    Serial.println();
    return true;
}

bool EspNowManager::removePeer(const uint8_t *peer_addr) {
    if (!_ready || peer_addr == nullptr) {
        return false;
    }

    if (!esp_now_is_peer_exist(peer_addr)) {
        return true;
    }

    esp_err_t status = esp_now_del_peer(peer_addr);
    if (status != ESP_OK) {
        Serial.print(F("[EspNowManager] Failed to remove peer, error: "));
        Serial.println(status);
        return false;
    }

    Serial.print(F("[EspNowManager] Peer removed: "));
    printMac(peer_addr);
    Serial.println();
    return true;
}

bool EspNowManager::hasPeer(const uint8_t *peer_addr) const {
    if (!_ready || peer_addr == nullptr) {
        return false;
    }
    return esp_now_is_peer_exist(peer_addr);
}

bool EspNowManager::send(const uint8_t *peer_addr,
                         const uint8_t *data,
                         size_t len)
{
    if (!_ready) {
        Serial.println(F("[EspNowManager] send: ESP-NOW not ready"));
        return false;
    }

    if (peer_addr == nullptr || data == nullptr || len == 0) {
        Serial.println(F("[EspNowManager] send: invalid arguments"));
        return false;
    }

    if (len > ESP_NOW_MAX_DATA_LEN) {
        Serial.print(F("[EspNowManager] send: payload too large (max "));
        Serial.print(ESP_NOW_MAX_DATA_LEN);
        Serial.println(F(" bytes)"));
        return false;
    }

    if (!esp_now_is_peer_exist(peer_addr)) {
        Serial.println(F("[EspNowManager] send: peer not registered"));
        return false;
    }

    esp_err_t result = esp_now_send(peer_addr, data, len);
    if (result != ESP_OK) {
        Serial.print(F("[EspNowManager] esp_now_send error: "));
        Serial.println(result);
        return false;
    }

    return true;
}

void EspNowManager::setReceiveCallback(ReceiveCallback cb) {
    _userRecvCb = cb;
}

void EspNowManager::setSendCallback(SendStatusCallback cb) {
    _userSendCb = cb;
}

void EspNowManager::onDataSentInternal(const uint8_t *mac_addr,
                                       esp_now_send_status_t status)
{
    if (_self && _self->_userSendCb) {
        _self->_userSendCb(mac_addr, status);
    } else {
        Serial.print(F("[EspNowManager] Last Packet Send Status: "));
        Serial.println(status == ESP_NOW_SEND_SUCCESS ?
                       F("Delivery Success") : F("Delivery Fail"));
    }
}

// FIXED: matches esp_now_recv_cb_t
void EspNowManager::onDataRecvInternal(const esp_now_recv_info_t *info,
                                       const uint8_t *incomingData,
                                       int len)
{
    // Extract source MAC from info for the user callback
    const uint8_t *mac = (info != nullptr) ? info->src_addr : nullptr;

    if (_self && _self->_userRecvCb) {
        _self->_userRecvCb(mac, incomingData, len);
    } else {
        Serial.print(F("[EspNowManager] Bytes received: "));
        Serial.println(len);
        Serial.print(F("From: "));
        printMac(mac);
        Serial.println();
    }
}

void EspNowManager::printMac(const uint8_t *mac, Stream &out) {
    if (!mac) return;
    for (int i = 0; i < 6; ++i) {
        if (mac[i] < 0x10) out.print('0');
        out.print(mac[i], HEX);
        if (i < 5) out.print(':');
    }
}
