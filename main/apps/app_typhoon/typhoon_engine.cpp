/**
 * Typhoon Compass engine — ported from typhoon-compass-fw (StickS3 Batch 8)
 */
#include "typhoon_engine.h"
#include "typhoon_data.h"
#include "typhoon_port.h"
#include "reference/tc_colors.h"
#include "reference/earth_data.h"
#include "reference/china_provinces.h"

#include <hal/hal.h>
#include <mooncake_log.h>
#include <esp_log.h>
#include <wifi_manager.h>
#include <ssid_manager.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <cstring>
#include <strings.h>
#include <cmath>
#include <cstdio>
#include <string>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace typhoon {

static LovyanGFX* disp_ = nullptr;

/*
 * Typhoon Compass — StickS3
 * Earth rendering: orthographic projection + coastline data
 *   from SkyCompass_Satellite (nongxl/SkyCompass_Satellite)
 * Wind rings / tracks: geographic km → pixels (EARTH_R / 6371)
 */

// Enums MUST appear before any function (Arduino auto-prototypes sit after includes)
enum Page : uint8_t {
  PAGE_BOOT = 0,
  PAGE_MAIN,
  PAGE_DETAIL,
  PAGE_TRACK,
  PAGE_MENU,
  PAGE_MULTI,
  PAGE_LOCATION,
  PAGE_WIFI,
  PAGE_ABOUT
};
enum BtnEvt : uint8_t { EVT_NONE=0, EVT_SHORT, EVT_LONG, EVT_DBL, EVT_COMBO };
enum WifiPhase : uint8_t {
  WIFI_STATUS = 0,
  WIFI_SCAN,
  WIFI_PIN,
  WIFI_CONNECTING
};
enum StormCat : uint8_t { CAT_TD=0, CAT_TS, CAT_STS, CAT_TY, CAT_STY, CAT_SSTY };

static void showEvent(const char* text, uint16_t color);
static void gotoPage(Page p);

#define VIEW_W     TC_SCREEN_W   // native StopWatch canvas (was StickS3 240)
#define VIEW_H     TC_SCREEN_H
// Oversized map so ±15° tilt still shows coastlines in newly exposed edges.
#define FRAME_MARGIN tcU(32)
#define FRAME_W    (VIEW_W + 2 * FRAME_MARGIN)
#define FRAME_H    (VIEW_H + 2 * FRAME_MARGIN)
#define FRAME_OX   FRAME_MARGIN
#define FRAME_OY   FRAME_MARGIN
#define SCR_X(x)   ((x) - FRAME_OX)
#define SCR_Y(y)   ((y) - FRAME_OY)
static int earth_cx = FRAME_W / 2;
static int earth_cy = FRAME_H / 2;
// Geographic FOV preserved vs StickS3 by scaling EARTH_R with screen DPI.
#define EARTH_R_MAIN   tcU(220)
#define EARTH_R_DETAIL tcU(1450)  // ≈4 provinces
#define EARTH_R_TRACK  tcU(860)   // Time Machine: a bit wider again
#define EARTH_R_MULTI  tcU(95)
static int earth_r = EARTH_R_MAIN;
#define EARTH_R      earth_r
#define R_EARTH_KM 6371.0f
#define DEG2RAD    0.0174532925f

// Map projection center (MAIN = observer; DETAIL/TRACK = typhoon follow)
static float ctr_lat = 31.2f;
static float ctr_lon = 121.5f;
// Observer location (DISTANCE / user marker)
static float user_lat = 31.2f;
static float user_lon = 121.5f;
static const char* city_name = "SHANGHAI";

// Active typhoon (mutable — MULTI selection)
static float typ_lat = 22.4f;
static float typ_lon = 131.8f;
static float typ_r7_km  = 280.0f;
static float typ_r10_km = 160.0f;
static float typ_r12_km = 80.0f;
static const char* typ_name = "KAZURO";
#define TYP_LAT    typ_lat
#define TYP_LON    typ_lon
#define TYP_R7_KM  typ_r7_km
#define TYP_R10_KM typ_r10_km
#define TYP_R12_KM typ_r12_km

// Batch 6/7: multi-storm set (demo seed → NMC live)
struct Storm {
  char name[12];
  float lat, lon;
  uint8_t cat;
  uint8_t wind_kt;
  uint16_t pressure;
  float r7, r10, r12;
};
#define STORM_MAX 4
static Storm STORMS[STORM_MAX];
static uint8_t storm_count = 0;
NmcTrack storm_tracks[STORM_MAX];
static uint8_t storm_sel = 0;  // active on MAIN/DETAIL
static uint8_t multi_sel = 0;  // highlight on MULTI
static int storm_sx[STORM_MAX];
static int storm_sy[STORM_MAX];
static bool data_stale = true;
static bool storms_are_live = false;  // false = demo seed; true = NMC
static uint32_t data_fetch_ms = 0;
static bool data_fetch_pending = false;
#define DATA_REFRESH_MS (30UL * 60UL * 1000UL)

static float distKm(float lat1, float lon1, float lat2, float lon2);

static int findStormIndexByName(const char* name, uint8_t n)
{
  if (!name || !name[0] || n == 0) return -1;
  for (uint8_t i = 0; i < n; i++) {
    if (strcasecmp(STORMS[i].name, name) == 0) return (int)i;
  }
  return -1;
}

static uint8_t findNearestStormIndex(uint8_t n)
{
  if (n == 0) return 0;
  uint8_t best = 0;
  float bestD = 1e9f;
  for (uint8_t i = 0; i < n; i++) {
    float d = distKm(user_lat, user_lon, STORMS[i].lat, STORMS[i].lon);
    if (d < bestD) {
      bestD = d;
      best = i;
    }
  }
  return best;
}

static void seedDemoStorms() {
  static const Storm DEMO[] = {
    {"KAZURO",  22.4f, 131.8f, CAT_SSTY, 140, 915, 280.f, 160.f, 80.f},
    {"LINFA",   15.2f, 128.5f, CAT_TS,    45, 995, 120.f,  60.f, 30.f},
    {"CHANHOM", 28.0f, 145.0f, CAT_TD,    30,1002,  80.f,  40.f, 20.f},
    {"KUJIRA",  18.5f, 115.0f, CAT_STS,   55, 985, 150.f,  80.f, 40.f},
  };
  storm_count = 4;
  memcpy(STORMS, DEMO, sizeof(DEMO));
  memset(storm_tracks, 0, sizeof(storm_tracks));
  data_stale = true;
  storms_are_live = false;
}

// Batch 6: night + idle dim
static uint32_t last_input_ms = 0;
static bool     screen_dimmed = false;
#define IDLE_DIM_MS 15000

// Mild exaggeration — lower than globe mode so rings stay proportional when zoomed
#define RING_EXAG  2.2f

static LGFX_Sprite* bg_sprite = nullptr;
static LGFX_Sprite* frame_sprite = nullptr; // MAIN composite for tilt
static LGFX_Sprite* view_sprite = nullptr;  // 240×135 present buffer (anti-flicker)
static LovyanGFX* G = nullptr;             // draw target (Display or sprite)

// Batch 5: IMU tilt (relative to calibrated hold pose)
static float imu_pitch = 0, imu_roll = 0;
static float tilt_pitch = 0, tilt_roll = 0;
static float cal_pitch = 0, cal_roll = 0; // neutral pose (K2 DBL / boot)
static bool     cal_ready = false;
static uint32_t imu_last_ms = 0;
static uint32_t imu_still_ms = 0;
static bool     imu_ok = false;
#define RAD2DEG 57.2957795f
#define TILT_LIM 15.0f
static uint16_t spiral_a1 = 0, spiral_a2 = 120, spiral_a3 = 240;
static uint32_t last_frame = 0;
static bool     blink_on = true;
static uint32_t last_blink = 0;
static uint8_t  pulse_phase = 0;
static uint32_t event_show_ms = 0;
static uint16_t event_color = TC_TEXT_DIM;
static char     event_text[28] = "";

static int typ_sx = 0, typ_sy = 0;
static int user_sx = 0, user_sy = 0;
static int ring_r7 = 4, ring_r10 = 3, ring_r12 = 2;

// --- Batch 2/4: pages + input ---
static Page page = PAGE_BOOT;
static bool night_mode = false;
static bool gyro_tilt = true;
static uint8_t brightness_user = 80; // user preference (before night/dim)
static bool units_kt = true;   // false = m/s
static bool units_km = true;   // false = nm
static uint16_t alert_radius_km = 200;
static uint8_t  brightness = 80; // effective 0..100 → Display.setBrightness
static uint32_t boot_start_ms = 0;
static uint16_t boot_angle = 0;
static uint8_t  boot_progress = 0;
static uint8_t  menu_cursor = 0;
static uint8_t  hold_progress = 0; // 0..100 while long-pressing
static int8_t   hold_btn = -1;    // 0=K1 1=K2 -1=none
static uint8_t  ring_mode = 0;    // DETAIL: 0=ALL 1=R7 2=R10 3=R12 4=OFF
static int8_t   track_sel = 0;    // selected index in TRACK[]
static bool     ui_dirty = true;  // static pages redraw only when dirty
static uint8_t  last_hold_prog = 255;

// Time Machine — playback advances in storm-hours (not fixed per index)
static bool     tm_playing = false;
static int8_t   tm_dir = 1;          // +1 forward / -1 reverse
static float    tm_pos = 0.0f;       // continuous index [0 .. TRACK_N-1]
static uint32_t tm_last_ms = 0;
static float    tm_lat = 0, tm_lon = 0, tm_r12 = 0;
static float    tm_hour_f = 0;       // playhead in data-hours relative to NOW
static uint8_t  tm_cat = 0, tm_wind = 0;
static uint16_t tm_pressure = 1000;
static int      tm_sx = 0, tm_sy = 0;
// Wall-clock ms per 1 hour of track data (3h gap → 300ms, 12h → 1.2s)
#define TM_MS_PER_DATA_HOUR  100
#define TM_END_HOLD_MS 900
static uint32_t tm_hold_until = 0;

// LOCATION
static uint8_t  loc_cursor = 2;   // Shanghai (Beijing, Nanjing, Shanghai…)
static uint8_t  loc_scroll = 0;
static uint8_t  loc_src = 2;      // 0=WIFI 1=GPS 2=MANUAL
static uint8_t  city_idx = 2;
static bool     loc_have_saved = false;  // NVS has a remembered observer fix
#define CITY_N 15
#define LOC_VISIBLE 10

// WiFi setup (scan → PIN → persistent STA)
#define WIFI_MAX_AP 12
#define WIFI_VISIBLE 8
#define WIFI_PASS_MAX 32
static const char WIFI_PASS_CHARS[] = "0123456789abcdefghijklmnopqrstuvwxyz";
#define WIFI_PASS_CHAR_N (sizeof(WIFI_PASS_CHARS) - 1)
static WifiPhase wifi_phase = WIFI_SCAN;
static char     wifi_pass_buf[WIFI_PASS_MAX + 1] = "sun123456"; // home AP
static uint8_t  wifi_pass_len = 9;
static uint8_t  wifi_idx = 0;
static Page     wifi_return = PAGE_MENU;
static char     wifi_ssid_sel[33] = "";
static char     wifi_ssids[WIFI_MAX_AP][33];
static int8_t   wifi_rssi[WIFI_MAX_AP];
static bool     wifi_open[WIFI_MAX_AP];
static uint8_t  wifi_count = 0;
static uint8_t  wifi_sel = 0;
static uint8_t  wifi_scroll = 0;
static bool     wifi_scan_pending = false;
static uint32_t wifi_conn_start = 0;
static char     wifi_pass_trying[65] = "";


static int wifiPassCharIndex(char c) {
  for (int i = 0; i < (int)WIFI_PASS_CHAR_N; i++)
    if (WIFI_PASS_CHARS[i] == c) return i;
  return 0;
}

static char wifiPassStep(char c, int dir) {
  int i = wifiPassCharIndex(c);
  i = (i + dir + (int)WIFI_PASS_CHAR_N) % (int)WIFI_PASS_CHAR_N;
  return WIFI_PASS_CHARS[i];
}

static void wifiPassSet(const char* pass) {
  if (!pass) pass = "";
  strncpy(wifi_pass_buf, pass, WIFI_PASS_MAX);
  wifi_pass_buf[WIFI_PASS_MAX] = 0;
  wifi_pass_len = (uint8_t)strlen(wifi_pass_buf);
  if (wifi_pass_len == 0) {
    wifi_pass_buf[0] = '0';
    wifi_pass_buf[1] = 0;
    wifi_pass_len = 1;
  }
  if (wifi_idx >= wifi_pass_len) wifi_idx = wifi_pass_len - 1;
}

// Combo uses raw press tracking; SHORT/LONG/DBL use M5Unified click count API
struct BtnTrack {
  bool     down;
  uint32_t t_down;
};
static BtnTrack btn[2]; // 0=K1 1=K2
static uint32_t combo_lock_until = 0;
static bool     combo_wait_release = false;
static uint32_t combo_suppress_until = 0; // ignore single-key after combo

#define BTN_LONG_MS    500   // matches M5Unified default hold / click-decide window
#define BTN_HOLD_BAR_MS 400  // Batch8: progress bar starts filling after 400ms
#define FW_VERSION     "v1.1"
#define FW_BATCH       "Batch8"

static void beep(uint16_t f, uint16_t ms) { (void)f; (void)ms; }

// --- Bright stars (from SkyCompass_Satellite earth_renderer.cpp) ---
struct BrightStar {
  const char* name;
  float ra;   // deg
  float dec;  // deg
  float mag;
  uint16_t color;
};

static const BrightStar BRIGHT_STARS[] = {
  {"Sirius",          101.2871f, -16.7161f, -1.46f, 0xFFFF},
  {"Canopus",          95.9879f, -52.6956f, -0.74f, 0xFFFF},
  {"Rigil Kentaurus", 219.9021f, -60.8339f, -0.27f, 0xFFE0},
  {"Arcturus",        213.9154f,  19.1822f, -0.05f, 0xFD20},
  {"Vega",            279.2346f,  38.7836f,  0.03f, 0xFFFF},
  {"Capella",          79.1725f,  45.9981f,  0.08f, 0xFFE0},
  {"Rigel",            78.6346f,  -8.2017f,  0.13f, 0x07FF},
  {"Procyon",         114.8254f,   5.2250f,  0.34f, 0xFFFF},
  {"Achernar",         24.4283f, -57.2367f,  0.46f, 0x07FF},
  {"Betelgeuse",       88.7929f,   7.4069f,  0.50f, 0xF800},
  {"Hadar",           210.9558f, -60.3731f,  0.61f, 0x07FF},
  {"Altair",          297.6958f,   8.8683f,  0.76f, 0xFFFF},
  {"Acrux",           186.6496f, -63.0992f,  0.76f, 0x07FF},
  {"Aldebaran",        68.9800f,  16.5092f,  0.86f, 0xFD20},
  {"Antares",        247.3517f, -26.4319f,  0.96f, 0xF800},
  {"Spica",           201.2983f, -11.1614f,  0.97f, 0x07FF},
  {"Pollux",          116.3287f,  28.0261f,  1.14f, 0xFD20},
  {"Fomalhaut",       344.4125f, -29.6222f,  1.16f, 0xFFFF},
  {"Deneb",           310.3579f,  45.2803f,  1.25f, 0xFFFF},
  {"Mimosa",          191.9300f, -59.6886f,  1.25f, 0x07FF},
  {"Regulus",         152.0929f,  11.9672f,  1.35f, 0x07FF},
  {"Adhara",          183.7862f, -58.7489f,  1.50f, 0x07FF},
  {"Castor",          113.6500f,  31.8883f,  1.58f, 0xFFFF},
  {"Gacrux",          187.7913f, -57.1131f,  1.63f, 0xF800},
  {"Shaula",          263.4021f, -37.1036f,  1.62f, 0x07FF},
};
static const int NUM_BRIGHT_STARS = 25;

// SkyCompass drawStars: RA/Dec → GMST → celestial sphere outside Earth disk
static void drawStars(LGFX_Sprite* s, uint32_t unixTime) {
  // Regional zoom fills the viewport — skip celestial backdrop
  (void)s; (void)unixTime;
}

static uint32_t getUnixTime() {
  time_t t = time(nullptr);
  if (t > 1700000000L) return (uint32_t)t;
  return 1752213600UL; // 2025-07-11 06:00 UTC demo (matches HUD date)
}

// --- Orthographic projection (SkyCompass_Satellite) ---
static bool project(float lat, float lon, int& outX, int& outY) {
  float latR = lat * DEG2RAD, lonR = lon * DEG2RAD;
  float cLat = ctr_lat * DEG2RAD, cLon = ctr_lon * DEG2RAD;
  float cos_c = sinf(cLat) * sinf(latR) + cosf(cLat) * cosf(latR) * cosf(lonR - cLon);
  if (cos_c < 0.15f) return false; // regional: drop far side / limb
  float x = EARTH_R * cosf(latR) * sinf(lonR - cLon);
  float y = EARTH_R * (cosf(cLat) * sinf(latR) - sinf(cLat) * cosf(latR) * cosf(lonR - cLon));
  outX = earth_cx + (int)x;
  outY = earth_cy - (int)y;
  return true;
}

