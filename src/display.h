#pragma once
#include <stdint.h>
#include <time.h>

void display_init();
void display_flip();              // toggle 0° / 180° rotation
void display_set_rotation(int r); // 0–7 (LovyanGFX values)
void draw_frame(const struct tm &t, bool wifi_connected, bool wifi_synced, const char *msg,
                uint32_t msg_age_ms, uint32_t now_ms);
