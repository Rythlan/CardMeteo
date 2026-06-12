#include "common.h"

// --- Sensor Instances ---
Adafruit_AHTX0 aht;
Adafruit_BMP280 bmp;
Preferences prefs;
M5Canvas canvas(&M5Cardputer.Display);

// --- Sensor Status ---
bool aht_found = false;
bool bmp_found = false;

float bat_smoothed = 0.0f;

// --- State ---
DeviceConfig config;
DisplayState state;
Telemetry telemetry;

Theme themes[5] = {
    {TFT_BLACK, TFT_NAVY, TFT_WHITE, TFT_MAROON, TFT_DARKGREEN, TFT_DARKCYAN},
    {TFT_BLACK, 0x01E0, TFT_GREEN, 0x0120, 0x0120, 0x0120},
    {0x0010, 0x0008, TFT_WHITE, 0x18E3, 0x0410, 0x0210},
    {0x2945, 0x18C3, TFT_WHITE, 0x8A22, 0x6104, 0x4082},
    {TFT_WHITE, TFT_LIGHTGREY, TFT_BLACK, TFT_SILVER, TFT_SILVER, TFT_SILVER}};

void playTone(int type)
{
    if (!config.sound_enabled)
        return;
    if (type == 1)
    {
        M5Cardputer.Speaker.tone(1000, 100);
        delay(100);
        M5Cardputer.Speaker.tone(1500, 150);
    }
    else if (type == -1)
    {
        M5Cardputer.Speaker.tone(1000, 100);
        delay(100);
        M5Cardputer.Speaker.tone(800, 150);
    }
    else
    {
        M5Cardputer.Speaker.tone(2000, 50);
    }
}

void saveSettings()
{
    prefs.begin("weather_app", false);
    prefs.putFloat("alt", config.altitude);
    prefs.putInt("intvl", config.update_interval);
    prefs.putBool("snd", config.sound_enabled);
    prefs.putInt("brt", config.screen_brightness);
    prefs.putInt("dimto", config.auto_dim_timeout);
    prefs.end();
    state.settings_dirty = false;
}

void loadSettings()
{
    prefs.begin("weather_app", true);
    config.altitude = prefs.getFloat("alt", 100.0f);
    config.update_interval = prefs.getInt("intvl", 1);
    config.sound_enabled = prefs.getBool("snd", false);
    config.show_pressure = prefs.getBool("prs", true);
    state.current_theme = prefs.getInt("thm", 0);
    config.screen_brightness = prefs.getInt("brt", 255);
    config.auto_dim_timeout = prefs.getInt("dimto", 30);
}

void initSensors()
{
    Wire.begin(2, 1);
    M5Cardputer.Display.setRotation(1);
    canvas.createSprite(240, 135);
    canvas.fillSprite(TFT_BLACK);
    canvas.setTextColor(TFT_GREEN);
    canvas.setTextDatum(middle_center);
    canvas.drawString("Initializing Sensors...", 120, 67);
    canvas.pushSprite(0, 0);
    if (aht.begin(&Wire, 0, 0x38))
        aht_found = true;
    if (bmp.begin(0x77))
    {
        bmp_found = true;
        bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                        Adafruit_BMP280::SAMPLING_X1, Adafruit_BMP280::SAMPLING_X4,
                        Adafruit_BMP280::FILTER_X16, Adafruit_BMP280::STANDBY_MS_1000);
    }
    delay(1000);
}

static void handleGlobalKeys()
{
    if (M5Cardputer.Keyboard.isKeyPressed(27) || M5Cardputer.Keyboard.isKeyPressed('`'))
        ESP.restart();
    if (M5Cardputer.Keyboard.isKeyPressed('0'))
    {
        state.is_dimmed = !state.is_dimmed;
        M5Cardputer.Display.setBrightness(state.is_dimmed ? 0 : config.screen_brightness);
        config.auto_dimmed = false;
        config.last_keypress_time = millis();
    }
    for (char k = '1'; k <= '5'; k++)
        if (M5Cardputer.Keyboard.isKeyPressed(k))
        {
            state.current_theme = k - '1';
            playTone(0);
            state.redraw = true;
        }
}

static void handleDashboardKeys()
{
    if (M5Cardputer.Keyboard.isKeyPressed(' ') || M5Cardputer.Keyboard.isKeyPressed('\r'))
        state.current_window = 1;
    if (M5Cardputer.Keyboard.isKeyPressed('p'))
        config.show_pressure = !config.show_pressure;
}

