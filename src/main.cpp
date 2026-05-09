/*
  HoloDisplay -- ESP32-C3 round clock with BLE NUS
  Board : ESP32-2424S012N  (GC9A01 240x240, no touch)

  Tasker / nRF Connect -> write to NUS RX characteristic:
    TIME:<unix_epoch>   -- sync clock   e.g. TIME:1746835200
    MSG:<text>          -- show message e.g. MSG:Hello!
    (bare text)         -- treated as MSG
*/

#include <Arduino.h>
#include <time.h>
#include "display.h"
#include "ble_nus.h"

void setup() {
    Serial.begin(115200);

    // Epoch 0 until BLE TIME: arrives
    struct timeval tv = {0, 0};
    settimeofday(&tv, nullptr);

    display_init();
    ble_init();
}

void loop() {
    static uint32_t last_draw   = 0;
    static uint32_t msg_recv_ms = 0;
    static char     cur_msg[256] = "";

    uint32_t now = millis();

    if (ble_take_message(cur_msg, sizeof(cur_msg)))
        msg_recv_ms = now;

    // Redraw at 200 ms -- smooth for animations, easy on the CPU
    if (now - last_draw >= 200) {
        last_draw = now;

        time_t epoch; time(&epoch);
        struct tm t;  localtime_r(&epoch, &t);

        draw_frame(t, ble_is_connected(), ble_is_synced(),
                   cur_msg, now - msg_recv_ms, now);
    }

    delay(20);
}
