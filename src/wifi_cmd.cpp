#include "wifi_cmd.h"
#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <time.h>

static WebServer s_server(80);

static bool s_connected = false;
static bool s_synced = false;
static bool s_new_msg = false;
static bool s_flip = false;
static char s_msg[256] = "";

// ─────────────────────────────────────────────────────────────────────────────
// Command handler — POST /cmd
// ─────────────────────────────────────────────────────────────────────────────
static void handle_cmd() {
  if (!s_server.hasArg("plain")) {
    s_server.send(400, "text/plain", "No body");
    return;
  }
  String v = s_server.arg("plain");
  if (v.startsWith("TIME:")) {
    long long epoch = atoll(v.c_str() + 5);
    if (epoch > 1000000000LL) {
      struct timeval tv = {(time_t)epoch, 0};
      settimeofday(&tv, nullptr);
      s_synced = true;
      Serial.printf("[WiFi] TIME set: %lld\n", epoch);
    }
  } else if (v == "FLIP") {
    s_flip = true;
    Serial.println("[WiFi] FLIP");
  } else {
    const char *text = v.c_str();
    if (v.startsWith("MSG:"))
      text += 4;
    strncpy(s_msg, text, sizeof(s_msg) - 1);
    s_msg[sizeof(s_msg) - 1] = '\0';
    s_new_msg = true;
    Serial.printf("[WiFi] MSG: %s\n", s_msg);
  }
  s_server.send(200, "text/plain", "OK");
}

void wifi_init() {
  WiFiManager wm;
  wm.setConnectTimeout(20);       // 20s to connect before opening AP portal
  wm.setConfigPortalTimeout(180); // 3 min portal, then reboot if no config

  if (!wm.autoConnect("HoloDisplay-Setup", "12345678")) {
    Serial.println("[WiFi] Portal timed out, rebooting");
    ESP.restart();
  }

  s_connected = true;
  Serial.printf("[WiFi] Connected — IP: %s\n", WiFi.localIP().toString().c_str());

  // NTP sync
  configTzTime("BRT3", "pool.ntp.org", "time.nist.gov");
  Serial.print("[WiFi] NTP syncing");
  uint32_t ntp_t = millis();
  while (time(nullptr) < 1000000000UL && millis() - ntp_t < 5000) {
    delay(200);
    Serial.print(".");
  }
  s_synced = time(nullptr) > 1000000000UL;
  Serial.println(s_synced ? "\n[WiFi] NTP synced" : "\n[WiFi] NTP timeout");

  s_server.on("/cmd", HTTP_POST, handle_cmd);
  s_server.on("/status", HTTP_GET, []() {
  char buf[128];
  snprintf(buf, sizeof(buf), "{\"ip\":\"%s\",\"synced\":%s,\"uptime\":%lu}",
           WiFi.localIP().toString().c_str(), s_synced ? "true" : "false", millis() / 1000);
  s_server.send(200, "application/json", buf);
});

  s_server.begin();
  Serial.println("[WiFi] HTTP server on port 80");
}

void wifi_handle() { s_server.handleClient(); }

bool wifi_is_connected() { return s_connected; }
bool wifi_is_synced() { return s_synced; }

bool wifi_take_flip() {
  if (!s_flip)
    return false;
  s_flip = false;
  return true;
}

bool wifi_take_message(char *out, int len) {
  if (!s_new_msg)
    return false;
  strncpy(out, s_msg, len - 1);
  out[len - 1] = '\0';
  s_new_msg = false;
  return true;
}