static void handleForecastKeys()
{
    if (M5Cardputer.Keyboard.isKeyPressed(' ') || M5Cardputer.Keyboard.isKeyPressed('\r'))
        state.current_window = 0;
}

static void handleSettingsKeys()
{
    bool navD = M5Cardputer.Keyboard.isKeyPressed('s') || M5Cardputer.Keyboard.isKeyPressed('S') || M5Cardputer.Keyboard.isKeyPressed('.');
    bool navU = M5Cardputer.Keyboard.isKeyPressed('e') || M5Cardputer.Keyboard.isKeyPressed('E') || M5Cardputer.Keyboard.isKeyPressed(';');
    if (navD)
    {
        state.settings_cursor = (state.settings_cursor + 1) % 5;
        state.redraw = true;
    }
    if (navU)
    {
        state.settings_cursor = (state.settings_cursor > 0) ? state.settings_cursor - 1 : 4;
        state.redraw = true;
    }
    bool aP = M5Cardputer.Keyboard.isKeyPressed('a') || M5Cardputer.Keyboard.isKeyPressed('A') || M5Cardputer.Keyboard.isKeyPressed(',');
    bool dP = M5Cardputer.Keyboard.isKeyPressed('d') || M5Cardputer.Keyboard.isKeyPressed('D') || M5Cardputer.Keyboard.isKeyPressed('/');
    if (aP || dP)
    {
        int step = aP ? -1 : 1;
        switch (state.settings_cursor)
        {
        case 0:
            config.altitude = constrain(config.altitude + step * 5.0f, 0.0f, 5000.0f);
            break;
        case 1:
            config.update_interval = constrain(config.update_interval + step, 1, 60);
            break;
        case 2:
            config.sound_enabled = !config.sound_enabled;
            break;
        case 3:
            config.screen_brightness = constrain(config.screen_brightness + step * 25, 0, 255);
            M5Cardputer.Display.setBrightness(config.screen_brightness);
            config.auto_dimmed = false;
            state.is_dimmed = false;
            break;
        case 4:
            config.auto_dim_timeout = constrain(config.auto_dim_timeout + step * 5, 5, 120);
            break;
        }
        state.settings_dirty = true;
        state.redraw = true;
    }
}

void handleKeyboard()
{
    M5Cardputer.update();
    bool isPressed = M5Cardputer.Keyboard.isPressed();
    if (!isPressed && !state.wasPressed)
        return;
    if (isPressed && !state.wasPressed)
    {
        config.last_keypress_time = millis();
        if (config.auto_dimmed)
        {
            config.auto_dimmed = false;
            M5Cardputer.Display.setBrightness(config.screen_brightness);
            state.redraw = true;
        }
        handleGlobalKeys();
        if (state.current_window == 0)
            handleDashboardKeys();
        else if (state.current_window == 1)
            handleForecastKeys();
        else if (state.current_window == 2)
            handleSettingsKeys();
    }
    state.wasPressed = isPressed;
}

void setup()
{
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    M5Cardputer.Speaker.begin();
    M5Cardputer.Speaker.setVolume(128);
    loadSettings();
    M5Cardputer.Display.setBrightness(config.screen_brightness);
    initSensors();
    telemetry.trendStartTime = millis();
}

void loop()
{
    handleKeyboard();
    static unsigned long mKeyTimer = 0;
    if ((M5Cardputer.Keyboard.isKeyPressed('M') || M5Cardputer.Keyboard.isKeyPressed('m')) && millis() - mKeyTimer > 300)
    {
        mKeyTimer = millis();
        if (state.current_window == 2)
            saveSettings();
        state.current_window = (state.current_window == 2) ? 0 : 2;
        playTone(0);
        state.redraw = true;
    }
    if (config.auto_dim_timeout > 0 && !state.is_dimmed && !config.auto_dimmed && state.current_window != 2)
    {
        if (millis() - config.last_keypress_time > (unsigned long)(config.auto_dim_timeout * 1000))
        {
            M5Cardputer.Display.setBrightness(0);
            config.auto_dimmed = true;
            state.redraw = true;
        }
    }
    if (millis() - telemetry.lastUpdate > (config.update_interval * 1000))
    {
        telemetry.lastUpdate = millis();
        readSensors();
        if (bmp_found)
            updateTrend(telemetry.slp);
        if (state.current_window != 2)
            state.redraw = true;
    }
    if (state.redraw)
    {
        if (state.current_window == 0)
            drawDashboard();
        else if (state.current_window == 1)
            drawForecast();
        else if (state.current_window == 2)
            drawSettings();
        canvas.pushSprite(0, 0);
        state.redraw = false;
    }
    delay(10);
}
