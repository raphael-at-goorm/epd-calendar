#include "google_auth.h"
#include "secrets.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>

static const char *NVS_NS       = "gcal";
static const char *NVS_KEY_RT   = "refresh_token";

static String s_refreshToken;
static String s_accessToken;
static unsigned long s_tokenExpireMs = 0;   // millis() when token expires

// ── NVS helpers ───────────────────────────────────────────────────────────────

static bool nvs_saveRefreshToken(const String &token) {
    Preferences prefs;
    prefs.begin(NVS_NS, false);
    bool ok = prefs.putString(NVS_KEY_RT, token);
    prefs.end();
    return ok;
}

static String nvs_loadRefreshToken() {
    Preferences prefs;
    prefs.begin(NVS_NS, true);
    String t = prefs.getString(NVS_KEY_RT, "");
    prefs.end();
    return t;
}

// ── HTTPS POST helper ─────────────────────────────────────────────────────────

static String httpsPost(const char *host, const char *path, const String &body,
                        const char *contentType = "application/x-www-form-urlencoded") {
    WiFiClientSecure client;
    client.setInsecure();   // personal desk device; CA pinning is a future improvement

    HTTPClient http;
    String url = String("https://") + host + path;
    http.begin(client, url);
    http.addHeader("Content-Type", contentType);

    int code = http.POST(body);
    if (code <= 0) { http.end(); return ""; }
    String resp = http.getString();
    http.end();
    return resp;
}

// ── Token refresh ─────────────────────────────────────────────────────────────

static bool refreshAccessToken() {
    if (s_refreshToken.isEmpty()) return false;

    String body = String("grant_type=refresh_token")
                + "&client_id=" + GOOGLE_CLIENT_ID
                + "&client_secret=" + GOOGLE_CLIENT_SECRET
                + "&refresh_token=" + s_refreshToken;

    String resp = httpsPost("oauth2.googleapis.com", "/token", body);
    if (resp.isEmpty()) return false;

    JsonDocument doc;
    if (deserializeJson(doc, resp) != DeserializationError::Ok) return false;

    String at = doc["access_token"] | "";
    if (at.isEmpty()) {
        Serial.printf("[auth] refresh failed: %s\n", (doc["error"] | "?"));
        return false;
    }

    s_accessToken    = at;
    int expiresIn    = doc["expires_in"] | 3600;
    s_tokenExpireMs  = millis() + (unsigned long)(expiresIn - 60) * 1000UL;
    return true;
}

// ── Public API ────────────────────────────────────────────────────────────────

bool gauth_init() {
    s_refreshToken = nvs_loadRefreshToken();
    return !s_refreshToken.isEmpty();
}

bool gauth_hasToken() {
    return !s_refreshToken.isEmpty();
}

String gauth_getAccessToken() {
    if (s_accessToken.isEmpty() || millis() >= s_tokenExpireMs) {
        if (!refreshAccessToken()) return "";
    }
    return s_accessToken;
}

bool gauth_requestDeviceCode(DeviceCodeInfo &out) {
    String body = String("client_id=") + GOOGLE_CLIENT_ID
                + "&scope=https://www.googleapis.com/auth/calendar.readonly";

    String resp = httpsPost("oauth2.googleapis.com", "/device/code", body);
    if (resp.isEmpty()) return false;

    JsonDocument doc;
    if (deserializeJson(doc, resp) != DeserializationError::Ok) return false;

    String dc = doc["device_code"] | "";
    if (dc.isEmpty()) return false;

    out.deviceCode      = dc;
    out.userCode        = doc["user_code"].as<String>();
    out.verificationUrl = doc["verification_url"].as<String>();
    out.expiresIn       = doc["expires_in"] | 1800;
    out.interval        = doc["interval"] | 5;
    return true;
}

bool gauth_pollDeviceAuth(const DeviceCodeInfo &info) {
    String body = String("grant_type=urn:ietf:params:oauth:grant-type:device_code")
                + "&client_id=" + GOOGLE_CLIENT_ID
                + "&client_secret=" + GOOGLE_CLIENT_SECRET
                + "&device_code=" + info.deviceCode;

    String resp = httpsPost("oauth2.googleapis.com", "/token", body);
    if (resp.isEmpty()) return false;

    Serial.printf("[auth] poll resp: %s\n", resp.c_str());

    JsonDocument doc;
    if (deserializeJson(doc, resp) != DeserializationError::Ok) return false;

    String rt = doc["refresh_token"] | "";
    if (rt.isEmpty()) return false;  // still pending or error

    s_refreshToken   = rt;
    s_accessToken    = doc["access_token"] | "";
    int expiresIn    = doc["expires_in"] | 3600;
    s_tokenExpireMs  = millis() + (unsigned long)(expiresIn - 60) * 1000UL;

    bool saved = nvs_saveRefreshToken(s_refreshToken);
    Serial.printf("[auth] token saved=%d  rt_len=%d\n", saved, s_refreshToken.length());
    return true;
}
