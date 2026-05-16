#define LGFX_USE_V1
#include "display.h"
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <Preferences.h>
#include <math.h>

// ─────────────────────────────────────────────────────────────────────────────
// LGFX — GC9A01 · SPI2 · 240×240  (ESP32-2424S012N pin-out)
// ─────────────────────────────────────────────────────────────────────────────
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_GC9A01 _panel;
  lgfx::Bus_SPI _bus;
  lgfx::Light_PWM _bl;

public:
  LGFX() {
    {
      auto cfg = _bus.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 80000000;
      cfg.freq_read = 20000000;
      cfg.pin_sclk = 6;
      cfg.pin_mosi = 7;
      cfg.pin_miso = -1;
      cfg.pin_dc = 2;
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }

    {
      auto cfg = _panel.config();
      cfg.pin_cs = 10;
      cfg.pin_rst = -1;
      cfg.pin_busy = -1;
      cfg.memory_width = 240;
      cfg.memory_height = 240;
      cfg.panel_width = 240;
      cfg.panel_height = 240;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.invert = true;
      cfg.rgb_order = false;
      cfg.bus_shared = false;
      _panel.config(cfg);
    }

    {
      auto cfg = _bl.config();
      cfg.pin_bl = 3;
      cfg.invert = false;
      cfg.freq = 44100;
      cfg.pwm_channel = 1;
      _bl.config(cfg);
      _panel.setLight(&_bl);
    }

    setPanel(&_panel);
  }
};

static LGFX tft;
static lgfx::LGFX_Sprite canvas(&tft); // off-screen buffer → no flicker
static int s_rotation = 0;             // 0 = normal, 2 = 180°
static Preferences s_prefs;

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────
void display_init() {
  s_prefs.begin("display", false);
  s_rotation = s_prefs.getInt("rotation", 4); // default: mirrored for beam splitter

  tft.init();
  tft.setRotation(s_rotation);
  tft.setBrightness(0);      // backlight off while we flush
  tft.fillScreen(TFT_BLACK); // wipe full 240×240 panel RAM
  tft.fillScreen(TFT_BLACK); // second pass — ensures no stale pixels
  tft.setBrightness(200);    // backlight on with a clean frame
  canvas.createSprite(200, 200);
}

// ─────────────────────────────────────────────────────────────────────────────
// draw_frame — renders a full clock face into the sprite, then pushes once
// ─────────────────────────────────────────────────────────────────────────────
static constexpr int CX = 100, CY = 100, CR = 95;