static int kmToPx(float km) {
  int px = (int)(km / R_EARTH_KM * EARTH_R * RING_EXAG + 0.5f);
  return px < 2 ? 2 : px;
}

// Haversine km (for HUD)
static float distKm(float lat1, float lon1, float lat2, float lon2) {
  float p1 = lat1 * DEG2RAD, p2 = lat2 * DEG2RAD;
  float dphi = (lat2 - lat1) * DEG2RAD, dl = (lon2 - lon1) * DEG2RAD;
  float a = sinf(dphi/2)*sinf(dphi/2) + cosf(p1)*cosf(p2)*sinf(dl/2)*sinf(dl/2);
  return 2.0f * R_EARTH_KM * asinf(sqrtf(a));
}

// Bearing from (lat1,lon1) → (lat2,lon2) as N/NE/E/...
static const char* bearingTo(float lat1, float lon1, float lat2, float lon2) {
  float y = sinf((lon2 - lon1) * DEG2RAD) * cosf(lat2 * DEG2RAD);
  float x = cosf(lat1 * DEG2RAD) * sinf(lat2 * DEG2RAD)
          - sinf(lat1 * DEG2RAD) * cosf(lat2 * DEG2RAD) * cosf((lon2 - lon1) * DEG2RAD);
  float br = atan2f(y, x) / DEG2RAD;
  if (br < 0) br += 360.0f;
  static const char* dirs[] = {"N","NE","E","SE","S","SW","W","NW"};
  return dirs[(int)((br + 22.5f) / 45.0f) % 8];
}

// --- Continents from SkyCompass earth_data.h ---
static bool earth_pending = false;

// Province outlines already cover China coasts (incl. Taiwan/Hainan). Skip the
// coarser world_map coastline there so the two layers don't double / misalign.
static bool isChinaProvinceCoast(float latRad, float sinLon, float cosLon) {
  float lat = latRad * RAD2DEG;
  float lon = atan2f(sinLon, cosLon) * RAD2DEG;
  return lat >= 18.0f && lat <= 42.0f && lon >= 108.0f && lon <= 125.0f;
}

static void drawContinents(LGFX_Sprite* s) {
  float cLat = ctr_lat * DEG2RAD, cLon = ctr_lon * DEG2RAD;
  float sin_cLat = sinf(cLat), cos_cLat = cosf(cLat);
  float sin_cLon = sinf(cLon), cos_cLon = cosf(cLon);

  for (int i = 0; i < world_map_count; i++) {
    if ((i & 7) == 0) GetHAL().feedTheDog();
    const MapPoint* pts = world_map[i].points;
    int n = world_map[i].length;
    int prevX = -1, prevY = -1;
    bool prevVis = false;

    // High-res canvas: keep denser polylines; only thin out at extreme zoom.
    const int step = (EARTH_R > tcU(1600)) ? 3 : 2;
    for (int j = 0; j < n; j += step) {
      float sin_lat = pts[j].sinLat, cos_lat = pts[j].cosLat;
      float sin_lon = pts[j].sinLon, cos_lon = pts[j].cosLon;

      // Prefer province coastlines over world_map in China waters.
      if (isChinaProvinceCoast(pts[j].latRad, sin_lon, cos_lon)) {
        prevVis = false;
        continue;
      }

      float cos_dLon = cos_lon * cos_cLon + sin_lon * sin_cLon;
      float sin_dLon = sin_lon * cos_cLon - cos_lon * sin_cLon;
      float cos_c = sin_cLat * sin_lat + cos_cLat * cos_lat * cos_dLon;

      if (cos_c >= 0.15f) { // keep near-center coastlines when zoomed
        float x = EARTH_R * cos_lat * sin_dLon;
        float y = EARTH_R * (cos_cLat * sin_lat - sin_cLat * cos_lat * cos_dLon);
        int outX = earth_cx + (int)x;
        int outY = earth_cy - (int)y;
        // Skip far off-screen vertices (regional zoom)
        if (outX < -40 || outX > FRAME_W + 40 || outY < -40 || outY > FRAME_H + 40) {
          prevVis = false;
          continue;
        }

        if (prevVis && abs(outX - prevX) < tcU(200) && abs(outY - prevY) < tcU(200)) {
          float latRad = pts[j].latRad;
          uint8_t cr = 45, cg = 150, cb = 85;
          if (latRad > 0) {
            float f = latRad / 1.5708f; if (f > 1) f = 1;
            cr = (uint8_t)(45 * (1 - f));
            cb = (uint8_t)(85 * (1 - f) + 120 * f);
          } else {
            float f = -latRad / 1.5708f; if (f > 1) f = 1;
            cg = (uint8_t)(150 * (1 - f) + 60 * f);
          }
          uint16_t col = s->color565(cr, cg, cb);
          if (night_mode) {
            // red-tint coastline
            uint8_t nr = (uint8_t)((cr + cg / 2) / 3 + 40); if (nr > 255) nr = 255;
            col = s->color565(nr, nr / 5, nr / 8);
          }
          s->drawLine(prevX, prevY, outX, outY, col);
          // Light 2px stroke — enough on 466 without StickS3 chunkiness
          s->drawLine(prevX, prevY + 1, outX, outY + 1, col);
        }
        prevX = outX; prevY = outY; prevVis = true;
      } else {
        prevVis = false;
      }
    }
  }
}

// Project & stroke a MapPath list onto the map sprite.
static void strokeMapPaths(LGFX_Sprite* s, const MapPath* paths, int count,
                           uint16_t col, bool thick)
{
  float cLat = ctr_lat * DEG2RAD, cLon = ctr_lon * DEG2RAD;
  float sin_cLat = sinf(cLat), cos_cLat = cosf(cLat);
  float sin_cLon = sinf(cLon), cos_cLon = cosf(cLon);
  const int maxSeg = tcU(220);
  const int margin = tcU(40);

  for (int i = 0; i < count; i++) {
    if ((i & 7) == 0) GetHAL().feedTheDog();
    const MapPoint* pts = paths[i].points;
    int n = paths[i].length;
    int prevX = -1, prevY = -1;
    bool prevVis = false;

    for (int j = 0; j < n; j++) {
      float sin_lat = pts[j].sinLat, cos_lat = pts[j].cosLat;
      float sin_lon = pts[j].sinLon, cos_lon = pts[j].cosLon;
      float cos_dLon = cos_lon * cos_cLon + sin_lon * sin_cLon;
      float sin_dLon = sin_lon * cos_cLon - cos_lon * sin_cLon;
      float cos_c = sin_cLat * sin_lat + cos_cLat * cos_lat * cos_dLon;

      if (cos_c >= 0.15f) {
        float x = EARTH_R * cos_lat * sin_dLon;
        float y = EARTH_R * (cos_cLat * sin_lat - sin_cLat * cos_lat * cos_dLon);
        int outX = earth_cx + (int)x;
        int outY = earth_cy - (int)y;
        if (outX < -margin || outX > FRAME_W + margin ||
            outY < -margin || outY > FRAME_H + margin) {
          prevVis = false;
          continue;
        }
        if (prevVis && abs(outX - prevX) < maxSeg && abs(outY - prevY) < maxSeg) {
          s->drawLine(prevX, prevY, outX, outY, col);
          if (thick) {
            s->drawLine(prevX, prevY + 1, outX, outY + 1, col);
            s->drawLine(prevX + 1, prevY, outX + 1, outY, col);
          }
        }
        prevX = outX; prevY = outY; prevVis = true;
      } else {
        prevVis = false;
      }
    }
  }
}

// Province borders — finer data, soft inland color
static void drawChinaProvinces(LGFX_Sprite* s) {
  uint16_t col = night_mode ? s->color565(78, 32, 32) : s->color565(70, 118, 108);
  strokeMapPaths(s, china_provinces, china_provinces_count, col, false);
}

// Regional lat/lon grid (10° meridians / 5° parallels)
static void drawGeoGrid(LGFX_Sprite* s, uint16_t color) {
  for (int lon = -180; lon < 180; lon += 10) {
    int px = -1, py = -1; bool pv = false;
    for (int lat = -80; lat <= 80; lat += 2) {
      int x, y;
      if (project((float)lat, (float)lon, x, y)) {
        if (x < -40 || x > FRAME_W + 40 || y < -40 || y > FRAME_H + 40) { pv = false; continue; }
        if (pv && abs(x-px) < 120 && abs(y-py) < 120) s->drawLine(px, py, x, y, color);
        px = x; py = y; pv = true;
      } else pv = false;
    }
  }
  for (int lat = -60; lat <= 60; lat += 5) {
    int px = -1, py = -1; bool pv = false;
    uint16_t c = (lat == 0)
      ? (night_mode ? s->color565(90, 20, 20) : s->color565(50, 110, 120))
      : color;
    for (int lon = -180; lon <= 180; lon += 2) {
      int x, y;
      if (project((float)lat, (float)lon, x, y)) {
        if (x < -40 || x > FRAME_W + 40 || y < -40 || y > FRAME_H + 40) { pv = false; continue; }
        if (pv && abs(x-px) < 120 && abs(y-py) < 120) s->drawLine(px, py, x, y, c);
        px = x; py = y; pv = true;
      } else pv = false;
    }
  }
}

static void buildEarthBg(LGFX_Sprite* s) {
  // Oversized regional map — margins supply pixels when view tilts
  if (night_mode) {
    s->fillScreen(s->color565(28, 4, 4));
    drawGeoGrid(s, s->color565(70, 15, 15));
  } else {
    s->fillScreen(TC_OCEAN);
    drawGeoGrid(s, s->color565(22, 55, 65));
  }
  drawContinents(s);
  drawChinaProvinces(s);
}

// --- Unified track: past → NOW → forecast ---
struct TrackPt {
  int16_t hour;   // relative to NOW
  float   lat, lon;
  uint16_t pressure;
  uint8_t  wind_kt;
  uint8_t  cat;     // strength bars
  float    r12_km;  // 12-level ring at this time
};
#define TRACK_MAX     24
static TrackPt TRACK[TRACK_MAX];
static uint8_t TRACK_N = 10;
static uint8_t TRACK_NOW_IDX = 5;
static int track_s[TRACK_MAX][2];

static void seedDemoTrack() {
  static const TrackPt DEMO[] = {
    {-168, 12.0f, 148.0f, 998,  35, 1, 140.0f},
    {-144, 13.2f, 146.5f, 992,  45, 1, 130.0f},
    {-120, 14.5f, 144.8f, 985,  55, 2, 120.0f},
    { -96, 16.0f, 142.5f, 975,  70, 2, 110.0f},
    { -72, 17.5f, 140.0f, 960,  90, 3, 100.0f},
    { -48, 19.0f, 137.5f, 945, 110, 4,  90.0f},
    { -24, 20.5f, 135.0f, 930, 125, 5,  85.0f},
    { -12, 21.5f, 133.5f, 920, 135, 5,  80.0f},
    {   0, 22.4f, 131.8f, 915, 140, 5,  80.0f},
    {  12, 23.5f, 129.5f, 925, 130, 5,  75.0f},
    {  24, 25.0f, 127.0f, 940, 115, 4,  65.0f},
    {  48, 27.0f, 124.5f, 960,  95, 3,  55.0f},
    {  72, 29.0f, 122.0f, 980,  70, 2,  45.0f},
    {  96, 31.0f, 120.0f, 995,  45, 1,  35.0f},
  };
  TRACK_N = 14;
  TRACK_NOW_IDX = 8;
  memcpy(TRACK, DEMO, sizeof(DEMO));
}

static void loadTrackFromNmc(uint8_t idx) {
  if (idx >= storm_count) return;
  const NmcTrack& src = storm_tracks[idx];
  if (src.n == 0) { seedDemoTrack(); return; }
  TRACK_N = src.n;
  TRACK_NOW_IDX = src.now_idx;
  for (uint8_t i = 0; i < src.n; i++) {
    TRACK[i].hour = src.pts[i].hour;
    TRACK[i].lat = src.pts[i].lat;
    TRACK[i].lon = src.pts[i].lon;
    TRACK[i].pressure = src.pts[i].pressure;
    TRACK[i].wind_kt = src.pts[i].wind_kt;
    TRACK[i].cat = src.pts[i].cat;
    TRACK[i].r12_km = src.pts[i].r12_km;
  }
}

static void projectStorms();

static void projectAll() {
  if (!project(user_lat, user_lon, user_sx, user_sy))
    user_sx = user_sy = -1;
  project(TYP_LAT, TYP_LON, typ_sx, typ_sy);
  for (int i = 0; i < (int)TRACK_N; i++)
    if (!project(TRACK[i].lat, TRACK[i].lon, track_s[i][0], track_s[i][1]))
      track_s[i][0] = -1;

  ring_r7  = kmToPx(TYP_R7_KM);
  ring_r10 = kmToPx(TYP_R10_KM);
  ring_r12 = kmToPx(TYP_R12_KM);
  // do not clobber user track_sel every frame — only when unset
  if (track_sel < 0 || track_sel >= (int)TRACK_N) track_sel = TRACK_NOW_IDX;
}

// Rebuild map when camera moves enough; tiny moves just reproject markers.
static void setMapCenter(float lat, float lon, bool force = false) {
  float dlat = fabsf(lat - ctr_lat);
  float dlon = fabsf(lon - ctr_lon);
  ctr_lat = lat;
  ctr_lon = lon;
  if (force || dlat >= 0.06f || dlon >= 0.06f) {
    if (bg_sprite) buildEarthBg(bg_sprite);
  }
  projectAll();
  projectStorms();
}

// --- Preset cities (Batch 4 LOCATION) ---
struct City {
  const char* name;
  float lat, lon;
};
static const City CITIES[] = {
  {"BEIJING",   39.9f, 116.4f},
  {"NANJING",   32.1f, 118.8f},
  {"SHANGHAI",  31.2f, 121.5f},
  {"GUANGZHOU", 23.1f, 113.3f},
  {"SHENZHEN",  22.5f, 114.1f},
  {"HONG KONG", 22.3f, 114.2f},
  {"TAIPEI",    25.0f, 121.5f},
  {"XIAMEN",    24.5f, 118.1f},
  {"FUZHOU",    26.1f, 119.3f},
  {"HANGZHOU",  30.3f, 120.2f},
  {"NINGBO",    29.9f, 121.5f},
  {"QINGDAO",   36.1f, 120.4f},
  {"OKINAWA",   26.2f, 127.7f},
  {"MANILA",    14.6f, 121.0f},
  {"TOKYO",     35.7f, 139.7f},
};

static char city_name_buf[16] = "SHANGHAI";

static void saveObserverLocation()
{
  nvs_handle_t h = 0;
  if (nvs_open("tcloc", NVS_READWRITE, &h) != ESP_OK) return;
  nvs_set_blob(h, "lat", &user_lat, sizeof(user_lat));
  nvs_set_blob(h, "lon", &user_lon, sizeof(user_lon));
  nvs_set_str(h, "name", city_name_buf);
  nvs_set_u8(h, "src", loc_src);
  nvs_set_u8(h, "city", city_idx);
  nvs_commit(h);
  nvs_close(h);
  loc_have_saved = true;
}

static bool loadObserverLocation()
{
  nvs_handle_t h = 0;
  if (nvs_open("tcloc", NVS_READONLY, &h) != ESP_OK) return false;
  float lat = 0, lon = 0;
  size_t latLen = sizeof(lat), lonLen = sizeof(lon);
  char name[16] = {};
  size_t nameLen = sizeof(name);
  uint8_t src = 2, city = 2;
  bool ok = nvs_get_blob(h, "lat", &lat, &latLen) == ESP_OK
         && nvs_get_blob(h, "lon", &lon, &lonLen) == ESP_OK
         && latLen == sizeof(lat) && lonLen == sizeof(lon)
         && lat >= -90.0f && lat <= 90.0f && lon >= -180.0f && lon <= 180.0f;
  if (ok) {
    nvs_get_str(h, "name", name, &nameLen);
    nvs_get_u8(h, "src", &src);
    nvs_get_u8(h, "city", &city);
  }
  nvs_close(h);
  if (!ok) return false;

  user_lat = lat;
  user_lon = lon;
  if (name[0]) {
    std::strncpy(city_name_buf, name, sizeof(city_name_buf) - 1);
    city_name_buf[sizeof(city_name_buf) - 1] = 0;
  } else {
    std::strncpy(city_name_buf, "SAVED", sizeof(city_name_buf) - 1);
  }
  city_name = city_name_buf;
  loc_src = (src <= 2) ? src : 2;
  city_idx = (city < CITY_N) ? city : 2;
  loc_cursor = city_idx;
  loc_have_saved = true;
  ctr_lat = user_lat;
  ctr_lon = user_lon;
  ESP_LOGI("Typhoon", "[LOC] recall %s %.2fN %.2fE src=%u", city_name, user_lat, user_lon, loc_src);
  return true;
}

