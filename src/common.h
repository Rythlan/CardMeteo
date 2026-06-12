#ifndef COMMON_H
#define COMMON_H

#include <M5Cardputer.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>
#include <Preferences.h>
#include <Arduino.h>

// --- Hardware Instances ---
extern Adafruit_AHTX0 aht;
extern Adafruit_BMP280 bmp;
extern Preferences prefs;
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
    bool wasPressed = false;
    bool settings_dirty = false;
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
const char *predictWeather(float current_slp, int trend);
void initSensors();

void drawHeader(const char *title);
void drawFooter(const char *hint);
void drawDashboard();
void drawForecast();
void drawSettings();

void readSensors();
void updateTrend(float current_slp);

void handleKeyboard();

#endif // COMMON_H
