#include "common.h"

void drawHeader(const char *title)
{
    Theme t = themes[state.current_theme];
    canvas.fillSprite(t.bg);
    canvas.fillRect(0, 0, 240, 20, t.header);
    canvas.setTextColor(t.text);
    uint16_t vbat_mV = M5Cardputer.Power.getBatteryVoltage();
    if (bat_smoothed == 0.0f)
        bat_smoothed = (float)vbat_mV;
    else
        bat_smoothed = bat_smoothed * 0.85f + (float)vbat_mV * 0.15f;
    int batPct = map(constrain((int)bat_smoothed, 3300, 4200), 3300, 4200, 0, 100);
    char batStr[16];
    snprintf(batStr, sizeof(batStr), "Bat:%d%%", batPct);
    canvas.setTextDatum(middle_right);
    canvas.drawString(batStr, 235, 10);
    canvas.setTextDatum(middle_left);
    canvas.drawString(title, 5, 10);
}

void drawFooter(const char *hint)
{
    Theme t = themes[state.current_theme];
    canvas.fillRect(0, 115, 240, 20, t.header);
    canvas.setTextColor(t.text);
    canvas.setTextDatum(middle_center);
    canvas.drawString(hint, 120, 125);
}

void drawDashboard()
{
    Theme t = themes[state.current_theme];
    drawHeader("DASHBOARD");
    canvas.setTextColor(t.text);
    canvas.setTextDatum(top_center);
    char tStr[16], hStr[16], pStr[32];
    snprintf(tStr, sizeof(tStr), "%.1f C", telemetry.temperature);
    snprintf(hStr, sizeof(hStr), "%.1f %%", telemetry.humidity);
    snprintf(pStr, sizeof(pStr), "%.1f hPa", telemetry.pressure);
    if (config.show_pressure)
    {
        canvas.fillRoundRect(5, 25, 110, 40, 4, t.box1);
        canvas.fillRoundRect(125, 25, 110, 40, 4, t.box2);
        canvas.fillRoundRect(5, 70, 230, 40, 4, t.box3);
        canvas.drawString("TEMP", 60, 28);
        canvas.drawString((bmp_found || aht_found) ? tStr : "N/A", 60, 45);
        canvas.drawString("HUMIDITY", 180, 28);
        canvas.drawString(aht_found ? hStr : "N/A", 180, 45);
        canvas.drawString("LOCAL PRESSURE", 120, 73);
        canvas.drawString(bmp_found ? pStr : "N/A", 120, 90);
    }
    else
    {
        canvas.fillRoundRect(5, 25, 110, 85, 6, t.box1);
        canvas.fillRoundRect(125, 25, 110, 85, 6, t.box2);
        canvas.drawString("TEMP", 60, 35);
        canvas.drawString("HUMIDITY", 180, 35);
        canvas.setTextSize(1.5);
        canvas.drawString((bmp_found || aht_found) ? tStr : "N/A", 60, 65);
        canvas.drawString(aht_found ? hStr : "N/A", 180, 65);
        canvas.setTextSize(1);
    }
    drawFooter("[SPC]Forecast [P]S\\H Pres [M]Set");
}

void drawForecast()
{
    Theme t = themes[state.current_theme];
    drawHeader("FORECAST");
    canvas.fillRoundRect(10, 25, 220, 95, 8, t.box3);
    canvas.setTextColor(t.text);
    canvas.setTextDatum(top_center);
    if (bmp_found)
    {
        char slpStr[32], statusStr[48];
        snprintf(slpStr, sizeof(slpStr), "Sea Level: %.1f hPa", telemetry.slp);
        canvas.drawString(slpStr, 120, 32);
        canvas.setTextSize(1.5);
        canvas.drawString(predictWeather(telemetry.slp, telemetry.pressure_trend), 120, 55);

        unsigned long elapsed = millis() - telemetry.trendStartTime;
        unsigned long mins = elapsed / 60000UL;
        unsigned long hrs = mins / 60UL;
        mins %= 60UL;
        char timeStr[16];
        if (hrs > 0)
            snprintf(timeStr, sizeof(timeStr), "%luh %lum", hrs, mins);
        else
            snprintf(timeStr, sizeof(timeStr), "%lum", mins);

        if (telemetry.slp_history == 0.0f)
        {
            canvas.setTextColor(TFT_LIGHTGREY);
            canvas.drawString("Collecting data...", 120, 72);
            canvas.drawString(timeStr, 120, 89);
        }
        else
        {
            const char *dir;
            if (telemetry.pressure_trend == 1)
                dir = "RISING";
            else if (telemetry.pressure_trend == -1)
                dir = "FALLING";
            else
                dir = "STEADY";

            if (telemetry.trend_ready)
            {
                canvas.setTextColor(TFT_GREEN);
                canvas.drawString("TREND CONFIRMED", 120, 72);
            }
            else if (telemetry.trend_samples > 0)
            {
                canvas.setTextColor(TFT_YELLOW);
                canvas.drawString("POTENTIAL TREND", 120, 72);
            }
            else
            {
                canvas.setTextColor(TFT_LIGHTGREY);
                canvas.drawString("STEADY", 120, 72);
            }
            canvas.setTextColor(TFT_LIGHTGREY);
            canvas.drawString(timeStr, 120, 89);
        }
    }
    else
    {
        canvas.drawString("BMP280 Sensor Required", 120, 55);
    }
    drawFooter("[M] Settings");
}

void drawSettings()
{
    Theme t = themes[state.current_theme];
    drawHeader("SETTINGS");
    canvas.setTextColor(t.text);
    canvas.setTextDatum(middle_left);
    canvas.drawString(state.settings_cursor == 0 ? "> Altitude:" : "  Altitude:", 10, 28);

    char altStr[16];
    snprintf(altStr, sizeof(altStr), "%.0f m", config.altitude);
    canvas.drawString(altStr, 140, 28);
    canvas.drawString(state.settings_cursor == 1 ? "> Update Rate:" : "  Update Rate:", 10, 43);

    char intStr[16];
    snprintf(intStr, sizeof(intStr), "%d sec", config.update_interval);
    canvas.drawString(intStr, 140, 43);
    canvas.drawString(state.settings_cursor == 2 ? "> Sound Alerts:" : "  Sound Alerts:", 10, 58);
    canvas.drawString(config.sound_enabled ? "ON" : "OFF", 140, 58);
    canvas.drawString(state.settings_cursor == 3 ? "> Brightness:" : "  Brightness:", 10, 73);

    char brtStr[16];
    snprintf(brtStr, sizeof(brtStr), "%d", config.screen_brightness);
    canvas.drawString(brtStr, 140, 73);
    canvas.drawString(state.settings_cursor == 4 ? "> AutoDim (sec):" : "  AutoDim (sec):", 10, 85);

    char dimStr[16];
    snprintf(dimStr, sizeof(dimStr), "%d", config.auto_dim_timeout);
    canvas.drawString(dimStr, 140, 85);
    canvas.drawLine(10, 97, 230, 97, t.header);
    uint16_t vbat_mV = M5Cardputer.Power.getBatteryVoltage();

    char statStr[32];
    snprintf(statStr, sizeof(statStr), "Bat: %d mV", vbat_mV);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(vbat_mV < 3300 ? TFT_RED : TFT_GREEN);
    canvas.drawString(statStr, 120, 107);

    drawFooter("[,/]Change [;.]Nav [M]Save & Exit");
}