static void applyCity(uint8_t idx) {
  if (idx >= CITY_N) return;
  user_lat = CITIES[idx].lat;
  user_lon = CITIES[idx].lon;
  std::strncpy(city_name_buf, CITIES[idx].name, sizeof(city_name_buf) - 1);
  city_name_buf[sizeof(city_name_buf) - 1] = 0;
  city_name = city_name_buf;
  loc_cursor = idx;
  city_idx = idx;
  loc_src = 2;
  saveObserverLocation();
  // MAIN overview is centered on the observer
  if (page == PAGE_MAIN || page == PAGE_BOOT || page == PAGE_LOCATION) {
    setMapCenter(user_lat, user_lon, true);
  } else {
    projectAll();
  }
  ui_dirty = true;
  ESP_LOGI("Typhoon", "[LOC] observer -> %s %.1fN %.1fE", city_name, user_lat, user_lon);
}

static void applyLatLon(float lat, float lon, const char* name) {
  user_lat = lat;
  user_lon = lon;
  if (name && name[0]) {
    std::strncpy(city_name_buf, name, sizeof(city_name_buf) - 1);
    city_name_buf[sizeof(city_name_buf) - 1] = 0;
  } else {
    std::strncpy(city_name_buf, "IP LOC", sizeof(city_name_buf) - 1);
  }
  city_name = city_name_buf;
  // Snap cursor to nearest preset city for the list UI.
  float best = 1e9f;
  uint8_t besti = city_idx;
  for (uint8_t i = 0; i < CITY_N; i++) {
    float d = distKm(lat, lon, CITIES[i].lat, CITIES[i].lon);
    if (d < best) { best = d; besti = i; }
  }
  loc_cursor = besti;
  city_idx = besti;
  loc_src = 0;
  saveObserverLocation();
  if (page == PAGE_MAIN || page == PAGE_BOOT || page == PAGE_LOCATION) {
    setMapCenter(user_lat, user_lon, true);
  } else {
    projectAll();
  }
  ui_dirty = true;
  ESP_LOGI("Typhoon", "[LOC] wifi/ip -> %s %.2fN %.2fE", city_name, user_lat, user_lon);
}

// Async IP geolocation (LOCATION → WIFI/IP)
static volatile bool geo_busy = false;
static volatile bool geo_done = false;
static bool geo_ok = false;
static float geo_lat = 0, geo_lon = 0;
static char geo_city[16] = {};

static void geoTask(void*) {
  geo_ok = tcIpGeolocate(&geo_lat, &geo_lon, geo_city, sizeof(geo_city));
  geo_done = true;
  geo_busy = false;
  vTaskDelete(nullptr);
}

static void startIpLocate() {
  if (geo_busy) {
    showEvent("LOCATING...", TC_YELLOW);
    return;
  }
  if (!tcWifiConnected()) {
    showEvent("WIFI OFF", TC_RED);
    return;
  }
  geo_busy = true;
  geo_done = false;
  geo_ok = false;
  showEvent("IP LOCATE...", TC_CYAN);
  xTaskCreate(geoTask, "tc_geo", 8192, nullptr, 3, nullptr);
}

static void pollIpLocate() {
  if (!geo_done) return;
  geo_done = false;
  if (!geo_ok) {
    showEvent("GEO FAIL", TC_RED);
    ui_dirty = true;
    return;
  }
  applyLatLon(geo_lat, geo_lon, geo_city);
  showEvent(city_name_buf, TC_GREEN);
  gotoPage(PAGE_MAIN);
}

static void applyBrightness() {
  // StickS3 backlight 0..255 — night caps 40%, idle dim → 20%
  uint8_t pct = brightness;
  if (night_mode && pct > 40) pct = 40;
  if (screen_dimmed) pct = 20;
  GetHAL().setBackLightBrightness(static_cast<int>(pct));
}

static void noteInput() {
  last_input_ms = GetHAL().millis();
  if (screen_dimmed) {
    screen_dimmed = false;
    applyBrightness();
  }
}

// Remap RGB565 → red-tinted night palette
static uint16_t C(uint16_t c) {
  if (!night_mode) return c;
  uint8_t r = (c >> 11) & 0x1F;
  uint8_t g = (c >> 5) & 0x3F;
  uint8_t b = c & 0x1F;
  int lum = (r * 38 + (g >> 1) * 75 + b * 15) >> 7; // ~0..31
  if (lum < 2) return 0x1000;
  uint8_t nr = (uint8_t)(6 + lum); if (nr > 31) nr = 31;
  uint8_t ng = (uint8_t)(lum / 4); if (ng > 20) ng = 20;
  uint8_t nb = (uint8_t)(lum / 10);
  return (uint16_t)((nr << 11) | (ng << 5) | nb);
}

static uint16_t catColor(uint8_t cat) {
  static const uint16_t cols[] = {
    TC_CAT_TD, TC_CAT_TS, TC_CAT_STS, TC_CAT_TY, TC_CAT_STY, TC_CAT_SSTY
  };
  return cols[cat % 6];
}

static const char* catName(uint8_t cat) {
  static const char* n[] = {"TD","TS","STS","TY","STY","SSTY"};
  return n[cat % 6];
}

static void applyStorm(uint8_t idx) {
  if (storm_count == 0) seedDemoStorms();
  if (idx >= storm_count) idx = 0;
  storm_sel = idx;
  const Storm& s = STORMS[idx];
  typ_lat = s.lat; typ_lon = s.lon;
  typ_r7_km = s.r7; typ_r10_km = s.r10; typ_r12_km = s.r12;
  typ_name = s.name;
  loadTrackFromNmc(idx);
  track_sel = TRACK_NOW_IDX;
  if (page == PAGE_DETAIL || page == PAGE_TRACK) {
    setMapCenter(TYP_LAT, TYP_LON, true);
  } else {
    projectAll();
  }
}

static void applyNmcStorms(const NmcStorm* src, const NmcTrack* tr, uint8_t n) {
  if (n == 0) return;
  if (n > STORM_MAX) n = STORM_MAX;
  storm_count = n;
  for (uint8_t i = 0; i < n; i++) {
    strncpy(STORMS[i].name, src[i].name, sizeof(STORMS[i].name) - 1);
    STORMS[i].name[sizeof(STORMS[i].name) - 1] = 0;
    STORMS[i].lat = src[i].lat;
    STORMS[i].lon = src[i].lon;
    STORMS[i].cat = src[i].cat;
    STORMS[i].wind_kt = src[i].wind_kt;
    STORMS[i].pressure = src[i].pressure;
    STORMS[i].r7 = src[i].r7;
    STORMS[i].r10 = src[i].r10;
    STORMS[i].r12 = src[i].r12;
    storm_tracks[i] = tr[i];
  }
  data_stale = false;
  data_fetch_ms = GetHAL().millis();
  char prevName[16] = {};
  if (typ_name) std::strncpy(prevName, typ_name, sizeof(prevName) - 1);
  const bool wasLive = storms_are_live;
  storms_are_live = true;
  uint8_t sel = 0;
  if (wasLive) {
    int m = findStormIndexByName(prevName, storm_count);
    sel = (m >= 0) ? (uint8_t)m : findNearestStormIndex(storm_count);
  } else {
    sel = findNearestStormIndex(storm_count);
  }
  if (multi_sel >= storm_count) multi_sel = sel;
  applyStorm(sel);
  ESP_LOGI("Typhoon", "[DATA] live NMC x%d selected=%s", storm_count, typ_name);
}

static void tryFetchTyphoonData() { /* handled by DataService */ }

static void projectStorms() {
  for (int i = 0; i < storm_count; i++) {
    if (!project(STORMS[i].lat, STORMS[i].lon, storm_sx[i], storm_sy[i]))
      storm_sx[i] = storm_sy[i] = -1;
  }
}

static void setMapZoom(int r) {
  if (earth_r == r) { projectAll(); projectStorms(); return; }
  earth_r = r;
  buildEarthBg(bg_sprite);
  projectAll();
  projectStorms();
}

static void setNightMode(bool on) {
  if (night_mode == on) return;
  night_mode = on;
  screen_dimmed = false;
  buildEarthBg(bg_sprite);
  applyBrightness();
  ui_dirty = true;
}

bool wifiIsConnected() {
  return tcWifiConnected();
}

static void wifiSaveCreds(const char* ssid, const char* pass) {
  tcWifiPrefsOpen(false);
  tcWifiPrefsPutSsid(ssid);
  tcWifiPrefsPutPass(pass);
  tcWifiPrefsClose();
}

static void wifiClearCreds() {
  tcWifiPrefsOpen(false);
  tcWifiPrefsClear();
  tcWifiPrefsClose();
}

static void wifiStartConnect(const char* ssid, const char* pass) {
  strncpy(wifi_ssid_sel, ssid, sizeof(wifi_ssid_sel) - 1);
  wifi_ssid_sel[sizeof(wifi_ssid_sel) - 1] = 0;
  strncpy(wifi_pass_trying, pass ? pass : "", sizeof(wifi_pass_trying) - 1);
  wifi_pass_trying[sizeof(wifi_pass_trying) - 1] = 0;

  // Soft disconnect — keep radio on (wifioff=true caused instant CONNECT_FAILED)
  tcWifiDisconnect(false);
  GetHAL().delay(300);
  tcWifiModeSta();
  tcWifiSetAutoReconnect(false);
  /* persistent off */;
  GetHAL().delay(150);

  ESP_LOGI("Typhoon", "[WIFI] begin ssid='%s' pass_len=%u", wifi_ssid_sel,
                (unsigned)strlen(wifi_pass_trying));
  wifi_phase = WIFI_CONNECTING;
  wifi_conn_start = GetHAL().millis();
  ui_dirty = true;
  tcWifiBegin(wifi_ssid_sel, wifi_pass_trying);
}

static void wifiTryRestore() {
  // Keep remembered observer location — do not force WIFI/IP or re-geolocate.
  if (tcWifiConnected()) data_fetch_pending = true;
}

static void wifiDoScan() {
  tcWifiModeSta();
  // Do not disconnect an existing link — ESP32 can scan while connected
  int n = tcWifiScan();
  wifi_count = 0;
  wifi_sel = 0;
  wifi_scroll = 0;
  if (n < 0) n = 0;
  for (int i = 0; i < n && wifi_count < WIFI_MAX_AP; i++) {
    const char* s = tcWifiScanSsid(i);
    if (!s || !s[0]) continue;
    strncpy(wifi_ssids[wifi_count], s, 32);
    wifi_ssids[wifi_count][32] = 0;
    wifi_rssi[wifi_count] = (int8_t)tcWifiScanRssi(i);
    wifi_open[wifi_count] = (tcWifiScanOpen(i));
    wifi_count++;
  }
  tcWifiScanDelete();
  tcWifiSetAutoReconnect(true);
  ESP_LOGI("Typhoon", "[WIFI] scan found %d", wifi_count);
}

static void enterWifiSetup(Page ret) {
  // WiFi provisioning lives in StopWatch system Setup — not inside Typhoon.
  (void)ret;
  showEvent("USE SYSTEM WIFI", TC_YELLOW);
}

static void dashedCircle(int cx, int cy, int r, uint16_t color, int step, int dash) {
  if (r < 2) return;
  for (int a = 0; a < 360; a += step)
    for (int d = 0; d < dash; d++) {
      float rad = (a + d) * DEG2RAD;
      G->drawPixel(cx + (int)(cosf(rad)*r), cy + (int)(sinf(rad)*r), color);
    }
}

static void drawQuarterArc(int cx, int cy, int r, uint16_t color, uint16_t angle, int thick) {
  for (int a = 0; a < 90; a++) {
    float rad = (angle + a) * DEG2RAD;
    int x = cx + (int)(cosf(rad) * r);
    int y = cy + (int)(sinf(rad) * r);
    G->drawPixel(x, y, color);
    if (thick > 1)
      G->drawPixel(cx + (int)(cosf(rad)*(r-1)), cy + (int)(sinf(rad)*(r-1)), color);
  }
}

static void drawTracks() {
  const int dashStep = tcU(3);
  const int pt = tcU(2);
  const int ptSel = tcU(4);
  // Lines: past solid (2px), future dashed
  for (int i = 0; i < (int)TRACK_N - 1; i++) {
    if (track_s[i][0] < 0 || track_s[i+1][0] < 0) continue;
    int x0 = track_s[i][0], y0 = track_s[i][1];
    int x1 = track_s[i+1][0], y1 = track_s[i+1][1];
    if (i < TRACK_NOW_IDX) {
      G->drawLine(x0, y0, x1, y1, TC_TEXT_DIM);
      G->drawLine(x0, y0 + 1, x1, y1 + 1, TC_TEXT_DIM);
    } else {
      int dx = x1 - x0, dy = y1 - y0;
      int len = (int)sqrtf((float)(dx*dx + dy*dy)); if (len < 1) continue;
      uint16_t col = (page == PAGE_TRACK && i >= track_sel) ? TC_CYAN_DIM : TC_CYAN;
      for (int s = 0; s < len; s += dashStep)
        if ((s / dashStep) % 2 == 0) {
          int px = x0 + dx * s / len, py = y0 + dy * s / len;
          G->drawPixel(px, py, col);
          G->drawPixel(px, py + 1, col);
        }
    }
  }

  // Points
  for (int i = 0; i < TRACK_N; i++) {
    if (track_s[i][0] < 0) continue;
    int x = track_s[i][0], y = track_s[i][1];
    if (page == PAGE_TRACK && i == track_sel) {
      G->fillRect(x - ptSel, y - ptSel, ptSel * 2 + 1, ptSel * 2 + 1, TC_YELLOW);
      G->fillRect(x - pt, y - pt, pt * 2 + 1, pt * 2 + 1, TC_WHITE);
    } else if (i < TRACK_NOW_IDX) {
      G->fillRect(x - pt, y - pt, pt * 2 + 1, pt * 2 + 1, TC_TEXT_DIM);
    } else if (i == TRACK_NOW_IDX) {
      G->fillRect(x - pt, y - pt, pt * 2 + 1, pt * 2 + 1, TC_WHITE);
    } else if (page == PAGE_TRACK && i > track_sel) {
      G->drawRect(x - pt, y - pt, pt * 2 + 1, pt * 2 + 1, TC_CYAN_DIM);
    } else {
      G->fillRect(x - pt, y - pt, pt * 2 + 1, pt * 2 + 1, TC_CYAN);
    }
  }
}

static void drawWindRings() {
  if (page == PAGE_TRACK || page == PAGE_MULTI) return; // MULTI: ring on selected only
  const int st = tcU(8), dash = 4;
  auto drawAll = [&]() {
    dashedCircle(typ_sx, typ_sy, ring_r7,  C(TC_YELLOW), st, dash);
    dashedCircle(typ_sx, typ_sy, ring_r10, C(TC_ORANGE), st, dash);
    dashedCircle(typ_sx, typ_sy, ring_r12, C(TC_RED),    st + 2, dash);
  };
  if (page == PAGE_DETAIL) {
    switch (ring_mode) {
      case 0: drawAll(); break;
      case 1: dashedCircle(typ_sx, typ_sy, ring_r7,  C(TC_YELLOW), st, dash); break;
      case 2: dashedCircle(typ_sx, typ_sy, ring_r10, C(TC_ORANGE), st, dash); break;
      case 3: dashedCircle(typ_sx, typ_sy, ring_r12, C(TC_RED),    st + 2, dash); break;
      default: break; // OFF
    }
    return;
  }
  drawAll();
}

static void drawStormIcon(int sx, int sy, uint8_t cat, bool selected) {
  // size by category: TD/TS tiny, STS/TY medium, STY/SSTY full
  uint8_t tier = (cat <= CAT_TS) ? 0 : (cat <= CAT_TY) ? 1 : 2;
  const int R1[] = {tcU(2), tcU(3), tcU(4)};
  const int R2[] = {tcU(3), tcU(5), tcU(6)};
  const int R3[] = {tcU(4), tcU(6), tcU(8)};
  uint16_t col = C(catColor(cat));
  if (selected || tier == 2) {
    drawQuarterArc(sx, sy, R1[tier], C(TC_CYAN), spiral_a1, 2);
    drawQuarterArc(sx, sy, R2[tier], C(G->color565(50, 160, 170)), spiral_a2, 2);
    drawQuarterArc(sx, sy, R3[tier], C(G->color565(35, 110, 120)), spiral_a3, 2);
    G->drawRect(sx - tcU(2), sy - tcU(2), tcU(5), tcU(5), C(TC_RED));
    if (blink_on) G->fillRect(sx - tcU(1), sy - tcU(1), tcU(3), tcU(3), C(TC_WHITE));
    else          G->fillRect(sx - tcU(1), sy - tcU(1), tcU(3), tcU(3), C(TC_CYAN_DIM));
  } else {
    G->drawCircle(sx, sy, R2[tier], col);
    G->fillRect(sx - tcU(1), sy - tcU(1), tcU(2) + tier, tcU(2) + tier, col);
  }
}

