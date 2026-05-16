#pragma once
#include <stdbool.h>

void wifi_init();
void wifi_handle(); // call every loop()

bool wifi_is_connected();
bool wifi_is_synced();

bool wifi_take_message(char *out, int len);
bool wifi_take_flip();