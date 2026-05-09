#pragma once
#include <stdbool.h>

void  ble_init();
bool  ble_is_connected();
bool  ble_is_synced();

// Returns true and copies the pending message into `out` (max `len` bytes),
// then clears the pending flag.  Safe to call from loop().
bool  ble_take_message(char *out, int len);