static void drawTyphoon() {
  drawStormIcon(typ_sx, typ_sy, STORMS[storm_sel].cat, true);
}

static void drawUserMarker() {
  if (user_sx < 0 || user_sy < 0) return;
  if (user_sx < -tcU(20) || user_sx > FRAME_W + tcU(20) ||
      user_sy < -tcU(20) || user_sy > FRAME_H + tcU(20)) return;
  G->fillRect(user_sx - tcU(1), user_sy - tcU(1), tcU(3), tcU(3), C(TC_RED));
  int pr = tcU(4) + (pulse_phase % tcU(6));
  G->drawCircle(user_sx, user_sy, pr, C(TC_RED));
}

static void drawCompass() {
  // Prefer projected North Pole; fallback to top-of-screen N
  int nx, ny;
  if (project(90.0f, 0.0f, nx, ny)) {
    G->fillTriangle(nx, ny-4, nx-3, ny+1, nx+3, ny+1, C(TC_CYAN));
    G->setTextColor(C(TC_CYAN));
    G->setCursor(nx-3, ny+2);
    G->print("N");
  } else {
    int cx = FRAME_W / 2, cy = FRAME_OY + 4;
    G->fillTriangle(cx, cy-3, cx-3, cy+2, cx+3, cy+2, C(TC_CYAN));
    G->setTextColor(C(TC_CYAN));
    G->setCursor(cx-3, cy+4);
    G->print("N");
  }
}

static void drawLabels() {
  G->setTextSize(TC_FONT_SIZE);
  G->fillRect(typ_sx + tcU(8), typ_sy - tcU(14), tcU(3), tcU(3), C(catColor(STORMS[storm_sel].cat)));
  G->setTextColor(C(catColor(STORMS[storm_sel].cat)));
  G->setCursor(typ_sx + tcU(13), typ_sy - tcU(16));
  G->print(typ_name);
  if (user_sx >= 0 && user_sy >= 0
      && user_sx > -tcU(20) && user_sx < FRAME_W + tcU(20)
      && user_sy > -tcU(20) && user_sy < FRAME_H + tcU(20)) {
    G->setTextColor(C(TC_RED));
    G->setCursor(user_sx + tcU(5), user_sy - tcU(12));
    G->print(city_name);
  }
}

static void drawMoonIcon() {
  if (!night_mode || !G) return;
  int mx = VIEW_W / 2, my = tcU(18);
  G->fillCircle(mx, my, tcU(5), C(TC_YELLOW));
  G->fillCircle(mx + tcU(3), my - tcU(1), tcU(4), night_mode ? G->color565(28, 4, 4) : TC_OCEAN);
}

static int batteryPct() {
  int p = GetHAL().getBatteryLevel();
  if (p < 0) return 0;
  if (p > 100) return 100;
  // Some builds return 0 when USB-powered — show 100 if charging/unknown-zero on USB
  if (p == 0 && GetHAL().isBatteryCharging()) return 100;
  return p;
}

static void landfallLabel(char* out, size_t n) {
  int maxH = 0;
  for (int i = TRACK_NOW_IDX; i < (int)TRACK_N; i++)
    if (TRACK[i].hour > maxH) maxH = TRACK[i].hour;
  if (maxH > 0) snprintf(out, n, "~%dh", maxH);
  else snprintf(out, n, "---");
}

static const char* trendLabel() {
  if (TRACK_NOW_IDX > 0 && TRACK_NOW_IDX < TRACK_N) {
    int dw = (int)TRACK[TRACK_NOW_IDX].wind_kt - (int)TRACK[TRACK_NOW_IDX - 1].wind_kt;
    if (dw >= 5) return "INTENSIFY";
    if (dw <= -5) return "WEAKEN";
  }
  return "STEADY";
}

// --- Circular-safe overlay geometry (466 round AMOLED) ---
// LovyanGFX text is axis-aligned, so we keep glyphs inside the horizontal
// chord at each Y instead of rotating characters like ArcTopClock.
static constexpr float TC_CX = TC_SCREEN_W / 2.0f;
static constexpr float TC_CY = TC_SCREEN_H / 2.0f;
static constexpr float TC_SAFE_R = 218.0f;

static int circleHalfChordAtY(int y, int h = TC_CHAR_H) {
  float midY = (float)y + (float)h * 0.5f;
  float dy = midY - TC_CY;
  float r2 = TC_SAFE_R * TC_SAFE_R - dy * dy;
  if (r2 < 64.0f) return 8;
  return (int)sqrtf(r2);
}

static int circleSafeLeft(int y, int pad = 2, int h = TC_CHAR_H) {
  int v = (int)TC_CX - circleHalfChordAtY(y, h) + pad;
  return v < 0 ? 0 : v;
}

static int circleSafeRight(int y, int pad = 2, int h = TC_CHAR_H) {
  int v = (int)TC_CX + circleHalfChordAtY(y, h) - pad;
  return v > TC_SCREEN_W ? TC_SCREEN_W : v;
}

static int kbarH() { return tcU(12); }
static int kbarY() {
  // Lift above the bezel so the usable chord fits K1/K2 hints.
  return TC_SCREEN_H - kbarH() - tcU(8);
}

static void drawHUD() {
  float d = distKm(user_lat, user_lon, TYP_LAT, TYP_LON);
  char buf[24];
  G->setTextSize(TC_FONT_SIZE);
  // Drop below the clipped crown; x follows the chord at each row.
  const int y0 = tcU(22), y1 = y0 + tcU(11), y2 = y0 + tcU(20);
  const int x0 = circleSafeLeft(y0);
  const int x1 = x0 + tcU(22);
  G->setTextColor(TC_TEXT_DIM); G->setCursor(x0, y0); G->print("LAT");
  G->setTextColor(TC_CYAN);     G->setCursor(x1, y0);
  snprintf(buf, sizeof(buf), "%.1fN", user_lat); G->print(buf);
  G->setTextColor(TC_TEXT_DIM); G->setCursor(x0, y1); G->print("LON");
  G->setTextColor(TC_CYAN);     G->setCursor(x1, y1);
  snprintf(buf, sizeof(buf), "%.1fE", user_lon); G->print(buf);
  G->setTextColor(TC_TEXT_DIM); G->setCursor(x0, y2); G->print("SRC");
  if (wifiIsConnected()) {
    G->setTextColor(data_stale ? C(TC_YELLOW) : C(TC_GREEN));
    G->setCursor(x1, y2);
    G->print(data_stale ? "STALE" : "NMC");
  } else {
    static const char* SRC[] = {"WIFI","GPS","MAN"};
    G->setTextColor(loc_src == 0 ? C(TC_YELLOW) : C(TC_GREEN));
    G->setCursor(x1, y2);
    G->print(SRC[loc_src % 3]);
  }
  const int rw = tcU(60);
  int rx = circleSafeRight(y0) - rw;
  if (rx < TC_SCREEN_W / 2) rx = TC_SCREEN_W / 2;
  G->setTextColor(TC_TEXT_DIM); G->setCursor(rx, y0);            G->print("UTC");
  G->setTextColor(TC_YELLOW);   G->setCursor(rx + tcU(20), y0);  G->print("07-11");
  G->setTextColor(TC_TEXT_DIM); G->setCursor(rx, y1);            G->print("ACT");
  G->setTextColor(C(TC_RED));   G->setCursor(rx + tcU(20), y1);
  { char ab[8]; snprintf(ab, sizeof(ab), "%d/%d", storm_sel + 1, storm_count); G->print(ab); }
  G->setTextColor(TC_TEXT_DIM); G->setCursor(rx, y2);            G->print("TILT");
  G->setTextColor(gyro_tilt ? TC_CYAN : TC_TEXT_DIM);
  G->setCursor(rx + tcU(28), y2);
  if (gyro_tilt) {
    char tb[8]; snprintf(tb, sizeof(tb), "%+02.0f", tilt_pitch);
    G->print(tb);
  } else G->print("OFF");

  const int by = kbarY() - tcU(32);
  const int lx = circleSafeLeft(by);
  const int lx1 = lx + tcU(22);
  G->setTextColor(TC_TEXT_DIM); G->setCursor(lx, by);            G->print("NEAREST");
  G->setTextColor(C(TC_RED));   G->setCursor(lx + tcU(46), by);  G->print(typ_name);
  G->setTextColor(TC_TEXT_DIM); G->setCursor(lx, by + tcU(9));   G->print("DIST");
  snprintf(buf, sizeof(buf), "%dKM %s", (int)(d + 0.5f), bearingTo(user_lat, user_lon, TYP_LAT, TYP_LON));
  G->setTextColor(C(TC_RED));   G->setCursor(lx + tcU(26), by + tcU(9)); G->print(buf);
  G->setTextColor(TC_TEXT_DIM); G->setCursor(lx, by + tcU(18));  G->print("CAT");
  G->setTextColor(C(catColor(STORMS[storm_sel].cat)));
  G->setCursor(lx1, by + tcU(18)); G->print(catName(STORMS[storm_sel].cat));
  const int bw = tcU(48);
  int bx = circleSafeRight(by) - bw;
  if (bx < TC_SCREEN_W / 2) bx = TC_SCREEN_W / 2;
  G->setTextColor(TC_TEXT_DIM); G->setCursor(bx, by);                   G->print("BAT");
  G->setTextColor(C(TC_GREEN)); G->setCursor(bx + tcU(20), by);
  snprintf(buf, sizeof(buf), "%d%%", batteryPct()); G->print(buf);
  G->setTextColor(TC_TEXT_DIM); G->setCursor(bx, by + tcU(9));          G->print("IMU");
  G->setTextColor(gyro_tilt ? C(TC_GREEN) : TC_TEXT_DIM);
  G->setCursor(bx + tcU(20), by + tcU(9)); G->print(gyro_tilt ? "ON" : "OFF");
  G->setTextColor(TC_TEXT_DIM); G->setCursor(bx, by + tcU(18));         G->print("NIGHT");
  G->setTextColor(night_mode ? C(TC_RED) : TC_TEXT_DIM);
  G->setCursor(bx + tcU(32), by + tcU(18)); G->print(night_mode ? "ON" : "OFF");
}

static void drawDashedLine(int x0, int y0, int x1, int y1, uint16_t color) {
  int dx = x1 - x0, dy = y1 - y0;
  int len = (int)sqrtf((float)(dx * dx + dy * dy));
  if (len < 1) return;
  for (int s = 0; s < len; s += 4)
    if ((s / 4) % 2 == 0)
      G->drawPixel(x0 + dx * s / len, y0 + dy * s / len, color);
}

static void drawKbar(const char* left, const char* right) {
  LovyanGFX* dst = G ? G : (LovyanGFX*)disp_;
  const int kh = kbarH();
  const int ky = kbarY();
  const int half = circleHalfChordAtY(ky, kh);
  const int x0 = (int)TC_CX - half + 2;
  const int bw = half * 2 - 4;
  dst->setTextSize(TC_FONT_SIZE);
  dst->fillRect(x0, ky, bw, kh, TC_BLACK);
  if (hold_progress > 0) {
    int w = (bw * hold_progress) / 100;
    dst->fillRect(x0, ky, w, kh, dst->color565(20, 60, 70));
  }
  const int ty = ky + (kh - TC_CHAR_H) / 2;
  if (event_show_ms) {
    dst->setTextColor(event_color);
    int ew = (int)strlen(event_text) * TC_CHAR_W;
    int ex = (int)TC_CX - ew / 2;
    if (ex < x0 + 2) ex = x0 + 2;
    if (ex + ew > x0 + bw - 2) ex = x0 + 2;
    dst->setCursor(ex, ty);
    dst->print(event_text);
    return;
  }

  // Narrow bottom chord: "K1 BACK"+"K2 SEEK" was painting as "BACKSEEK".
  auto stripKey = [](const char* s) -> const char* {
    if (!s || !s[0]) return s;
    if ((s[0] == 'K' && s[1] == '1' && s[2] == ' ') ||
        (s[0] == 'K' && s[1] == '2' && s[2] == ' '))
      return s + 3;
    return s;
  };
  const char* L = left ? left : "";
  const char* R = right ? right : "";
  int lw = (int)strlen(L) * TC_CHAR_W;
  int rw = (int)strlen(R) * TC_CHAR_W;
  const int pad = 4;
  const int avail = bw - 2 * pad;
  if (R[0] && lw + rw + 8 > avail) {
    L = stripKey(L);
    R = stripKey(R);
    lw = (int)strlen(L) * TC_CHAR_W;
    rw = (int)strlen(R) * TC_CHAR_W;
  }
  dst->setTextColor(TC_CYAN);
  if (!R[0]) {
    int ex = (int)TC_CX - lw / 2;
    if (ex < x0 + pad) ex = x0 + pad;
    dst->setCursor(ex, ty);
    dst->print(L);
    return;
  }
  if (lw + rw + 8 > avail) {
    // Still tight — one centered "BACK · SEEK" group
    char buf[40];
    snprintf(buf, sizeof(buf), "%s · %s", L, R);
    int cw = (int)strlen(buf) * TC_CHAR_W;
    int ex = (int)TC_CX - cw / 2;
    if (ex < x0 + pad) ex = x0 + pad;
    dst->setCursor(ex, ty);
    dst->print(buf);
    return;
  }
  dst->setCursor(x0 + pad, ty);
  dst->print(L);
  dst->setCursor(x0 + bw - rw - pad, ty);
  dst->print(R);
}

// Present off-screen 240×135 buffer in one DMA-friendly push (no fillScreen flash)
static void presentView() { /* view_sprite is final buffer */ }

// Map frame → view_sprite (crop or tilt). Call with overlays already on frame_sprite.
static void blitFrameToView(bool tilt_active) {
  uint16_t bg = night_mode ? view_sprite->color565(28, 4, 4) : TC_OCEAN;
  view_sprite->fillScreen(bg);
  if (!frame_sprite) return;
  if (tilt_active) {
    float ang = tilt_roll * 0.35f;
    float dx = tilt_roll * 0.9f;
    float dy = -tilt_pitch * 0.9f;
    frame_sprite->pushRotateZoom(view_sprite,
      VIEW_W * 0.5f + dx, VIEW_H * 0.5f + dy, ang, 1.0f, 1.0f);
  } else {
    frame_sprite->pushSprite(view_sprite, -FRAME_OX, -FRAME_OY);
  }
}

static void showEvent(const char* text, uint16_t color) {
  strncpy(event_text, text, sizeof(event_text) - 1);
  event_text[sizeof(event_text) - 1] = 0;
  event_color = color;
  event_show_ms = GetHAL().millis();
  ui_dirty = true;
  ESP_LOGI("Typhoon", "[UI] %s", text);
}

static void tmEvalHour(float hour)
{
  if (TRACK_N == 0) return;
  float h0 = (float)TRACK[0].hour;
  float h1 = (float)TRACK[TRACK_N - 1].hour;
  if (hour < h0) hour = h0;
  if (hour > h1) hour = h1;
  tm_hour_f = hour;

  int i0 = 0;
  for (int i = 0; i < (int)TRACK_N - 1; i++) {
    if ((float)TRACK[i + 1].hour >= hour) { i0 = i; break; }
    i0 = i;
  }
  int i1 = i0 + 1;
  if (i1 >= (int)TRACK_N) i1 = TRACK_N - 1;
  const TrackPt& a = TRACK[i0];
  const TrackPt& b = TRACK[i1];
  float span = (float)(b.hour - a.hour);
  float u = (span > 0.01f) ? ((hour - (float)a.hour) / span) : 0.0f;
  if (u < 0) u = 0;
  if (u > 1) u = 1;
  float s = u * u * (3.0f - 2.0f * u);
  tm_lat = a.lat + (b.lat - a.lat) * s;
  tm_lon = a.lon + (b.lon - a.lon) * s;
  tm_r12 = a.r12_km + (b.r12_km - a.r12_km) * s;
  tm_wind = (uint8_t)(a.wind_kt + (b.wind_kt - a.wind_kt) * s + 0.5f);
  tm_pressure = (uint16_t)(a.pressure + (int)((b.pressure - a.pressure) * s + 0.5f));
  tm_cat = (s < 0.5f) ? a.cat : b.cat;
  track_sel = (int8_t)((s < 0.5f) ? i0 : i1);
  tm_pos = (float)i0 + u;

  if (page == PAGE_TRACK) {
    setMapCenter(tm_lat, tm_lon, false);
    tm_sx = earth_cx;
    tm_sy = earth_cy;
  } else if (!project(tm_lat, tm_lon, tm_sx, tm_sy)) {
    tm_sx = typ_sx;
    tm_sy = typ_sy;
  }
}

static void tmEval(float pos)
{
  if (TRACK_N == 0) return;
  if (pos < 0.0f) pos = 0.0f;
  if (pos > (float)(TRACK_N - 1)) pos = (float)(TRACK_N - 1);
  int i0 = (int)pos;
  int i1 = i0 + 1;
  if (i1 >= (int)TRACK_N) i1 = TRACK_N - 1;
  float u = pos - (float)i0;
  float hour = (float)TRACK[i0].hour + (float)(TRACK[i1].hour - TRACK[i0].hour) * u;
  tmEvalHour(hour);
}

