#include "common.h"
#include <SPI.h>
#include <SD.h>

static SPIClass sdSPI(HSPI);
static bool sd_inited = false;
static bool sd_available = false;

void toggleSDLogging()
{
    config.sd_logging = !config.sd_logging;
    prefs.begin("weather_app", false);
    prefs.putBool("sdlog", config.sd_logging);
    prefs.end();
    if (!config.sd_logging)
    {
        sd_inited = false;
        sd_available = false;
    }
    playTone(config.sd_logging ? 1 : -1);
    state.redraw = true;
}

// --- Zambretti-style forecaster (pressure + 3h tendency only) ---
// Returns a code 0..26 where higher values mean worse weather.
// Constants adapted from the classic Zambretti algorithm.
int computeZambretti(float slp, int trend, float rate3h)
{
    float P = slp;
    float Z;
    if (trend < 0)
        Z = 130.0f - 0.120f * P;
    else if (trend > 0)
        Z = 127.0f - 0.120f * P;
    else
        Z = 144.0f - 0.130f * P;
    // Rate-of-change bias (hPa over 3h): a fast fall worsens, a fast rise
    // improves the outlook beyond what the sign alone captures.
    Z += -rate3h * 1.5f;
    if (Z < 0.0f)
        Z = 0.0f;
    if (Z > 26.0f)
        Z = 26.0f;
    return (int)(Z + 0.5f);
}

const char *zambrettiLabel(int z)
{
    if (z < 0)
        return "----";
    if (z <= 8)
        return "FAIR";
    if (z <= 16)
        return "CHANGE";
    return "FOUL";
}

const char *predictWeather(float current_slp, int trend)
{
    // 3-hour tendency (hPa) from the pressure history ring buffer.
    float rate3h = telemetry.slp_rate * 3.0f;
    // Effective pressure: emphasise the tendency so the forecast reacts to
    // how fast pressure is moving, not only its direction.
    float eff = current_slp + rate3h * 2.0f;

    if (trend == -1)
    {
        if (eff < 995.0f)
            return "SEVERE STORM";
        if (eff < 1005.0f)
            return "STORMY / HEAVY RAIN";
        if (eff < 1013.0f)
            return "RAIN / WIND INCOMING";
        return "UNSETTLED / RAIN POSSIBLE";
    }
    if (trend == 1)
    {
        if (eff > 1025.0f)
            return "VERY FAIR / DRY";
        if (eff > 1018.0f)
            return "FAIR / IMPROVING";
        if (eff > 1010.0f)
            return "BECOMING FAIR";
        return "SLOW IMPROVEMENT";
    }
    if (eff > 1025.0f)
        return "STABLE / VERY FAIR";
    if (eff > 1018.0f)
        return "FAIR / STABLE";
    if (eff > 1010.0f)
        return "PARTLY CLOUDY";
    if (eff > 1000.0f)
        return "CLOUDY / STABLE";
    return "STEADY RAIN / LOW PRESSURE";
}

void readSensors()
{
    if (aht_found)
    {
        sensors_event_t h, t;
        aht.getEvent(&h, &t);
        telemetry.humidity = h.relative_humidity;
        if (!bmp_found)
            telemetry.temperature = t.temperature;
    }
    if (bmp_found)
    {
        // FORCED mode: trigger one conversion on demand, then the sensor
        bmp.takeForcedMeasurement();
        telemetry.pressure = bmp.readPressure() / 100.0f;
        telemetry.temperature = bmp.readTemperature();

        float tk = telemetry.temperature + 273.15f;
        float slf = pow(1.0f + (0.0065f * config.altitude) / tk, 5.25588f);
        telemetry.slp = telemetry.pressure * slf;
        // Fallback to the isothermal standard-atmosphere model if the temp
        // reading is clearly bogus.
        if (telemetry.temperature < -50.0f || telemetry.temperature > 60.0f)
            telemetry.slp = telemetry.pressure / pow(1.0f - (config.altitude / 44330.0f), 5.255f);
    }
}

