#include "common.h"

const char *predictWeather(float current_slp, int trend)
{
    if (trend == -1)
    {
        if (current_slp < 995.0f)
            return "SEVERE STORM";
        if (current_slp < 1005.0f)
            return "STORMY / HEAVY RAIN";
        if (current_slp < 1013.0f)
            return "RAIN / WIND INCOMING";
        return "UNSETTLED / RAIN POSSIBLE";
    }
    if (trend == 1)
    {
        if (current_slp > 1025.0f)
            return "VERY FAIR / DRY";
        if (current_slp > 1018.0f)
            return "FAIR / IMPROVING";
        if (current_slp > 1010.0f)
            return "BECOMING FAIR";
        return "SLOW IMPROVEMENT";
    }
    if (current_slp > 1025.0f)
        return "STABLE / VERY FAIR";
    if (current_slp > 1018.0f)
        return "FAIR / STABLE";
    if (current_slp > 1010.0f)
        return "PARTLY CLOUDY";
    if (current_slp > 1000.0f)
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
        telemetry.pressure = bmp.readPressure() / 100.0f;
        telemetry.temperature = bmp.readTemperature();
        float tk = telemetry.temperature + 273.15f;
        float slf = pow(1.0f + (0.0065f * config.altitude) / tk, 5.2553f);
        telemetry.slp = telemetry.pressure * slf;
        if (telemetry.temperature < -50.0f || telemetry.temperature > 60.0f)
            telemetry.slp = telemetry.pressure / pow(1.0f - (config.altitude / 44330.0f), 5.255f);
    }
}

void updateTrend(float current_slp)
{
    if (telemetry.slp_history == 0.0f)
    {
        telemetry.trendStartTime = millis();
    }

    if (millis() - telemetry.trendTimer > 300000UL || telemetry.slp_history == 0.0f)
    {
        if (telemetry.slp_history != 0.0f)
        {
            float diff = current_slp - telemetry.slp_history;
            int old_trend = telemetry.pressure_trend;

            unsigned long elapsed = millis() - telemetry.trendStartTime;
            float threshold;
            if (elapsed < 600000UL)
                threshold = 2.0f;
            else if (elapsed < 1800000UL)
                threshold = 1.5f;
            else if (elapsed < 3600000UL)
                threshold = 1.0f;
            else
                threshold = 0.7f;

            telemetry.pressure_trend = (diff >= threshold) ? 1 : (diff <= -threshold) ? -1
                                                                                      : 0;

            if (telemetry.pressure_trend != 0)
            {
                if (telemetry.trend_consistent == telemetry.pressure_trend)
                    telemetry.trend_samples++;
                else
                    telemetry.trend_consistent = telemetry.trend_samples = telemetry.pressure_trend;
                if (elapsed < 600000UL)
                    telemetry.trend_samples = 0;
                else if (telemetry.trend_samples >= 2)
                    telemetry.trend_ready = true;
            }
            else
            {
                telemetry.trend_consistent = telemetry.trend_samples = 0;
                telemetry.trend_ready = false;
            }
            if (telemetry.pressure_trend != old_trend && telemetry.pressure_trend != 0)
                playTone(telemetry.pressure_trend);
        }
        telemetry.slp_history = current_slp;
        telemetry.trendTimer = millis();
    }
}