static void tmStart(bool from_begin)
{
  if (TRACK_N < 2) {
    tm_playing = false;
    track_sel = TRACK_NOW_IDX;
    tmEvalHour((float)TRACK[track_sel].hour);
    return;
  }
  tm_dir = 1;
  tm_hold_until = 0;
  tm_last_ms = GetHAL().millis();
  tm_playing = true;
  float hour = from_begin ? (float)TRACK[0].hour : (float)TRACK[TRACK_NOW_IDX].hour;
  tmEvalHour(hour);
  showEvent("TIME MACHINE", TC_CYAN);
}

static void tmTick(uint32_t now)
{
  if (!tm_playing || TRACK_N < 2) return;
  float hFirst = (float)TRACK[0].hour;
  float hLast = (float)TRACK[TRACK_N - 1].hour;
  if (tm_hold_until) {
    if (now < tm_hold_until) {
      tm_last_ms = now;
      return;
    }
    tm_hold_until = 0;
    tm_dir = 1;
    tm_last_ms = now;
    tmEvalHour(hFirst);
    return;
  }
  if (tm_last_ms == 0) tm_last_ms = now;
  uint32_t dt = now - tm_last_ms;
  tm_last_ms = now;
  if (dt > 120) dt = 120;
  // Advance by real data-hour span (uneven gaps → uneven segment duration)
  float dh = (float)dt / (float)TM_MS_PER_DATA_HOUR;
  tm_hour_f += (float)tm_dir * dh;
  if (tm_hour_f >= hLast) {
    tm_hour_f = hLast;
    tmEvalHour(tm_hour_f);
    tm_hold_until = now + TM_END_HOLD_MS;
    return;
  }
  if (tm_hour_f <= hFirst) {
    tm_hour_f = hFirst;
    tm_dir = 1;
  }
  tmEvalHour(tm_hour_f);
}

static void gotoPage(Page p) {
  Page prev = page;
  page = p;
  hold_progress = 0;
  hold_btn = -1;
  event_show_ms = 0;
  if (p == PAGE_TRACK) {
    // Time Machine lives only under DETAIL → TRACK
    track_sel = TRACK_NOW_IDX;
    earth_r = -1;
    setMapZoom(EARTH_R_TRACK);
    tmStart(true);
    noteInput();  // keep screen awake while autoplay starts
  } else {
    tm_playing = false;
    if (prev == PAGE_TRACK) {
      // Leaving Time Machine — idle dim timer starts fresh from now
      noteInput();
    }
  }
  if (p == PAGE_LOCATION) {
    loc_cursor = city_idx;
    if (loc_cursor >= LOC_VISIBLE) loc_scroll = loc_cursor - LOC_VISIBLE + 1;
    else loc_scroll = 0;
  }
  if (p == PAGE_MULTI) {
    multi_sel = storm_sel;
    earth_r = -1; // force rebuild + reproject at MULTI scale
    setMapZoom(EARTH_R_MULTI);
  } else if (p == PAGE_MAIN) {
    // Restore original overview: observer-centered, StickS3 scale
    earth_r = -1;
    setMapZoom(EARTH_R_MAIN);
    setMapCenter(user_lat, user_lon, true);
  } else if (p == PAGE_DETAIL) {
    // Typhoon follow-cam + ~4-province FOV (not on MAIN)
    earth_r = -1;
    setMapZoom(EARTH_R_DETAIL);
    setMapCenter(TYP_LAT, TYP_LON, true);
  } else if (prev == PAGE_MULTI && p != PAGE_MULTI && p != PAGE_TRACK && p != PAGE_DETAIL) {
    earth_r = -1;
    setMapZoom(EARTH_R_MAIN);
    setMapCenter(user_lat, user_lon, true);
  }
  ui_dirty = true;
  const char* names[] = {"BOOT","MAIN","DETAIL","TRACK","MENU","MULTI","LOCATION","WIFI","ABOUT"};
  ESP_LOGI("Typhoon", "[NAV] -> %s", names[p]);
  beep(1400, 8);
}

static void renderBoot() {
  disp_->fillScreen(TC_BG_DEEP);
  int cx = TC_SCREEN_W / 2, cy = TC_SCREEN_H / 2 - 4;
  uint16_t rc = disp_->color565(30, 80, 90);
  disp_->drawCircle(cx, cy, 50, rc);
  disp_->drawCircle(cx, cy, 37, rc);
  disp_->drawCircle(cx, cy, 25, rc);
  disp_->drawCircle(cx, cy, 12, rc);
  disp_->drawFastHLine(cx - 52, cy, 104, rc);
  disp_->drawFastVLine(cx, cy - 52, 104, rc);
  disp_->setTextColor(TC_CYAN);
  disp_->setCursor(cx - 3, cy - 62); disp_->print("N");
  disp_->setCursor(cx - 3, cy + 54); disp_->print("S");
  disp_->setCursor(cx + 54, cy - 3); disp_->print("E");
  disp_->setCursor(cx - 62, cy - 3); disp_->print("W");
  for (int a = 0; a < 360; a += 22) {
    float rad = a * DEG2RAD;
    int x0 = cx + (int)(cosf(rad) * 50);
    int y0 = cy + (int)(sinf(rad) * 50);
    int x1 = cx + (int)(cosf(rad) * 53);
    int y1 = cy + (int)(sinf(rad) * 53);
    disp_->drawLine(x0, y0, x1, y1, (a % 90 == 0) ? TC_CYAN : rc);
  }
  for (int a = 0; a < 14; a++) {
    float rad = (boot_angle + a) * DEG2RAD;
    uint8_t bright = (uint8_t)(180 - a * 10);
    uint16_t c = disp_->color565(0, bright / 3, bright / 2);
    disp_->drawLine(cx, cy, cx + (int)(cosf(rad) * 50), cy + (int)(sinf(rad) * 50), c);
  }
  disp_->fillRect(cx - 1, cy - 1, 3, 3, TC_CYAN);
  disp_->setTextSize(TC_FONT_SIZE);
  const char* title = "TYPHOON COMPASS";
  const char* sub = "v1.0 MET RADAR";
  int titleY = tcU(28);
  disp_->setTextColor(TC_CYAN);
  disp_->setCursor(cx - (int)strlen(title) * TC_CHAR_W / 2, titleY);
  disp_->print(title);
  disp_->setTextColor(TC_TEXT_DIM);
  disp_->setCursor(cx - (int)strlen(sub) * TC_CHAR_W / 2, titleY + TC_CHAR_H + 2);
  disp_->print(sub);
  int sy = cy + tcU(58);
  auto bootLine = [&](int y, const char* t, uint16_t col) {
    disp_->setTextColor(col);
    int x = circleSafeLeft(y);
    int maxW = circleSafeRight(y) - x;
    int tw = (int)strlen(t) * TC_CHAR_W;
    if (tw > maxW) x = circleSafeLeft(y); // keep left-safe; clip long tails naturally
    disp_->setCursor(x, y);
    disp_->print(t);
  };
  bootLine(sy, "INIT SENSORS...", TC_TEXT_DIM);
  bootLine(sy + TC_CHAR_H + 2, ">WIFI READY", TC_GREEN);
  bootLine(sy + 2 * (TC_CHAR_H + 2),
           boot_progress < 85 ? ">FETCH NMC DATA..." : ">FETCH NMC DATA OK",
           boot_progress < 85 ? TC_CYAN : TC_GREEN);
  bootLine(sy + 3 * (TC_CHAR_H + 2),
           boot_progress < 95 ? " IMU CALIBRATION" : " IMU READY",
           boot_progress < 95 ? TC_TEXT_DIM : TC_GREEN);
  int bw = tcU(120), bx = (TC_SCREEN_W - bw) / 2, by = cy - tcU(70);
  disp_->drawRect(bx, by, bw, 6, TC_CYAN);
  disp_->fillRect(bx + 1, by + 1, (bw - 2) * boot_progress / 100, 4, TC_CYAN);
  char pct[8]; snprintf(pct, sizeof(pct), "%d%%", boot_progress);
  disp_->setTextColor(TC_CYAN);
  disp_->setCursor(cx - (int)strlen(pct) * TC_CHAR_W / 2, by - TC_CHAR_H - 2);
  disp_->print(pct);
  G = nullptr;
  drawKbar("K1+K2 SKIP", "");
}

// StickS3 + Lcd.setRotation(1): screen landscape, USB on the right (user photo)
// Accel (G) → device pitch/roll before calibration offset
static void imuAccelPR(float ax, float ay, float az, float& pitch, float& roll) {
  // Tip device top toward/away user ≈ pitch; tip left/right ≈ roll
  pitch = atan2f(-ay, sqrtf(ax * ax + az * az)) * RAD2DEG;
  roll  = atan2f( ax, az) * RAD2DEG;
}

static float imuWrap180(float a) {
  while (a > 180.0f) a -= 360.0f;
  while (a < -180.0f) a += 360.0f;
  return a;
}

static void imuCaptureCalibration() {
  float ax, ay, az;
  if (!tcImuGetAccel(&ax, &ay, &az)) return;
  float p, r;
  imuAccelPR(ax, ay, az, p, r);
  cal_pitch = p;
  cal_roll = r;
  cal_ready = true;
  imu_pitch = imu_roll = 0;
  tilt_pitch = tilt_roll = 0;
  imu_still_ms = GetHAL().millis();
  ESP_LOGI("Typhoon", "[IMU] cal P=%.1f R=%.1f (ax=%.2f ay=%.2f az=%.2f)",
                cal_pitch, cal_roll, ax, ay, az);
}

static void imuResetTilt() {
  imuCaptureCalibration();
}

static void imuPoll() {
  uint32_t now = GetHAL().millis();
  if (imu_last_ms == 0) imu_last_ms = now;
  float dt = (now - imu_last_ms) / 1000.0f;
  if (dt < 0.025f) return; // ~40Hz max
  if (dt > 0.1f) dt = 0.1f;
  imu_last_ms = now;

  if (!gyro_tilt) {
    tilt_pitch *= 0.8f;
    tilt_roll *= 0.8f;
    imu_pitch *= 0.8f;
    imu_roll *= 0.8f;
    return;
  }

  float ax, ay, az, gx, gy, gz;
  if (!tcImuGetAccel(&ax, &ay, &az)) { imu_ok = false; return; }
  if (!tcImuGetGyro(&gx, &gy, &gz)) { imu_ok = false; return; }
  imu_ok = true;

  // First samples → capture neutral (hand-held landscape as in photo)
  if (!cal_ready) {
    imuCaptureCalibration();
    return;
  }

  float raw_p, raw_r;
  imuAccelPR(ax, ay, az, raw_p, raw_r);
  // Delta from calibrated hold pose
  float accel_pitch = imuWrap180(raw_p - cal_pitch);
  float accel_roll  = imuWrap180(raw_r - cal_roll);

  // Gyro axes matched to same convention (deg/s)
  float g_pitch = -gx;
  float g_roll  =  gy;

  imu_pitch = 0.93f * (imu_pitch + g_pitch * dt) + 0.07f * accel_pitch;
  imu_roll  = 0.93f * (imu_roll  + g_roll  * dt) + 0.07f * accel_roll;

  if (imu_pitch > TILT_LIM) imu_pitch = TILT_LIM;
  if (imu_pitch < -TILT_LIM) imu_pitch = -TILT_LIM;
  if (imu_roll  > TILT_LIM) imu_roll  = TILT_LIM;
  if (imu_roll  < -TILT_LIM) imu_roll  = -TILT_LIM;

  tilt_pitch = tilt_pitch * 0.85f + imu_pitch * 0.15f;
  tilt_roll  = tilt_roll  * 0.85f + imu_roll  * 0.15f;

  // Still near neutral → ease to flat after 1s
  float motion = fabsf(gx) + fabsf(gy) + fabsf(gz);
  bool near_flat = fabsf(accel_pitch) < 2.5f && fabsf(accel_roll) < 2.5f;
  if (motion < 5.0f && near_flat) {
    if (now - imu_still_ms > 1000) {
      tilt_pitch *= 0.86f;
      tilt_roll  *= 0.86f;
      imu_pitch  *= 0.86f;
      imu_roll   *= 0.86f;
      if (fabsf(tilt_pitch) < 0.2f) tilt_pitch = 0;
      if (fabsf(tilt_roll)  < 0.2f) tilt_roll  = 0;
    }
  } else {
    imu_still_ms = now;
  }
}

static void pushMapCrop(LovyanGFX* dst) {
  // Show center VIEW_W×VIEW_H of oversized map
  bg_sprite->pushSprite(dst, -FRAME_OX, -FRAME_OY);
}

static void renderMainBase() {
  bool want_tilt = gyro_tilt && frame_sprite && view_sprite && (page == PAGE_MAIN || page == PAGE_MULTI);
  bool active = want_tilt && (fabsf(tilt_pitch) > 0.35f || fabsf(tilt_roll) > 0.35f);

  // Compose map layers on oversized frame (coords = frame space)
  G = frame_sprite ? (LovyanGFX*)frame_sprite : (LovyanGFX*)disp_;
  if (frame_sprite) {
    bg_sprite->pushSprite(frame_sprite, 0, 0);
  } else {
    pushMapCrop(disp_);
  }
  drawTracks();
  drawWindRings();
  drawTyphoon();
  drawUserMarker();
  drawCompass();
  drawLabels();

  // Off-screen present buffer: one push to LCD → no fillScreen flicker
  if (view_sprite && frame_sprite) {
    blitFrameToView(active);
    G = view_sprite;
    drawHUD();
    drawMoonIcon();
    if (want_tilt) {
      char pr[20];
      snprintf(pr, sizeof(pr), "P%+05.1f R%+05.1f", tilt_pitch, tilt_roll);
      G->setTextColor(C(TC_YELLOW));
      int ty = tcU(24);
      int tw = (int)strlen(pr) * TC_CHAR_W;
      G->setCursor(circleSafeRight(ty) - tw, ty);
      G->print(pr);
    }
    drawKbar("K1 MENU", "K2 DETAIL");
    presentView();
    disp_ = view_sprite;
  G = view_sprite;
    return;
  }

  // Fallback without view buffer
  disp_ = view_sprite;
  G = view_sprite;
  if (frame_sprite) {
    if (active) {
      float ang = tilt_roll * 0.35f;
      float dx = tilt_roll * 0.9f;
      float dy = -tilt_pitch * 0.9f;
      frame_sprite->pushRotateZoom(VIEW_W * 0.5f + dx, VIEW_H * 0.5f + dy, ang, 1.0f, 1.0f);
    } else {
      frame_sprite->pushSprite(disp_, -FRAME_OX, -FRAME_OY);
    }
  }
  drawHUD();
  drawMoonIcon();
  if (want_tilt) {
    char pr[20];
    snprintf(pr, sizeof(pr), "P%+05.1f R%+05.1f", tilt_pitch, tilt_roll);
    disp_->setTextColor(C(TC_YELLOW));
    int ty = tcU(24);
    int tw = (int)strlen(pr) * TC_CHAR_W;
    disp_->setCursor(circleSafeRight(ty) - tw, ty);
    disp_->print(pr);
  }
}

