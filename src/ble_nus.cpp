#include "ble_nus.h"
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <time.h>

// ─────────────────────────────────────────────────────────────────────────────
// NUS UUIDs  (Nordic UART Service — recognised by Tasker's BLE plugin)
// ─────────────────────────────────────────────────────────────────────────────
#define NUS_SVC_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_RX_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E" // phone → ESP32
#define NUS_TX_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9E" // ESP32 → phone

// ─────────────────────────────────────────────────────────────────────────────
// Internal state (file-scope only)
// ─────────────────────────────────────────────────────────────────────────────
static volatile bool s_connected = false;
static volatile bool s_synced    = false;
static volatile bool s_new_msg   = false;
static volatile bool s_flip      = false;
static portMUX_TYPE  s_mux       = portMUX_INITIALIZER_UNLOCKED;
static char          s_msg[256]  = "";

// ─────────────────────────────────────────────────────────────────────────────
// BLE callbacks
// ─────────────────────────────────────────────────────────────────────────────
class ServerCB : public BLEServerCallbacks
{
    void onConnect(BLEServer *) override { s_connected = true; }
    void onDisconnect(BLEServer *s) override
    {
        s_connected = false;
        s->startAdvertising(); // auto-restart so Tasker can reconnect
    }
};

class RxCB : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *c) override
    {
        std::string v = c->getValue();
        if (v.empty())
            return;

        if (v.rfind("TIME:", 0) == 0)
        {
            // TIME:<unix_epoch_seconds>
            long long epoch = atoll(v.c_str() + 5);
            if (epoch > 1000000000LL)
            {
                struct timeval tv = {(time_t)epoch, 0};
                settimeofday(&tv, nullptr);
                s_synced = true;
                Serial.printf("[BLE] TIME set: %lld\n", epoch);
            }
        }
        else if (v == "FLIP")
        {
            s_flip = true;
            Serial.println("[BLE] FLIP");
        }
        else
        {
            // MSG:<text>  or bare text
            const char *text = v.c_str();
            if (v.rfind("MSG:", 0) == 0)
                text += 4;
            taskENTER_CRITICAL(&s_mux);
            strncpy(s_msg, text, sizeof(s_msg) - 1);
            s_msg[sizeof(s_msg) - 1] = '\0';
            s_new_msg = true;
            taskEXIT_CRITICAL(&s_mux);
            Serial.printf("[BLE] MSG: %s\n", s_msg);
        }
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────
void ble_init()
{
    BLEDevice::init("HoloDisplay");
    BLEServer *srv = BLEDevice::createServer();
    srv->setCallbacks(new ServerCB());

    BLEService *svc = srv->createService(NUS_SVC_UUID);

    BLECharacteristic *rx = svc->createCharacteristic(
        NUS_RX_UUID,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
    rx->setCallbacks(new RxCB());

    BLECharacteristic *tx = svc->createCharacteristic(
        NUS_TX_UUID, BLECharacteristic::PROPERTY_NOTIFY);
    tx->addDescriptor(new BLE2902());

    svc->start();
    BLEDevice::getAdvertising()->addServiceUUID(NUS_SVC_UUID);
    BLEDevice::getAdvertising()->setScanResponse(true);
    BLEDevice::startAdvertising();
    Serial.println("[BLE] advertising as 'HoloDisplay'");
}

bool ble_is_connected() { return s_connected; }
bool ble_is_synced() { return s_synced; }
bool ble_take_flip() {
    if (!s_flip) return false;
    s_flip = false;
    return true;
}
bool ble_take_message(char *out, int len)
{
    if (!s_new_msg)
        return false;
    taskENTER_CRITICAL(&s_mux);
    strncpy(out, s_msg, len - 1);
    out[len - 1] = '\0';
    s_new_msg = false;
    taskEXIT_CRITICAL(&s_mux);
    return true;
}
