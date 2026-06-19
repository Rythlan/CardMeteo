#ifndef COMMON_H
#define COMMON_H

#include <M5Cardputer.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>
#include <Arduino.h>

#define SD_LOG_INTERVAL_MIN 10

// --- Hardware Instances ---
extern Adafruit_AHTX0 aht;
extern Adafruit_BMP280 bmp;
extern M5Canvas canvas;

// --- Sensor Status ---
extern bool aht_found;
extern bool bmp_found;

// --- Battery Smoothing ---
extern float bat_smoothed;

// --- Configuration / Settings ---
struct DeviceConfig
{
    float altitude = 100.0f;
    int update_interval = 1;
    bool sound_enabled = false;
    bool show_pressure = true;
    int screen_brightness = 255;
    int auto_dim_timeout = 60;
    unsigned long last_keypress_time = 0;
    bool auto_dimmed = false;
    // Optional SD card CSV logging (off by default).
    bool sd_logging = false;
    unsigned long last_sd_log = 0;
    // CPU frequency: 0 = Auto (240 awake / 80 dimmed), else fixed 240/160/80.
    int cpu_freq = 0;

    // --- Advanced Settings ---
    int volume = 128;
    float temp_offset = 0.0f;
    float hum_offset = 0.0f;
    float press_offset = 0.0f;
    bool alerts_enabled = false;
    float temp_high_thresh = 35.0f;
    float temp_low_thresh = 0.0f;
    float hum_high_thresh = 80.0f;
    float hum_low_thresh = 20.0f;
};
extern DeviceConfig config;

// --- Display State ---
struct DisplayState
{
    int current_window = 0;
    bool redraw = true;
    bool is_dimmed = false;
    int current_theme = 0;
    int settings_cursor = 0;
    int adv_settings_cursor = 0;
    bool wasPressed = false;
    bool settings_dirty = false;

    // Alert state tracking (prevents beeping every loop)
    bool prev_t_alert = false;
    bool prev_h_alert = false;
};
extern DisplayState state;

// --- Telemetry / Sensor Readings ---
struct Telemetry
{
    float temperature = 0.0f;
    float humidity = 0.0f;
    float pressure = 0.0f;
    float slp = 0.0f;
    float slp_history = 0.0f;
    unsigned long lastUpdate = 0;

    int pressure_trend = 0;
    bool trend_ready = false;
    int trend_samples = 0;
    int trend_consistent = 0;
    unsigned long trendTimer = 0;
    unsigned long trendStartTime = 0;

    // --- Pressure history ring buffer for rate-of-change & stats ---
    // One sample every 5 minutes, ~3 hours of history.
    static const int HISTORY_SIZE = 36;
    float slp_buf[HISTORY_SIZE] = {0.0f};
    unsigned long slp_time[HISTORY_SIZE] = {0UL};
    int hist_head = 0;
    int hist_count = 0;
    float slp_rate = 0.0f; // hPa / hour (least-squares slope)
    float slp_min = 0.0f;
    float slp_max = 0.0f;
    float slp_avg = 0.0f;
    int zambretti = -1; // Zambretti code 0..26 (higher = worse)
};
extern Telemetry telemetry;

struct Theme
{
    uint16_t bg, header, text, box1, box2, box3;
};
extern Theme themes[5];

// --- Function Declarations ---
void playTone(int type);
void saveSettings();
void loadSettings();
void toggleSDLogging();
void applyCpuFrequency(bool dimmed);
const char *predictWeather(float current_slp, int trend);
int computeZambretti(float slp, int trend, float rate3h);
const char *zambrettiLabel(int z);
void initSensors();

void drawHeader(const char *title);
void drawFooter(const char *hint);
void drawDashboard();
void drawForecast();
void drawStats();
void drawSettings();
void drawAdvancedSettings();

void readSensors();
void checkAlerts();
void updateTrend(float current_slp);
void updateHistory(float current_slp);
void logToSD();

void handleKeyboard();

#endif // COMMON_H