static void renderDetail() {
  // Compose without full-screen dim flash: earth → panel → light right-side dim
  disp_->startWrite();
  if (frame_sprite) {
    bg_sprite->pushSprite(frame_sprite, 0, 0);
    G = frame_sprite;
    drawTracks();
    drawWindRings();
    drawTyphoon();
    drawUserMarker();
    disp_ = view_sprite;
  G = view_sprite;
    frame_sprite->pushSprite(disp_, -FRAME_OX, -FRAME_OY);
  } else {
    disp_ = view_sprite;
  G = view_sprite;
    pushMapCrop(disp_);
  }

  disp_->setTextSize(TC_FONT_SIZE);
  const int pw = tcU(110);
  const int top = tcU(36);
  const int bot = kbarY() - tcU(4);
  // Opaque panel covers left mid-band — corners clipped by round bezel
  disp_->fillRect(0, top, pw, bot - top, TC_PANEL_BG);
  disp_->drawFastVLine(pw, top, bot - top, TC_CYAN);
  disp_->fillRect(0, top, pw, tcU(14), disp_->color565(10, 40, 50));
  disp_->setTextColor(C(TC_RED));
  {
    int ty = top + tcU(2);
    disp_->setCursor(circleSafeLeft(ty), ty);
    char t[20]; snprintf(t, sizeof(t), "%s", typ_name); disp_->print(t);
  }

  auto row = [&](int y, const char* lab, const char* val, uint16_t vc) {
    int lx = circleSafeLeft(y);
    if (lx < tcU(3)) lx = tcU(3);
    if (lx > pw - tcU(60)) lx = pw - tcU(60);
    disp_->setTextColor(TC_TEXT_DIM);
    disp_->setCursor(lx, y); disp_->print(lab);
    disp_->setTextColor(vc);
    disp_->setCursor(lx + tcU(45), y); disp_->print(val);
  };

  char buf[28];
  const Storm& st = STORMS[storm_sel];
  float d = distKm(user_lat, user_lon, TYP_LAT, TYP_LON);
  const char* br = bearingTo(user_lat, user_lon, TYP_LAT, TYP_LON);

  {
    int cy = top + tcU(16);
    int lx = circleSafeLeft(cy);
    if (lx < tcU(3)) lx = tcU(3);
    disp_->setTextColor(TC_TEXT_DIM); disp_->setCursor(lx, cy); disp_->print("CAT");
    disp_->setTextColor(C(catColor(st.cat))); disp_->setCursor(lx + tcU(25), cy); disp_->print(catName(st.cat));
    int bars = (int)st.cat + 1; if (bars > 5) bars = 5;
    for (int i = 0; i < bars; i++)
      disp_->fillRect(lx + tcU(55) + i * tcU(7), cy + 1, tcU(5), tcU(5), C(TC_RED));
  }

  snprintf(buf, sizeof(buf), "%.1fN%.1fE", st.lat, st.lon);
  row(top + tcU(28), "POS",  buf, C(TC_YELLOW));
  snprintf(buf, sizeof(buf), "%dhPa", st.pressure);
  row(top + tcU(40), "PRES", buf, C(TC_TEXT));
  snprintf(buf, sizeof(buf), "%dkt", st.wind_kt);
  row(top + tcU(52), "WIND", buf, C(TC_RED));
  snprintf(buf, sizeof(buf), "%dkt", st.wind_kt + 20);
  row(top + tcU(64), "GUST", buf, C(TC_RED));
  row(top + tcU(76), "MOVE", "12kt WNW", C(TC_TEXT));
  snprintf(buf, sizeof(buf), "%dkm", (int)st.r7);
  row(top + tcU(88), "R7",   buf, C(TC_YELLOW));
  snprintf(buf, sizeof(buf), "%dkm", (int)st.r10);
  row(top + tcU(100), "R10",  buf, C(TC_ORANGE));
  snprintf(buf, sizeof(buf), "%dkm", (int)st.r12);
  row(top + tcU(112), "R12",  buf, C(TC_RED));
  snprintf(buf, sizeof(buf), "%dKM %s", (int)(d + 0.5f), br);
  row(top + tcU(124), "DIST", buf, C(TC_YELLOW));
  { char lb[12]; landfallLabel(lb, sizeof(lb)); row(top + tcU(136), "LAND", lb, C(TC_CYAN)); }
  row(top + tcU(148), "TREND", trendLabel(), C(TC_RED));

  // Light dim on visible earth only (right of panel)
  for (int y = top; y < bot; y += 3)
    disp_->drawFastHLine(pw + 1, y, TC_SCREEN_W - pw - 1, TC_BLACK);

  const int rbh = tcU(52), rbw = tcU(60);
  int ly = top + tcU(4);
  int lx = circleSafeRight(ly, 2, rbh) - rbw;
  disp_->fillRect(lx, ly, rbw, rbh, TC_BLACK);
  disp_->drawRect(lx, ly, rbw, rbh, TC_CYAN);
  disp_->setTextColor(TC_CYAN);
  disp_->setCursor(lx + tcU(2), ly + tcU(2)); disp_->print("RINGS");
  disp_->setTextColor(TC_YELLOW);
  disp_->setCursor(lx + tcU(2), ly + tcU(14)); disp_->print("7  280");
  disp_->setTextColor(TC_ORANGE);
  disp_->setCursor(lx + tcU(2), ly + tcU(26)); disp_->print("10 160");
  disp_->setTextColor(TC_RED);
  disp_->setCursor(lx + tcU(2), ly + tcU(38)); disp_->print("12 80");

  static const char* RM[] = {"ALL","R7","R10","R12","OFF"};
  disp_->setTextColor(TC_CYAN);
  disp_->setCursor(lx + tcU(2), ly + rbh + 2);
  disp_->print(RM[ring_mode % 5]);

  drawKbar("K1 BACK", "K2 TIME");
  disp_->endWrite();
}

static void renderTrack() {
  if (TRACK_N > 0) tmEvalHour(tm_hour_f);

  disp_->startWrite();
  if (frame_sprite) {
    bg_sprite->pushSprite(frame_sprite, 0, 0);
    G = frame_sprite;
    drawTracks();
    // StickS3-style rings around playhead (approx R7/R10/R12)
    int r12 = kmToPx(tm_r12);
    if (r12 < tcU(8)) r12 = tcU(8);
    dashedCircle(tm_sx, tm_sy, (int)(r12 * 2.4f), TC_YELLOW, tcU(10), 4);
    dashedCircle(tm_sx, tm_sy, (int)(r12 * 1.5f), TC_ORANGE, tcU(8), 4);
    dashedCircle(tm_sx, tm_sy, r12, TC_RED, tcU(8), 4);
    G->fillCircle(tm_sx, tm_sy, tcU(3), TC_YELLOW);
    G->fillRect(tm_sx - tcU(1), tm_sy - tcU(1), tcU(3), tcU(3), TC_WHITE);
    G->setTextSize(TC_FONT_SIZE);
    G->setTextColor(C(TC_RED));
    G->setCursor(tm_sx + tcU(8), tm_sy - tcU(12));
    G->print(typ_name);
    drawUserMarker();
    disp_ = view_sprite;
    G = view_sprite;
    frame_sprite->pushSprite(disp_, -FRAME_OX, -FRAME_OY);
  } else {
    disp_ = view_sprite;
    G = view_sprite;
    pushMapCrop(disp_);
  }

  disp_->setTextSize(TC_FONT_SIZE);
  int hour_i = (int)(tm_hour_f + (tm_hour_f >= 0 ? 0.5f : -0.5f));

  // Top-rim status (ArcTopClock-style): chord text only — no opaque card
  // covering the centered typhoon / wind rings.
  char title[20], line[28];
  if (hour_i == 0) snprintf(title, sizeof(title), "NOW");
  else if (hour_i > 0) snprintf(title, sizeof(title), "+%dH FCST", hour_i);
  else snprintf(title, sizeof(title), "%dH PAST", hour_i);

  float d = distKm(user_lat, user_lon, tm_lat, tm_lon);
  const char* br = bearingTo(user_lat, user_lon, tm_lat, tm_lon);

  auto rimCenter = [&](int y, const char* t, uint16_t col) {
    int tw = (int)strlen(t) * TC_CHAR_W;
    int x0 = circleSafeLeft(y);
    int x1 = circleSafeRight(y);
    int x = (int)TC_CX - tw / 2;
    if (x < x0) x = x0;
    if (x + tw > x1) x = x1 - tw;
    disp_->setTextColor(col);
    disp_->setCursor(x, y);
    disp_->print(t);
  };

  const int yTitle = tcU(26);
  const int yPos   = yTitle + TC_CHAR_H + 2;
  const int yMeta  = yPos + TC_CHAR_H + 2;
  rimCenter(yTitle, title, TC_YELLOW);

  snprintf(line, sizeof(line), "%.1fN %.1fE  CAT%d/%dkt",
           tm_lat, tm_lon, tm_cat, tm_wind);
  rimCenter(yPos, line, TC_TEXT);

  snprintf(line, sizeof(line), "%dhPa  %d%s", tm_pressure, (int)(d + 0.5f), br);
  rimCenter(yMeta, line, TC_TEXT_DIM);

  {
    int ly = yTitle;
    disp_->setTextColor(TC_CYAN);
    disp_->setCursor(circleSafeLeft(ly), ly);
    disp_->print("LOCK");
  }

  // Timeline — chord-limited; adaptive label step + collision skip
  float hMin = 0, hMax = 0;
  for (uint8_t i = 0; i < TRACK_N; i++) {
    float h = (float)TRACK[i].hour;
    if (h < hMin) hMin = h;
    if (h > hMax) hMax = h;
  }
  if (hMax <= hMin) { hMin = -24; hMax = 24; }

  int th = tcU(10);
  int ty = kbarY() - tcU(20);
  int half = circleHalfChordAtY(ty, th + TC_CHAR_H);
  int tx = (int)TC_CX - half + 4;
  int tw = half * 2 - 8;
  auto hx = [&](float hour) -> int {
    float t = (hour - hMin) / (hMax - hMin);
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    return tx + 1 + (int)(t * (tw - 2));
  };

  // Tick marks every 24h; labels only at a step that fits the chord.
  const int minLabelGap = 3 * TC_CHAR_W + 8; // ~"0"/"24" + pad
  float span = hMax - hMin;
  int labelStep = 24;
  while (labelStep < 240) {
    int n = (int)(span / (float)labelStep) + 1;
    if (n <= 1) break;
    if (tw / n >= minLabelGap) break;
    labelStep += 24;
  }

  disp_->fillRect(tx, ty, tw, th, TC_BLACK);
  disp_->drawRect(tx, ty, tw, th, TC_CYAN_DIM);
  int nowx = hx(0.0f);
  if (nowx > tx + 1)
    disp_->fillRect(tx + 1, ty + 1, nowx - tx - 1, th - 2, disp_->color565(40, 48, 56));
  if (tx + tw - 1 > nowx)
    disp_->fillRect(nowx, ty + 1, tx + tw - 1 - nowx, th - 2, disp_->color565(90, 48, 0));
  disp_->drawFastVLine(nowx, ty - 1, th + 2, TC_WHITE);
  int selx = hx(tm_hour_f);
  disp_->fillRect(selx - 1, ty - 2, tcU(3), th + 4, TC_YELLOW);

  // Minor ticks (no text)
  int tick0 = ((int)hMin / 24) * 24;
  if (tick0 > (int)hMin) tick0 -= 24;
  for (int h = tick0; h <= (int)hMax + 1; h += 24) {
    if (h < (int)hMin - 1 || h > (int)hMax + 1) continue;
    disp_->drawFastVLine(hx((float)h), ty - 2, 3, TC_CYAN_DIM);
  }

  // Labels: place high-priority hours first (ends + NOW), then majors; skip overlaps.
  int cand[16];
  int nCand = 0;
  auto pushCand = [&](int h) {
    if (h < (int)hMin - 1 || h > (int)hMax + 1) return;
    for (int i = 0; i < nCand; i++) if (cand[i] == h) return;
    if (nCand < 16) cand[nCand++] = h;
  };
  pushCand((int)lroundf(hMin));
  pushCand((int)lroundf(hMax));
  if (hMin < -0.5f && hMax > 0.5f) pushCand(0);
  int major0 = ((int)hMin / labelStep) * labelStep;
  if (major0 < (int)hMin) major0 += labelStep;
  for (int h = major0; h <= (int)hMax && nCand < 16; h += labelStep)
    pushCand(h);

  // Occupied ranges (up to 8 labels)
  int occL[8], occR[8], nOcc = 0;
  for (int i = 0; i < nCand && nOcc < 8; i++) {
    int h = cand[i];
    char lb[8];
    snprintf(lb, sizeof(lb), "%d", h);
    int lw = (int)strlen(lb) * TC_CHAR_W;
    int lx = hx((float)h) - lw / 2;
    if (lx < tx) lx = tx;
    if (lx + lw > tx + tw) lx = tx + tw - lw;
    bool hit = false;
    for (int j = 0; j < nOcc; j++) {
      if (lx < occR[j] + 4 && lx + lw > occL[j] - 4) { hit = true; break; }
    }
    if (hit) continue;
    disp_->setTextColor(h == 0 ? TC_WHITE : TC_TEXT_DIM);
    disp_->setCursor(lx, ty - TC_CHAR_H - 1);
    disp_->print(lb);
    occL[nOcc] = lx;
    occR[nOcc] = lx + lw;
    nOcc++;
  }

  drawKbar("K1 BACK", "K2 SEEK");
  disp_->endWrite();
}

static const char* MENU_LABELS[] = {
  "LOCATION", "DISPLAY", "GYRO TILT",
  "NIGHT MODE", "ALERT RADIUS", "UNITS", "ABOUT", "EXIT"
};
#define MENU_N 8

static void renderMenu() {
  disp_->startWrite();
  disp_->setTextSize(TC_FONT_SIZE);
  pushMapCrop(disp_);
  for (int y = 0; y < TC_SCREEN_H; y += 2)
    disp_->drawFastHLine(0, y, TC_SCREEN_W, TC_BLACK);

  const char* hdr = "TYPHOON COMPASS v1.0";
  int titleY = tcU(24);
  int half = circleHalfChordAtY(titleY, tcU(14));
  int hx0 = (int)TC_CX - half;
  disp_->fillRect(hx0, titleY - 2, half * 2, tcU(14), TC_BLACK);
  disp_->setTextColor(TC_CYAN);
  disp_->setCursor((int)TC_CX - (int)strlen(hdr) * TC_CHAR_W / 2, titleY);
  disp_->print(hdr);

  int pw = tcU(180), ph = tcU(140);
  int px = (TC_SCREEN_W - pw) / 2, py = tcU(48);
  disp_->fillRect(px, py, pw, ph, TC_PANEL_BG);
  disp_->drawRect(px, py, pw, ph, TC_CYAN);
  disp_->fillRect(px, py, pw, tcU(14), disp_->color565(10, 40, 50));
  disp_->setTextColor(TC_CYAN);
  const char* mh = "MAIN MENU";
  disp_->setCursor(px + (pw - (int)strlen(mh) * TC_CHAR_W) / 2, py + tcU(2));
  disp_->print(mh);

  char val[16];
  const int rowH = tcU(14);
  for (int i = 0; i < MENU_N; i++) {
    int iy = py + tcU(16) + i * rowH;
    bool sel = (i == menu_cursor);
    if (sel) disp_->fillRect(px + 1, iy - 1, pw - 2, rowH, TC_CYAN);
    disp_->setTextColor(sel ? TC_BLACK : TC_TEXT);
    disp_->setCursor(px + tcU(4), iy);
    disp_->print(sel ? ">" : " ");
    disp_->print(MENU_LABELS[i]);
    val[0] = 0;
    if (i == 3) snprintf(val, sizeof(val), gyro_tilt ? "[ON]" : "[OFF]");
    else if (i == 4) snprintf(val, sizeof(val), night_mode ? "[ON]" : "[OFF]");
    else if (i == 2) snprintf(val, sizeof(val), "[%d%%]", brightness);
    else if (i == 5) snprintf(val, sizeof(val), "[%d]", alert_radius_km);
    else if (i == 6) snprintf(val, sizeof(val), units_kt ? (units_km ? "[KT/KM]" : "[KT/NM]") : (units_km ? "[MS/KM]" : "[MS/NM]"));
    if (val[0]) {
      disp_->setCursor(px + pw - tcU(6) - (int)strlen(val) * TC_CHAR_W, iy);
      disp_->print(val);
    }
  }
  drawKbar("K1 UP", "K2 OK");
  disp_->endWrite();
}

