#pragma once
#include <stdint.h>
#include <time.h>

void display_init();
void display_flip(); // toggle 0° / 180° rotation
void draw_frame(const struct tm &t, bool ble_connected, bool ble_synced, const char *msg,
                uint32_t msg_age_ms, uint32_t now_ms);
