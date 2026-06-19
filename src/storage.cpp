#include "common.h"
#include <SPI.h>
#include <SD.h>

static SPIClass sdSPI(HSPI);
static bool sd_inited = false;
static bool sd_available = false;

bool ensureSDReady() {
    if (!sd_inited) {
        sdSPI.begin(40, 39, 14, 12); 
        sd_available = SD.begin(12, sdSPI, 25000000);
        sd_inited = true;
    }
    if (sd_available && !SD.exists("/cardmeteo")) {
        SD.mkdir("/cardmeteo");
    }
    return sd_available;
}

void deinitSD() {
    sd_inited = false;
    sd_available = false;
}

void toggleSDLogging() {
    config.sd_logging = !config.sd_logging;
    saveSettings(); // Persist immediately to SD
    if (!config.sd_logging) deinitSD();
    playTone(config.sd_logging ? 1 : -1);
    state.redraw = true;
}

void loadSettings() {
    if (!ensureSDReady()) return; // Fallback to defaults if no SD
    
    File f = SD.open("/cardmeteo/settings.cfg", FILE_READ);
    if (!f) return;
    
    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        int eq = line.indexOf('=');
        if (eq < 0) continue;
        
        String key = line.substring(0, eq);
        String val = line.substring(eq + 1);
        
        if (key == "alt") config.altitude = val.toFloat();
        else if (key == "intvl") config.update_interval = val.toInt();
        else if (key == "snd") config.sound_enabled = (val == "1");
        else if (key == "prs") config.show_pressure = (val == "1");
        else if (key == "thm") state.current_theme = val.toInt();
        else if (key == "brt") config.screen_brightness = val.toInt();
        else if (key == "dimto") config.auto_dim_timeout = val.toInt();
        else if (key == "sdlog") config.sd_logging = (val == "1");
        else if (key == "cpu") config.cpu_freq = val.toInt();
        else if (key == "vol") config.volume = val.toInt();
        else if (key == "toffset") config.temp_offset = val.toFloat();
        else if (key == "hoffset") config.hum_offset = val.toFloat();
        else if (key == "poffset") config.press_offset = val.toFloat();
        else if (key == "alert") config.alerts_enabled = (val == "1");
        else if (key == "thi") config.temp_high_thresh = val.toFloat();
        else if (key == "tlo") config.temp_low_thresh = val.toFloat();
        else if (key == "hhi") config.hum_high_thresh = val.toFloat();
        else if (key == "hlo") config.hum_low_thresh = val.toFloat();
    }
    f.close();
    
    // If alerts are enabled, force sound on
    if (config.alerts_enabled) config.sound_enabled = true;
}

void saveSettings() {
    if (!ensureSDReady()) return;
    
    File f = SD.open("/cardmeteo/settings.cfg", FILE_WRITE);
    if (!f) return;
    
    f.printf("alt=%.1f\n", config.altitude);
    f.printf("intvl=%d\n", config.update_interval);
    f.printf("snd=%d\n", config.sound_enabled ? 1 : 0);
    f.printf("prs=%d\n", config.show_pressure ? 1 : 0);
    f.printf("thm=%d\n", state.current_theme);
    f.printf("brt=%d\n", config.screen_brightness);
    f.printf("dimto=%d\n", config.auto_dim_timeout);
    f.printf("sdlog=%d\n", config.sd_logging ? 1 : 0);
    f.printf("cpu=%d\n", config.cpu_freq);
    f.printf("vol=%d\n", config.volume);
    f.printf("toffset=%.2f\n", config.temp_offset);
    f.printf("hoffset=%.2f\n", config.hum_offset);
    f.printf("poffset=%.2f\n", config.press_offset);
    f.printf("alert=%d\n", config.alerts_enabled ? 1 : 0);
    f.printf("thi=%.1f\n", config.temp_high_thresh);
    f.printf("tlo=%.1f\n", config.temp_low_thresh);
    f.printf("hhi=%.1f\n", config.hum_high_thresh);
    f.printf("hlo=%.1f\n", config.hum_low_thresh);
    f.close();
}

void logToSD() {
    if (!config.sd_logging) return;

    unsigned long interval = (unsigned long)SD_LOG_INTERVAL_MIN * 60UL * 1000UL;
    if (config.last_sd_log != 0 && (millis() - config.last_sd_log) < interval) return;
    config.last_sd_log = millis();

    if (!ensureSDReady()) return;

    File f = SD.open("/cardmeteo/meteo.csv", FILE_APPEND);
    if (!f) return;

    if (f.size() == 0)
        f.println("uptime_ms,temp_c,humidity,pressure_hpa,slp_hpa,trend,rate_hpa_h,bat_pct");

    int mv = M5Cardputer.Power.getBatteryVoltage();
    int bat = map(constrain(mv, 3300, 4200), 3300, 4200, 0, 100);
    char row[112];
    snprintf(row, sizeof(row), "%lu,%.2f,%.2f,%.2f,%.2f,%d,%.3f,%d",
             millis(), telemetry.temperature, telemetry.humidity,
             telemetry.pressure, telemetry.slp, telemetry.pressure_trend,
             telemetry.slp_rate, bat);
    f.println(row);
    f.close();
}
