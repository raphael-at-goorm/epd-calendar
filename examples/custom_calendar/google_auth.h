#pragma once
#include <Arduino.h>

// Call once on boot. Returns true if a stored refresh token was found.
bool gauth_init();

// True if we have a valid (or refreshable) session.
bool gauth_hasToken();

// Returns a valid Bearer access token (refreshes automatically if expired).
// Returns empty string on failure.
String gauth_getAccessToken();

// ── Device Authorization Flow (first-time setup) ─────────────────────────────
struct DeviceCodeInfo {
    String deviceCode;
    String userCode;
    String verificationUrl;
    int    expiresIn;
    int    interval;
};

// Step 1: request a device + user code pair from Google.
bool gauth_requestDeviceCode(DeviceCodeInfo &out);

// Step 2: poll until the user completes authorization.
// Returns true and saves the refresh token on success.
// Call repeatedly with the interval from DeviceCodeInfo.
bool gauth_pollDeviceAuth(const DeviceCodeInfo &info);