static const char *DAYS[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static const char *MONS[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                             "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

// Strip UTF-8 diacritics → ASCII so FreeSans fonts can render them
static void utf8_to_ascii(const char *src, char *dst, int dst_len) {
  // Table of common Latin-1 supplement (U+00C0–U+00FF) → ASCII
  static const char latin1_map[64] = {
      'A', 'A', 'A', 'A', 'A', 'A', 'A', 'C', 'E', 'E', 'E', 'E', 'I', 'I', 'I', 'I',
      'D', 'N', 'O', 'O', 'O', 'O', 'O', 0,   'O', 'U', 'U', 'U', 'U', 'Y', 'T', 's',
      'a', 'a', 'a', 'a', 'a', 'a', 'a', 'c', 'e', 'e', 'e', 'e', 'i', 'i', 'i', 'i',
      'd', 'n', 'o', 'o', 'o', 'o', 'o', 0,   'o', 'u', 'u', 'u', 'u', 'y', 't', 'y'};
  int j = 0;
  const uint8_t *s = (const uint8_t *)src;
  while (*s && j < dst_len - 1) {
    if (*s < 0x80) {
      dst[j++] = (char)*s++;
    } else if ((*s & 0xE0) == 0xC0) {
      // 2-byte sequence: U+0080–U+07FF
      uint32_t cp = (*s & 0x1F) << 6;
      s++;
      if (*s)
        cp |= (*s++ & 0x3F);
      if (cp >= 0xC0 && cp <= 0xFF) {
        char c = latin1_map[cp - 0xC0];
        if (c)
          dst[j++] = c;
      } else {
        dst[j++] = '?';
      }
    } else if ((*s & 0xF0) == 0xE0) {
      // 3-byte sequence — skip (CJK, emoji, etc.)
      s++;
      if (*s)
        s++;
      if (*s)
        s++;
      dst[j++] = '?';
    } else if ((*s & 0xF8) == 0xF0) {
      // 4-byte sequence — skip
      s++;
      if (*s)
        s++;
      if (*s)
        s++;
      if (*s)
        s++;
      dst[j++] = '?';
    } else {
      s++; // invalid byte, skip
    }
  }
  dst[j] = '\0';
}

void draw_frame(const struct tm &t, bool wifi_connected, bool wifi_synced, const char *msg,
                uint32_t msg_age_ms, uint32_t now_ms) {
  canvas.fillScreen(TFT_BLACK);

  // ── Second sweep arc (cyan 0–29 s, orange 30–59 s) ───────────────────────
  float sweep = t.tm_sec / 60.0f * 360.0f;
  if (sweep > 0.5f) {
    uint16_t col = (t.tm_sec < 30) ? 0x07FFu : 0xF81Fu;
    canvas.drawArc(CX, CY, CR, CR - 4, 270.0f, 270.0f + sweep, col);
  }

  // ── Bezel + hour ticks ───────────────────────────────────────────────────
  canvas.drawCircle(CX, CY, CR + 2, 0x2104u);
  for (int i = 0; i < 12; i++) {
    float a = ((i * 30.0f) - 90.0f) * DEG_TO_RAD;
    canvas.drawLine(CX + (int)((CR - 5) * cosf(a)), CY + (int)((CR - 5) * sinf(a)),
                    CX + (int)((CR - 16) * cosf(a)), CY + (int)((CR - 16) * sinf(a)),
                    i == 0 ? TFT_WHITE : 0x8410u);
  }

  // ── WiFi indicator (800 ms heartbeat pulse) ────────────────────────────────
  bool pulse = wifi_connected && ((now_ms / 800) & 1) == 0;
  uint16_t wifi_col = wifi_connected ? (pulse ? TFT_GREEN : 0x03E0u) : 0x2104u;
  canvas.fillCircle(CX - 22, 26, 5, wifi_col);
  canvas.setFont(&lgfx::fonts::Font2);
  canvas.setTextDatum(lgfx::ML_DATUM);
  canvas.setTextColor(wifi_connected ? TFT_GREEN : 0x4208u, TFT_BLACK);
  canvas.drawString("NET", CX - 14, 26);

  // ── "no sync" badge ───────────────────────────────────────────────────────
  if (!wifi_synced) {
    canvas.setTextDatum(lgfx::MR_DATUM);
    canvas.setTextColor(0x8410u, TFT_BLACK);
    canvas.drawString("no sync", 196, 26); // right-align within 200px wide canvas
  }

  // ── HH:MM ─────────────────────────────────────────────────────────────────
  char hhmm[6];
  snprintf(hhmm, sizeof(hhmm), "%02d:%02d", t.tm_hour, t.tm_min);
  canvas.setFont(&lgfx::fonts::FreeSansBold24pt7b);
  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas.setTextDatum(lgfx::MC_DATUM);
  canvas.drawString(hhmm, CX - 14, CY - 8);

  // ── Seconds (small, right of HH:MM) ──────────────────────────────────────
  char ss[4];
  snprintf(ss, sizeof(ss), "%02d", t.tm_sec);
  canvas.setFont(&lgfx::fonts::FreeSans12pt7b);
  canvas.setTextColor(0x8410u, TFT_BLACK);
  canvas.setTextDatum(lgfx::TL_DATUM);
  canvas.drawString(ss, CX + 58, CY + 6);

  // ── Date ─────────────────────────────────────────────────────────────────
  char date[32];
  snprintf(date, sizeof(date), "%s %d %s %04d", DAYS[t.tm_wday], t.tm_mday, MONS[t.tm_mon],
           1900 + t.tm_year);
  canvas.setFont(&lgfx::fonts::FreeSans9pt7b);
  canvas.setTextColor(0xC618u, TFT_BLACK);
  canvas.setTextDatum(lgfx::TC_DATUM);
  canvas.drawString(date, CX, CY + 36);

  // ── Message (slides up from below over 500 ms) ────────────────────────────
  if (msg && msg[0]) {
    char ascii_msg[256];
    utf8_to_ascii(msg, ascii_msg, sizeof(ascii_msg));
    int y = (msg_age_ms < 500) ? 168 + (int)((1.0f - msg_age_ms / 500.0f) * 24.0f) : 168;
    canvas.setFont(&lgfx::fonts::FreeSans9pt7b);
    canvas.setTextColor(TFT_YELLOW, TFT_BLACK);
    canvas.setTextDatum(lgfx::TC_DATUM);
    canvas.drawString(ascii_msg, CX, y);
  }

  // Clear the 20 px border around the sprite every frame so no ghost pixels
  // accumulate from arc/circle drawing that clips slightly outside the canvas.
  tft.fillRect(0, 0, 240, 20, TFT_BLACK);    // top
  tft.fillRect(0, 220, 240, 20, TFT_BLACK);  // bottom
  tft.fillRect(0, 20, 20, 200, TFT_BLACK);   // left
  tft.fillRect(220, 20, 20, 200, TFT_BLACK); // right

  canvas.pushSprite(20, 20); // centre 200×200 sprite on 240×240 screen
}

void display_flip() {
  // Toggle between normal (0) and horizontally mirrored (4)
  s_rotation = (s_rotation == 0) ? 4 : 0;
  tft.setRotation(s_rotation);
  s_prefs.putInt("rotation", s_rotation); // persist across reboots
  Serial.printf("[DISPLAY] rotation -> %d (%s)\n", s_rotation,
                s_rotation == 4 ? "mirrored" : "normal");
}