// Push a sample into the SLP ring buffer and recompute rate / min / max / avg.
void updateHistory(float current_slp)
{
    telemetry.slp_buf[telemetry.hist_head] = current_slp;
    telemetry.slp_time[telemetry.hist_head] = millis();
    telemetry.hist_head = (telemetry.hist_head + 1) % Telemetry::HISTORY_SIZE;
    if (telemetry.hist_count < Telemetry::HISTORY_SIZE)
        telemetry.hist_count++;

    if (telemetry.hist_count < 2)
    {
        telemetry.slp_rate = 0.0f;
        telemetry.slp_min = telemetry.slp_max = telemetry.slp_avg = current_slp;
        return;
    }

    // Least-squares slope of SLP vs time (hours) over the available history.
    float sumX = 0.0f, sumY = 0.0f, sumXY = 0.0f, sumX2 = 0.0f;
    float mn = current_slp, mx = current_slp, s = 0.0f;
    int n = telemetry.hist_count;
    int start = (telemetry.hist_count < Telemetry::HISTORY_SIZE) ? 0 : telemetry.hist_head;
    for (int i = 0; i < n; i++)
    {
        int idx = (start + i) % Telemetry::HISTORY_SIZE;
        float x = (float)telemetry.slp_time[idx] / 3600000.0f;
        float y = telemetry.slp_buf[idx];
        sumX += x;
        sumY += y;
        sumXY += x * y;
        sumX2 += x * x;
        if (y < mn)
            mn = y;
        if (y > mx)
            mx = y;
        s += y;
    }
    float denom = n * sumX2 - sumX * sumX;
    if (denom > 1e-6f || denom < -1e-6f)
        telemetry.slp_rate = (n * sumXY - sumX * sumY) / denom; // hPa / hour
    telemetry.slp_min = mn;
    telemetry.slp_max = mx;
    telemetry.slp_avg = s / n;
}

void updateTrend(float current_slp)
{
    if (telemetry.slp_history == 0.0f)
    {
        telemetry.slp_history = current_slp;
        telemetry.trendStartTime = millis();
        telemetry.trendTimer = millis();
        telemetry.pressure_trend = 0;
        updateHistory(current_slp); // baseline sample for the ring buffer
    }
    else if (millis() - telemetry.trendTimer > 300000UL)
    {
        float diff = current_slp - telemetry.slp_history;
        int old_trend = telemetry.pressure_trend;

        unsigned long elapsed = millis() - telemetry.trendStartTime;
        float threshold;
        if (elapsed < 600000UL)
            threshold = 1.5f;
        else if (elapsed < 1800000UL)
            threshold = 1.0f;
        else if (elapsed < 3600000UL)
            threshold = 0.7f;
        else
            threshold = 0.5f;

        int new_trend = (diff >= threshold) ? 1 : (diff <= -threshold) ? -1
                                                                       : 0;

        if (telemetry.trend_samples < 999)
            telemetry.trend_samples++;
        if (new_trend != 0 && new_trend == telemetry.pressure_trend)
        {
            if (telemetry.trend_consistent < 999)
                telemetry.trend_consistent++;
        }
        else if (new_trend == 0)
            telemetry.trend_consistent = 0;
        else
            telemetry.trend_consistent = 1;
        telemetry.trend_ready = (telemetry.trend_samples >= 6) && (telemetry.trend_consistent >= 3);

        telemetry.pressure_trend = new_trend;

        if (telemetry.pressure_trend != old_trend && telemetry.pressure_trend != 0)
            playTone(telemetry.pressure_trend);

        updateHistory(current_slp);

        if (elapsed > 7200000UL)
        {
            telemetry.slp_history = current_slp;
            telemetry.trendStartTime = millis();
        }

        telemetry.trendTimer = millis();
    }

    telemetry.zambretti = computeZambretti(current_slp, telemetry.pressure_trend,
                                           telemetry.slp_rate * 3.0f);
}

void logToSD()
{
    if (!config.sd_logging)
        return;

    unsigned long interval = (unsigned long)SD_LOG_INTERVAL_MIN * 60UL * 1000UL;
    if (config.last_sd_log != 0 && (millis() - config.last_sd_log) < interval)
        return;
    config.last_sd_log = millis();

    if (!sd_inited)
    {
        sdSPI.begin(40, 39, 14, 12); // SCK, MISO, MOSI, CS (Cardputer SD slot)
        sd_available = SD.begin(12, sdSPI, 25000000);
        sd_inited = true;
    }
    if (!sd_available)
        return;

    File f = SD.open("/meteo.csv", FILE_APPEND);
    if (!f)
        return;

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