static void renderLocation() {
  disp_->startWrite();
  disp_->setTextSize(TC_FONT_SIZE);
  disp_->fillScreen(TC_BG_DEEP);
  const int pw = tcU(130);
  const int top = tcU(36);
  const int bot = kbarY() - tcU(4);
  const int rowH = tcU(14);
  disp_->fillRect(0, top, pw, bot - top, TC_PANEL_BG);
  disp_->drawFastVLine(pw, top, bot - top, TC_CYAN);
  disp_->fillRect(0, top, pw, tcU(14), disp_->color565(10, 40, 50));
  {
    int hy = top + tcU(2);
    disp_->setTextColor(TC_CYAN);
    disp_->setCursor(circleSafeLeft(hy), hy);
    disp_->print("PRESET CITIES");
  }

  // Keep scroll window covering cursor
  if (loc_cursor < loc_scroll) loc_scroll = loc_cursor;
  if (loc_cursor >= loc_scroll + LOC_VISIBLE) loc_scroll = loc_cursor - LOC_VISIBLE + 1;

  for (int row = 0; row < LOC_VISIBLE; row++) {
    uint8_t i = loc_scroll + row;
    if (i >= CITY_N) break;
    int iy = top + tcU(16) + row * rowH;
    if (iy + rowH > bot) break;
    bool sel = (i == loc_cursor);
    int lx = circleSafeLeft(iy);
    if (lx < 2) lx = 2;
    if (lx > pw - tcU(40)) lx = 2;
    if (sel) disp_->fillRect(1, iy - 1, pw - 2, rowH, TC_CYAN);
    disp_->setTextColor(sel ? TC_BLACK : TC_TEXT);
    disp_->setCursor(lx, iy);
    disp_->print(sel ? ">" : " ");
    disp_->print(CITIES[i].name);
    char latb[8];
    snprintf(latb, sizeof(latb), "%.0fN", CITIES[i].lat);
    int latX = pw - tcU(32);
    if (latX < lx + tcU(60)) latX = lx + tcU(60);
    disp_->setCursor(latX, iy);
    disp_->print(latb);
  }

  // Source panel — right of list, chord-safe
  int sx = pw + tcU(6), sy = top;
  int sw = circleSafeRight(sy, 2, tcU(52)) - sx;
  if (sw < tcU(90)) sw = tcU(90);
  disp_->fillRect(sx, sy, sw, tcU(52), TC_BLACK);
  disp_->drawRect(sx, sy, sw, tcU(52), TC_CYAN);
  disp_->setTextColor(TC_CYAN);
  disp_->setCursor(sx + tcU(4), sy + tcU(2)); disp_->print("SOURCE");
  static const char* SN[] = {"WIFI/IP", "GPS", "MANUAL"};
  for (int i = 0; i < 3; i++) {
    disp_->setTextColor(i == loc_src ? TC_CYAN : TC_TEXT_DIM);
    disp_->setCursor(sx + tcU(4), sy + tcU(16) + i * tcU(12));
    disp_->print(i == loc_src ? "(*) " : "( ) ");
    disp_->print(SN[i]);
  }

  // Mini regional preview — zoomed map with city label
  int box_x = sx, box_y = sy + tcU(58);
  int box_w = sw, box_h = tcU(110);
  if (box_y + box_h > bot - tcU(36))
    box_h = bot - tcU(36) - box_y;
  if (box_h < tcU(60)) box_h = tcU(60);
  disp_->fillRect(box_x, box_y, box_w, box_h, TC_OCEAN);
  disp_->drawRect(box_x, box_y, box_w, box_h, TC_CYAN);
  int mcx = box_x + box_w / 2, mcy = box_y + box_h / 2 + tcU(2);
  const float zoomR = (float)tcU(110);
  {
    float clat = CITIES[loc_cursor].lat * DEG2RAD;
    float clon = CITIES[loc_cursor].lon * DEG2RAD;
    float sin_cLat = sinf(clat), cos_cLat = cosf(clat);
    float sin_cLon = sinf(clon), cos_cLon = cosf(clon);

    auto miniProj = [&](float lat, float lon, int& ox, int& oy) -> bool {
      float latR = lat * DEG2RAD, lonR = lon * DEG2RAD;
      float cos_c = sin_cLat * sinf(latR) + cos_cLat * cosf(latR) * cosf(lonR - clon);
      if (cos_c < 0.2f) return false;
      float x = zoomR * cosf(latR) * sinf(lonR - clon);
      float y = zoomR * (cos_cLat * sinf(latR) - sin_cLat * cosf(latR) * cosf(lonR - clon));
      ox = mcx + (int)x;
      oy = mcy - (int)y;
      return (ox > box_x && ox < box_x + box_w - 1 && oy > box_y && oy < box_y + box_h - 1);
    };

    // Coastline snippets (subsampled for speed)
    for (int i = 0; i < world_map_count; i++) {
      const MapPoint* pts = world_map[i].points;
      int n = world_map[i].length;
      int prevX = -1, prevY = -1; bool pv = false;
      for (int j = 0; j < n; j += 2) {
        float sin_lat = pts[j].sinLat, cos_lat = pts[j].cosLat;
        float sin_lon = pts[j].sinLon, cos_lon = pts[j].cosLon;
        float cos_dLon = cos_lon * cos_cLon + sin_lon * sin_cLon;
        float sin_dLon = sin_lon * cos_cLon - cos_lon * sin_cLon;
        float cos_c = sin_cLat * sin_lat + cos_cLat * cos_lat * cos_dLon;
        if (cos_c < 0.25f) { pv = false; continue; }
        float x = zoomR * cos_lat * sin_dLon;
        float y = zoomR * (cos_cLat * sin_lat - sin_cLat * cos_lat * cos_dLon);
        int ox = mcx + (int)x, oy = mcy - (int)y;
        if (ox <= box_x || ox >= box_x + box_w - 1 || oy <= box_y || oy >= box_y + box_h - 1) {
          pv = false; continue;
        }
        if (pv && abs(ox - prevX) < tcU(40) && abs(oy - prevY) < tcU(40))
          disp_->drawLine(prevX, prevY, ox, oy, disp_->color565(50, 160, 90));
        prevX = ox; prevY = oy; pv = true;
      }
    }

    int ux, uy, tx, ty;
    const char* cname = CITIES[loc_cursor].name;
    if (miniProj(CITIES[loc_cursor].lat, CITIES[loc_cursor].lon, ux, uy)) {
      disp_->fillRect(ux - tcU(2), uy - tcU(2), tcU(5), tcU(5), TC_RED);
      // City name beside the marker (flip side if near right edge)
      int nameW = (int)strlen(cname) * TC_CHAR_W;
      int nx = ux + tcU(6);
      int ny = uy - TC_CHAR_H / 2;
      if (nx + nameW > box_x + box_w - 2) nx = ux - tcU(6) - nameW;
      if (nx < box_x + 2) nx = box_x + 2;
      if (ny < box_y + 2) ny = box_y + 2;
      if (ny + TC_CHAR_H > box_y + box_h - 2) ny = box_y + box_h - 2 - TC_CHAR_H;
      disp_->setTextColor(TC_RED);
      disp_->setCursor(nx, ny);
      disp_->print(cname);
    }
    if (miniProj(TYP_LAT, TYP_LON, tx, ty)) {
      disp_->fillRect(tx - tcU(1), ty - tcU(1), tcU(3), tcU(3), TC_YELLOW);
      int nameW = (int)strlen(typ_name) * TC_CHAR_W;
      int nx = tx + tcU(5);
      int ny = ty - TC_CHAR_H / 2;
      if (nx + nameW > box_x + box_w - 2) nx = tx - tcU(5) - nameW;
      if (nx < box_x + 2) nx = box_x + 2;
      if (ny < box_y + 2) ny = box_y + 2;
      if (ny + TC_CHAR_H > box_y + box_h - 2) ny = box_y + box_h - 2 - TC_CHAR_H;
      disp_->setTextColor(TC_YELLOW);
      disp_->setCursor(nx, ny);
      disp_->print(typ_name);
    }
  }

  // Status under mini-map
  int statusY = box_y + box_h + tcU(4);
  disp_->setTextColor(TC_TEXT);
  disp_->setCursor(sx + tcU(2), statusY);
  disp_->print(CITIES[loc_cursor].name);
  if (loc_src == 0) {
    disp_->setTextColor(tcWifiConnected() ? TC_GREEN : TC_YELLOW);
    disp_->setCursor(sx + tcU(2), statusY + tcU(14));
    if (geo_busy) disp_->print("LOCATING");
    else if (!tcWifiConnected()) disp_->print("WIFI OFF");
    else if (loc_have_saved)
      disp_->print("SAVED K2USE");
    else
      disp_->print("K2:IP LOC");
  } else if (loc_src == 1) {
    disp_->setTextColor(TC_TEXT_DIM);
    disp_->setCursor(sx + tcU(2), statusY + tcU(14));
    disp_->print("GPS N/A");
  } else if (loc_have_saved) {
    disp_->setTextColor(TC_GREEN);
    disp_->setCursor(sx + tcU(2), statusY + tcU(14));
    disp_->print("SAVED");
  }

  const char* k2 =
      (loc_src == 0) ? (loc_have_saved ? "K2 USE" : "K2 LOCATE")
                     : (loc_src == 1 ? "K2 N/A" : "K2 SET");
  drawKbar("K1 BACK", k2);
  disp_->endWrite();
}

static void renderWifi() {
  // WiFi setup removed — provision via StopWatch system Setup app.
  gotoPage(PAGE_MENU);
}

static void renderAbout() {
  disp_->startWrite();
  disp_->fillScreen(TC_BG_DEEP);
  disp_->setTextSize(TC_FONT_SIZE);
  const int bw = tcU(180), bh = tcU(100);
  const int bx = (TC_SCREEN_W - bw) / 2;
  const int by = (TC_SCREEN_H - bh) / 2 - tcU(12);
  disp_->fillRect(bx, by, bw, bh, TC_PANEL_BG);
  disp_->drawRect(bx, by, bw, bh, TC_CYAN);
  auto centered = [&](int y, const char* t, uint16_t col) {
    disp_->setTextColor(col);
    disp_->setCursor(bx + (bw - (int)strlen(t) * TC_CHAR_W) / 2, y);
    disp_->print(t);
  };
  centered(by + tcU(8), "TYPHOON COMPASS", TC_CYAN);
  {
    char v[24]; snprintf(v, sizeof(v), "%s %s", FW_VERSION, FW_BATCH);
    centered(by + tcU(22), v, TC_TEXT);
  }
  centered(by + tcU(36), "HW: M5 StopWatch", TC_TEXT_DIM);
  centered(by + tcU(48), "MAP: SkyCompass", TC_TEXT_DIM);
  if (wifiIsConnected() && !data_stale)
    centered(by + tcU(60), "DATA: NMC LIVE", TC_TEXT_DIM);
  else
    centered(by + tcU(60), "DATA: DEMO/STALE", TC_TEXT_DIM);
  centered(by + tcU(76), "K1/K2 = BACK", TC_YELLOW);
  disp_ = view_sprite;
  G = view_sprite;
  drawKbar("K1 BACK", "K2 BACK");
  disp_->endWrite();
}

static void renderMulti() {
  // Guarantee MULTI scale every frame (coast + markers share earth_r)
  if (earth_r != EARTH_R_MULTI) setMapZoom(EARTH_R_MULTI);
  else { projectAll(); projectStorms(); }
  bool want_tilt = gyro_tilt && frame_sprite && view_sprite;
  bool active = want_tilt && (fabsf(tilt_pitch) > 0.35f || fabsf(tilt_roll) > 0.35f);

  G = frame_sprite ? (LovyanGFX*)frame_sprite : (LovyanGFX*)disp_;
  if (frame_sprite) bg_sprite->pushSprite(frame_sprite, 0, 0);
  else pushMapCrop(disp_);

  for (int i = 0; i < storm_count; i++) {
    if (storm_sx[i] < 0) continue;
    uint16_t lc = C(catColor(STORMS[i].cat));
    if (i != multi_sel) lc = C(TC_TEXT_DIM);
    drawDashedLine(user_sx, user_sy, storm_sx[i], storm_sy[i], lc);
  }
  for (int i = 0; i < storm_count; i++) {
    if (storm_sx[i] < 0) continue;
    bool sel = (i == multi_sel);
    drawStormIcon(storm_sx[i], storm_sy[i], STORMS[i].cat, sel);
    if (sel) {
      int rr = kmToPx(STORMS[i].r12);
      dashedCircle(storm_sx[i], storm_sy[i], rr, C(TC_RED), 12, 3);
      G->drawRect(storm_sx[i] - 12, storm_sy[i] - 12, 24, 24, C(TC_YELLOW));
      G->setTextColor(C(catColor(STORMS[i].cat)));
      G->setCursor(storm_sx[i] + 8, storm_sy[i] - 12);
      G->print(STORMS[i].name);
    }
  }
  drawUserMarker();
  drawCompass();

  LovyanGFX* screen = (view_sprite && frame_sprite) ? (LovyanGFX*)view_sprite : (LovyanGFX*)disp_;
  if (view_sprite && frame_sprite) {
    blitFrameToView(active);
  } else if (frame_sprite) {
    if (active) {
      float ang = tilt_roll * 0.35f;
      float dx = tilt_roll * 0.9f;
      float dy = -tilt_pitch * 0.9f;
      frame_sprite->pushRotateZoom(VIEW_W * 0.5f + dx, VIEW_H * 0.5f + dy, ang, 1.0f, 1.0f);
    } else {
      frame_sprite->pushSprite(disp_, -FRAME_OX, -FRAME_OY);
    }
  }

  G = screen;
  const int rowH = TC_CHAR_H + 2;
  const int pw = tcU(66);
  const int py = tcU(36);
  const int ph = TC_CHAR_H + 4 + storm_count * rowH;
  int px = circleSafeRight(py, 2, ph) - pw;
  G->fillRect(px, py, pw, ph, TC_BLACK);
  G->drawRect(px, py, pw, ph, C(TC_CYAN));
  G->setTextColor(C(TC_CYAN));
  G->setCursor(px + 2, py + 1);
  char hdr[12]; snprintf(hdr, sizeof(hdr), "ACT(%d)", storm_count);
  G->print(hdr);
  for (int i = 0; i < storm_count; i++) {
    int iy = py + TC_CHAR_H + 2 + i * rowH;
    bool sel = (i == multi_sel);
    if (sel) G->fillRect(px + 1, iy - 1, pw - 2, rowH, C(TC_CYAN));
    G->setTextColor(sel ? TC_BLACK : C(TC_TEXT));
    G->setCursor(px + 2, iy);
    G->print(STORMS[i].name);
    float d = distKm(user_lat, user_lon, STORMS[i].lat, STORMS[i].lon);
    char db[8]; snprintf(db, sizeof(db), "%d", (int)(d + 0.5f));
    G->setCursor(px + pw - 2 - (int)strlen(db) * TC_CHAR_W, iy);
    G->print(db);
  }

  drawMoonIcon();
  drawKbar("K1 FOCUS", "K2 NEXT");
  if (view_sprite && frame_sprite) presentView();
  disp_ = view_sprite;
  G = view_sprite;
}

static void render() {
  switch (page) {
    case PAGE_BOOT:     renderBoot(); break;
    case PAGE_MAIN:     renderMainBase(); break;
    case PAGE_DETAIL:   renderDetail(); break;
    case PAGE_TRACK:    renderTrack(); break;
    case PAGE_MENU:     renderMenu(); break;
    case PAGE_MULTI:    renderMulti(); break;
    case PAGE_LOCATION: renderLocation(); break;
    case PAGE_WIFI:     renderWifi(); break;
    case PAGE_ABOUT:    renderAbout(); break;
  }
}

