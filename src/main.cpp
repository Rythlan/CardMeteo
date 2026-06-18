#include "common.h"
#include "esp32-hal-cpu.h"

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
    prefs.putBool("prs", config.show_pressure);
    prefs.putInt("brt", config.screen_brightness);
    prefs.putInt("dimto", config.auto_dim_timeout);
    prefs.putBool("sdlog", config.sd_logging);
    prefs.putInt("cpu", config.cpu_freq);
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
    config.sd_logging = prefs.getBool("sdlog", false);
    config.cpu_freq = prefs.getInt("cpu", 0);
    prefs.end();
}

// Apply the configured CPU frequency. Auto mode underclocks to 80 MHz while
// dimmed (battery) and returns to 240 MHz when awake
void applyCpuFrequency(bool dimmed)
{
    int target = (config.cpu_freq != 0) ? config.cpu_freq : (dimmed ? 80 : 240);
    if ((int)getCpuFrequencyMhz() != target)
        setCpuFrequencyMhz((uint32_t)target);
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
        // FORCED mode: one measurement on demand, sleep in between (low power).
        bmp.setSampling(Adafruit_BMP280::MODE_FORCED,
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
        applyCpuFrequency(state.is_dimmed);
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
    if (state.current_window != 2)
    {
        if (M5Cardputer.Keyboard.isKeyPressed('t') || M5Cardputer.Keyboard.isKeyPressed('T'))
        {
            state.current_window = (state.current_window == 3) ? 0 : 3;
            playTone(0);
            state.redraw = true;
        }
        if (M5Cardputer.Keyboard.isKeyPressed('l') || M5Cardputer.Keyboard.isKeyPressed('L'))
            toggleSDLogging();
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

static void handleStatsKeys()
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
        state.settings_cursor = (state.settings_cursor + 1) % 6;
        state.redraw = true;
    }
    if (navU)
    {
        state.settings_cursor = (state.settings_cursor > 0) ? state.settings_cursor - 1 : 5;
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
        case 5:
            // Cycle: Auto(0) -> 240 -> 160 -> 80 -> Auto.
            {
                static const int opts[] = {0, 240, 160, 80};
                const int nopts = 4;
                int idx = 0;
                for (int i = 0; i < nopts; i++)
                    if (opts[i] == config.cpu_freq)
                    {
                        idx = i;
                        break;
                    }
                idx = (idx + step + nopts) % nopts;
                config.cpu_freq = opts[idx];
                applyCpuFrequency(config.auto_dimmed);
            }
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
            applyCpuFrequency(false);
            state.redraw = true;
        }
        handleGlobalKeys();
        if (state.current_window == 0)
            handleDashboardKeys();
        else if (state.current_window == 1)
            handleForecastKeys();
        else if (state.current_window == 2)
            handleSettingsKeys();
        else if (state.current_window == 3)
            handleStatsKeys();
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
    applyCpuFrequency(false);
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
            applyCpuFrequency(true);
            state.redraw = true;
        }
    }
    if (millis() - telemetry.lastUpdate > (config.update_interval * 1000))
    {
        telemetry.lastUpdate = millis();
        readSensors();
        if (bmp_found)
            updateTrend(telemetry.slp);
        logToSD();
        // Skip redraws while the screen is dimmed
        if (!config.auto_dimmed && state.current_window != 2)
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
        else if (state.current_window == 3)
            drawStats();
        canvas.pushSprite(0, 0);
        state.redraw = false;
    }
    // Longer sleep while dimmed: fewer wake-ups
    delay(config.auto_dimmed ? 150 : 10);
}
