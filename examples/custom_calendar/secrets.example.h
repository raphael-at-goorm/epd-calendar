#pragma once

// Rename this file to secrets.h and fill in your credentials.
// secrets.h is listed in .gitignore and will not be committed.

// ── Wi-Fi ─────────────────────────────────────────────────────────────────────
#define WIFI_SSID       "YOUR_WIFI_SSID"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"

// ── Google OAuth2 ─────────────────────────────────────────────────────────────
// 1. Go to https://console.cloud.google.com
// 2. Create a project, enable "Google Calendar API"
// 3. Create OAuth2 credentials → type: "TVs and Limited Input devices"
// 4. Copy the Client ID and Client Secret below
#define GOOGLE_CLIENT_ID        "YOUR_CLIENT_ID.apps.googleusercontent.com"
#define GOOGLE_CLIENT_SECRET    "YOUR_CLIENT_SECRET"

// ── Google Calendar ID ────────────────────────────────────────────────────────
// Usually your Gmail address, e.g. "yourname@gmail.com"
// For a shared/team calendar use the calendar ID from Google Calendar settings.
#define GOOGLE_CALENDAR_ID      "your_email@gmail.com"

// ── HTTP pull OTA (optional, cross-network) ───────────────────────────────────
// Uncomment and fill in to enable pull-based firmware updates from any network.
// Upload version.txt + firmware.bin to a public URL (e.g. GitHub Releases).
// version.txt must contain exactly the FIRMWARE_VERSION string.
// #define OTA_VERSION_URL  "https://github.com/YOUR_GITHUB_USER/epd-calendar/releases/latest/download/version.txt"
// #define OTA_FIRMWARE_URL "https://github.com/YOUR_GITHUB_USER/epd-calendar/releases/latest/download/firmware.bin"