static void handleEvt(uint8_t id, BtnEvt ev) {
  noteInput();
  if (page == PAGE_BOOT) {
    if (ev != EVT_NONE) { gotoPage(PAGE_MAIN); showEvent("BOOT SKIP", TC_CYAN); }
    return;
  }
  if (ev == EVT_COMBO) {
    gotoPage(PAGE_MAIN); showEvent("COMBO MAIN", TC_RED); return;
  }
  if (id == 0 && ev == EVT_DBL) {
    setNightMode(!night_mode);
    showEvent(night_mode ? "NIGHT ON" : "NIGHT OFF", TC_GREEN);
    return;
  }
  if (id == 1 && ev == EVT_DBL) {
    if (page == PAGE_LOCATION && loc_src == 0) {
      // Explicit IP re-geocode (saved fix is used by short press)
      startIpLocate();
      showEvent("RELOCATE", TC_YELLOW);
      return;
    }
    imuResetTilt();
    showEvent("GYRO RESET", TC_GREEN);
    return;
  }
  switch (page) {
    case PAGE_MAIN:
      if (id==0 && ev==EVT_SHORT) {
        applyStorm((storm_sel + storm_count - 1) % storm_count);
        char msg[20]; snprintf(msg, sizeof(msg), "TYP %s", typ_name);
        showEvent(msg, TC_CYAN);
      }
      if (id==0 && ev==EVT_LONG)  { gotoPage(PAGE_MENU); showEvent("MENU", TC_YELLOW); }
      if (id==1 && ev==EVT_SHORT) { gotoPage(PAGE_DETAIL); showEvent("DETAIL", TC_CYAN); }
      if (id==1 && ev==EVT_LONG)  { gotoPage(PAGE_MULTI); showEvent("MULTI", TC_YELLOW); }
      break;
    case PAGE_DETAIL:
      if (id==0 && ev==EVT_SHORT) gotoPage(PAGE_MAIN);
      if (id==1 && ev==EVT_SHORT) { gotoPage(PAGE_TRACK); showEvent("TIME MACHINE", TC_CYAN); }
      if (id==1 && ev==EVT_LONG) {
        ring_mode = (ring_mode + 1) % 5;
        static const char* RM[] = {"RINGS ALL","RINGS R7","RINGS R10","RINGS R12","RINGS OFF"};
        showEvent(RM[ring_mode], TC_YELLOW);
      }
      break;
    case PAGE_TRACK:
      if (id==0 && ev==EVT_SHORT) {
        tm_playing = false;
        gotoPage(PAGE_DETAIL);
      }
      if (id==1 && ev==EVT_SHORT) {
        if (tm_playing) {
          tm_playing = false;
          showEvent("PAUSED", TC_YELLOW);
        } else if (TRACK_N > 0) {
          // SEEK: jump to next real data point (hour-aligned)
          int next = (int)track_sel + 1;
          if (next >= (int)TRACK_N) next = 0;
          tm_hold_until = 0;
          tmEvalHour((float)TRACK[next].hour);
          char msg[16];
          int h = TRACK[next].hour;
          if (h == 0) snprintf(msg, sizeof(msg), "SEEK NOW");
          else if (h > 0) snprintf(msg, sizeof(msg), "SEEK +%dH", h);
          else snprintf(msg, sizeof(msg), "SEEK %dH", h);
          showEvent(msg, TC_CYAN);
        }
        ui_dirty = true;
      }
      if (id==1 && ev==EVT_LONG) {
        // Resume / restart autoplay (hour-scaled)
        tmStart(true);
        ui_dirty = true;
      }
      break;
    case PAGE_MENU:
      if (id==0 && ev==EVT_SHORT) {
        menu_cursor = (menu_cursor + MENU_N - 1) % MENU_N;
        showEvent(MENU_LABELS[menu_cursor], TC_CYAN);
      }
      if (id==0 && ev==EVT_LONG) gotoPage(PAGE_MAIN);
      if (id==1 && ev==EVT_SHORT) {
        switch (menu_cursor) {
          case 0: gotoPage(PAGE_LOCATION); showEvent("LOCATION", TC_CYAN); break;
          case 1: // DISPLAY brightness cycle
            brightness = (brightness <= 40) ? 70 : (brightness <= 70) ? 100 : 40;
            brightness_user = brightness;
            screen_dimmed = false;
            applyBrightness();
            { char b[16]; snprintf(b, sizeof(b), "BRIGHT %d%%", brightness); showEvent(b, TC_YELLOW); }
            break;
          case 2:
            gyro_tilt = !gyro_tilt;
            if (gyro_tilt) imuCaptureCalibration();
            else { tilt_pitch = tilt_roll = imu_pitch = imu_roll = 0; cal_ready = false; }
            showEvent(gyro_tilt ? "GYRO ON" : "GYRO OFF", TC_GREEN);
            break;
          case 3:
            setNightMode(!night_mode);
            showEvent(night_mode ? "NIGHT ON" : "NIGHT OFF", TC_GREEN);
            break;
          case 4: {
            static const uint16_t AR[] = {100, 200, 300, 500};
            uint8_t ai = 0;
            for (uint8_t k = 0; k < 4; k++) if (alert_radius_km == AR[k]) { ai = (k + 1) % 4; break; }
            alert_radius_km = AR[ai];
            char b[20]; snprintf(b, sizeof(b), "ALERT %dKM", alert_radius_km);
            showEvent(b, TC_YELLOW);
            break;
          }
          case 5:
            if (units_kt && units_km) { units_kt = true; units_km = false; }
            else if (units_kt && !units_km) { units_kt = false; units_km = true; }
            else if (!units_kt && units_km) { units_kt = false; units_km = false; }
            else { units_kt = true; units_km = true; }
            showEvent(units_kt ? (units_km?"KT/KM":"KT/NM") : (units_km?"MS/KM":"MS/NM"), TC_YELLOW);
            break;
          case 6: gotoPage(PAGE_ABOUT); break;
          case 7: gotoPage(PAGE_MAIN); showEvent("MAIN", TC_CYAN); break;
        }
      }
      break;
    case PAGE_LOCATION:
      if (id==0 && ev==EVT_SHORT) {
        loc_cursor = (loc_cursor + CITY_N - 1) % CITY_N;
        showEvent(CITIES[loc_cursor].name, TC_CYAN);
      }
      if (id==0 && ev==EVT_LONG) { gotoPage(PAGE_MENU); showEvent("MENU", TC_YELLOW); }
      if (id==1 && ev==EVT_SHORT) {
        if (loc_src == 0) {
          // Reuse remembered IP fix — do not hit the network every time.
          if (loc_have_saved) {
            setMapCenter(user_lat, user_lon, true);
            gotoPage(PAGE_MAIN);
            showEvent("SAVED LOC", TC_GREEN);
          } else {
            startIpLocate();
          }
        } else if (loc_src == 1) {
          showEvent("GPS N/A", TC_TEXT_DIM);
        } else {
          applyCity(loc_cursor);
          gotoPage(PAGE_MAIN);
          showEvent(city_name, TC_GREEN);
        }
      }
      if (id==1 && ev==EVT_LONG) {
        loc_src = (loc_src + 1) % 3;
        static const char* SN[] = {"SRC WIFI/IP", "SRC GPS", "SRC MANUAL"};
        showEvent(SN[loc_src], TC_YELLOW);
        ui_dirty = true;
      }
      break;
    case PAGE_WIFI:
      gotoPage(PAGE_MENU);
      showEvent("USE SYSTEM WIFI", TC_YELLOW);
      break;
    case PAGE_ABOUT:
      if (ev == EVT_SHORT || ev == EVT_LONG) gotoPage(PAGE_MENU);
      break;
    case PAGE_MULTI:
      if (id==0 && ev==EVT_SHORT) {
        applyStorm(multi_sel);
        gotoPage(PAGE_MAIN);
        showEvent(typ_name, TC_YELLOW);
      }
      if (id==0 && ev==EVT_LONG)  gotoPage(PAGE_MENU);
      if (id==1 && ev==EVT_SHORT) {
        multi_sel = (multi_sel + 1) % storm_count;
        char msg[20]; snprintf(msg, sizeof(msg), "SEL %s", STORMS[multi_sel].name);
        showEvent(msg, TC_CYAN);
      }
      if (id==1 && ev==EVT_LONG) {
        applyStorm(multi_sel);
        gotoPage(PAGE_DETAIL);
      }
      break;
    default: break;
  }
}

static void pollButtons() {
  uint32_t now = GetHAL().millis();
  bool raw[2] = { GetHAL().btnA.isPressed(), GetHAL().btnB.isPressed() };

  // Track press edges for combo timing + hold progress bar
  for (int i = 0; i < 2; i++) {
    if (raw[i] && !btn[i].down) {
      noteInput();
      btn[i].down = true;
      btn[i].t_down = now;
      if (!combo_wait_release && now >= combo_suppress_until) {
        hold_btn = i;
        hold_progress = 0;
      }
    } else if (!raw[i] && btn[i].down) {
      btn[i].down = false;
      if (hold_btn == i) { hold_progress = 0; hold_btn = -1; }
    } else if (raw[i] && btn[i].down && !combo_wait_release && now >= combo_suppress_until) {
      uint32_t held = now - btn[i].t_down;
      if (held >= BTN_HOLD_BAR_MS) {
        int p = (int)((held - BTN_HOLD_BAR_MS) * 100 / (BTN_LONG_MS > BTN_HOLD_BAR_MS
              ? (BTN_LONG_MS - BTN_HOLD_BAR_MS + 100) : 200));
        hold_progress = (uint8_t)(p > 100 ? 100 : (p < 0 ? 0 : p));
        hold_btn = i;
      }
    }
  }

  // After combo: ignore until both released
  if (combo_wait_release) {
    if (!raw[0] && !raw[1]) {
      combo_wait_release = false;
      combo_suppress_until = now + 550; // drop residual click-count decides
      memset(btn, 0, sizeof(btn));
      hold_progress = 0;
      hold_btn = -1;
    }
    return;
  }

  // Combo: both down → MAIN
  if (raw[0] && raw[1] && now >= combo_lock_until) {
    combo_lock_until = now + 600;
    combo_wait_release = true;
    hold_progress = 0;
    hold_btn = -1;
    beep(2000, 25);
    handleEvt(0, EVT_COMBO);
    return;
  }

  if (now < combo_suppress_until) return;

  // Single-key via M5Unified (DBL after ~500ms decide window)
  
  

  if (GetHAL().btnA.wasDoubleClicked()) { beep(1800, 5); handleEvt(0, EVT_DBL); }
  else if (GetHAL().btnA.wasClicked()) { beep(1200, 5); handleEvt(0, EVT_SHORT); }
  else if (GetHAL().btnA.wasHold()) {
    hold_progress = 0; hold_btn = -1;
    beep(1600, 10); handleEvt(0, EVT_LONG);
  }

  if (GetHAL().btnB.wasDoubleClicked()) { beep(2200, 5); handleEvt(1, EVT_DBL); }
  else if (GetHAL().btnB.wasClicked()) { beep(1200, 5); handleEvt(1, EVT_SHORT); }
  else if (GetHAL().btnB.wasHold()) {
    hold_progress = 0; hold_btn = -1;
    beep(1600, 10); handleEvt(1, EVT_LONG);
  }
}

void Engine::open() {
  
    
  
  
  
  if (disp_) disp_->setTextWrap(false);

  ESP_LOGI("Typhoon", "\n=== Typhoon Compass Batch 8 ===");
  brightness_user = brightness;
  last_input_ms = GetHAL().millis();
  seedDemoStorms();
  seedDemoTrack();
  applyBrightness();
  if (!loadObserverLocation()) {
    // First run — keep default Shanghai until user locates / picks a city
    loc_have_saved = false;
  }
  wifiTryRestore();
  if (wifiIsConnected()) data_fetch_pending = true;

  auto& parent = GetHAL().getDisplay();
  bg_sprite = new LGFX_Sprite(&parent);
  bg_sprite->setColorDepth(16);
  bg_sprite->setPsram(true);
  if (!bg_sprite->createSprite(FRAME_W, FRAME_H)) {
    ESP_LOGE("Typhoon", "[MAP] bg sprite alloc failed");
    delete bg_sprite;
    bg_sprite = nullptr;
    return;
  }
  bg_sprite->setTextWrap(false);
  frame_sprite = new LGFX_Sprite(&parent);
  frame_sprite->setColorDepth(16);
  frame_sprite->setPsram(true);
  if (!frame_sprite->createSprite(FRAME_W, FRAME_H)) {
    ESP_LOGW("Typhoon", "[IMU] frame sprite alloc failed — tilt disabled");
    delete frame_sprite;
    frame_sprite = nullptr;
  } else {
    frame_sprite->setTextWrap(false);
  }
  view_sprite = new LGFX_Sprite(&parent);
  view_sprite->setColorDepth(16);
  view_sprite->setPsram(true);
  if (!view_sprite->createSprite(VIEW_W, VIEW_H)) {
    ESP_LOGE("Typhoon", "[GFX] view sprite alloc failed");
    delete view_sprite;
    view_sprite = nullptr;
    return;
  }
  view_sprite->setTextWrap(false);
  view_sprite->setTextSize(TC_FONT_SIZE);
  if (bg_sprite) bg_sprite->setTextSize(TC_FONT_SIZE);
  if (frame_sprite) frame_sprite->setTextSize(TC_FONT_SIZE);
  ESP_LOGI("Typhoon", "[GFX] view %dx%d native canvas OK", VIEW_W, VIEW_H);
  disp_ = view_sprite;
  G = view_sprite;
  imu_ok = true;
  // StickS3 also builds earth once, but Arduino loop yields; here we defer the heavy
  // coastline pass out of onOpen() so Mooncake/LVGL teardown + first frame stay light.
  if (bg_sprite) {
    bg_sprite->fillScreen(TC_OCEAN);
    drawGeoGrid(bg_sprite, bg_sprite->color565(22, 55, 65));
  }
  earth_pending = true;
  applyStorm(0);
  memset(btn, 0, sizeof(btn));
  page = PAGE_MAIN;
  ui_dirty = true;
  boot_progress = 100;
  last_frame = last_blink = GetHAL().millis();
}

void Engine::update() {
  uint32_t now = GetHAL().millis();
  pollIpLocate();

  if (earth_pending && bg_sprite) {
    GetHAL().feedTheDog();
    buildEarthBg(bg_sprite);
    GetHAL().feedTheDog();
    earth_pending = false;
    ui_dirty = true;
    last_frame = now;
    render();
    return;
  }

  pollButtons();
  imuPoll();

  // Auto Time Machine: hold full brightness; idle dim only after leaving TRACK
  if (page == PAGE_TRACK && tm_playing) {
    last_input_ms = now;
    if (screen_dimmed) {
      screen_dimmed = false;
      applyBrightness();
    }
  } else if (page != PAGE_BOOT && !screen_dimmed && (now - last_input_ms > IDLE_DIM_MS)) {
    // Batch 6: 15s idle → dim backlight to 20%
    screen_dimmed = true;
    applyBrightness();
  }

  if (event_show_ms && (now - event_show_ms > 1800)) {
    event_show_ms = 0;
    ui_dirty = true; // restore kbar text
  }
  if (hold_progress != last_hold_prog) {
    last_hold_prog = hold_progress;
    ui_dirty = true;
  }

  if (page == PAGE_BOOT) {
    uint32_t elapsed = now - boot_start_ms;
    if (elapsed < 1200) boot_progress = (uint8_t)(elapsed * 68 / 1200);
    else if (elapsed < 2200) boot_progress = 68;
    else if (elapsed < 3000) boot_progress = (uint8_t)(68 + (elapsed - 2200) * 32 / 800);
    else { boot_progress = 100; gotoPage(PAGE_MAIN); showEvent("MAIN", TC_CYAN); }
    if (now - last_frame >= 80) {
      boot_angle = (boot_angle + 12) % 360;
      last_frame = now;
      render();
    }
    /* yield */
    return;
  }

  // Time Machine: follow-cam rebuild is heavier — ~5–6 fps is enough at 2.5s/pt
  if (page == PAGE_TRACK) {
    if (tm_playing) tmTick(now);
    if (tm_playing || ui_dirty) {
      if (now - last_frame >= 160 || ui_dirty) {
        last_frame = now;
        render();
        ui_dirty = false;
      }
    }
    /* yield */
    return;
  }

  // DETAIL / MENU: static UI — redraw only when dirty (no 10fps flash)
  bool static_page = (page == PAGE_DETAIL || page == PAGE_MENU
                   || page == PAGE_LOCATION || page == PAGE_ABOUT);

  if (static_page) {
    if (ui_dirty) {
      render();
      ui_dirty = false;
    }
    /* yield */
    return;
  }

  if (now - last_blink >= 600) { blink_on = !blink_on; last_blink = now; }
  bool tilting = gyro_tilt && (fabsf(tilt_pitch) > 0.4f || fabsf(tilt_roll) > 0.4f);
  uint32_t frame_ms = tilting ? 66 : 100; // ~15fps while tilting
  if (now - last_frame >= frame_ms) {
    spiral_a1 = (spiral_a1 + 12) % 360;
    spiral_a2 = (spiral_a2 + 9) % 360;
    spiral_a3 = (spiral_a3 + 7) % 360;
    pulse_phase++;
    last_frame = now;
    render();
  }
  /* yield */
}

// --- Engine public API ---

Engine::~Engine() { close(); }

void Engine::close() {
    if (bg_sprite) { bg_sprite->deleteSprite(); delete bg_sprite; bg_sprite = nullptr; }
    if (frame_sprite) { frame_sprite->deleteSprite(); delete frame_sprite; frame_sprite = nullptr; }
    if (view_sprite) { view_sprite->deleteSprite(); delete view_sprite; view_sprite = nullptr; }
    disp_ = nullptr;
}

void Engine::applySnapshot(const TyphoonSnapshot& snap) {
    if (snap.count == 0) return;
    static uint32_t last_applied_ms = 0;
    if (snap.updated_ms != 0 && snap.updated_ms == last_applied_ms) return;
    last_applied_ms = snap.updated_ms;

    // Keep current pick before STORMS[] is overwritten (typ_name aliases into it).
    char prevName[16] = {};
    if (typ_name) std::strncpy(prevName, typ_name, sizeof(prevName) - 1);
    const bool wasLive = storms_are_live;

    // Apply in place — do not allocate NmcTrack[STORM_MAX] on the main task stack.
    uint8_t n = snap.count;
    if (n > STORM_MAX) n = STORM_MAX;
    storm_count = n;
    for (uint8_t i = 0; i < n; i++) {
        std::strncpy(STORMS[i].name, snap.storms[i].name, sizeof(STORMS[i].name) - 1);
        STORMS[i].name[sizeof(STORMS[i].name) - 1] = 0;
        STORMS[i].lat = snap.storms[i].lat;
        STORMS[i].lon = snap.storms[i].lon;
        STORMS[i].cat = snap.storms[i].category;
        STORMS[i].wind_kt = static_cast<uint8_t>(snap.storms[i].wind);
        STORMS[i].pressure = snap.storms[i].pressure;
        STORMS[i].r7 = snap.storms[i].r7;
        STORMS[i].r10 = snap.storms[i].r10;
        STORMS[i].r12 = snap.storms[i].r12;
        storm_tracks[i] = snap.tracks[i];
    }
    data_stale = !snap.live;
    data_fetch_ms = GetHAL().millis();
    storms_are_live = snap.live;

    uint8_t sel = 0;
    if (wasLive && snap.live) {
      // Refresh: stick to the same named storm when still active
      int m = findStormIndexByName(prevName, n);
      sel = (m >= 0) ? (uint8_t)m : findNearestStormIndex(n);
    } else if (snap.live) {
      // First live replace of demo → nearest to observer
      sel = findNearestStormIndex(n);
    } else {
      if (storm_sel < n) sel = storm_sel;
    }
    if (multi_sel >= n) multi_sel = sel;
    applyStorm(sel);
    ui_dirty = true;
    ESP_LOGI("Typhoon", "[DATA] snapshot n=%u live=%d sel=%s (was %s)",
             n, (int)snap.live, typ_name, prevName[0] ? prevName : "-");
}

LGFX_Sprite* Engine::legacySprite() { return view_sprite; }

float Engine::presentScale() const
{
    (void)page;
    // Native 466 canvas → 1:1 on StopWatch panel (no low-res upscale).
    return 1.0f;
}

}  // namespace typhoon
