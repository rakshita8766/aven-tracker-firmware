/*
 * @file    : tracker.c
 * @project : AVEN Tracker
 * @brief   : Core tracking logic - ignition, theft, modes
 * @date    : 2026-04-24
 */

#include "tracker.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* Buzzer pin - defined in CubeMX */
#define BUZZER_PORT    Buzzer_GPIO_Port
#define BUZZER_PIN     Buzzer_Pin

/* Movement threshold in meters */
#define MOVEMENT_THRESHOLD_M   20.0f

/* -------------------------------------------------------- */
void Tracker_Init(TrackerState_t *state)
/* -------------------------------------------------------- */
{
    memset(state, 0, sizeof(TrackerState_t));

    state->mode               = MODE_PARKING;
    state->ignition_on        = 0;
    state->theft_detected     = 0;
    state->mqtt_connected     = 1;        /* MQTT already connected in main */
    state->publish_interval   = 60000;    /* 60s */
    state->last_publish_time  = 0;
    state->last_lat           = 0.0f;
    state->last_lon           = 0.0f;

    printf("Tracker initialized - PARKING mode\r\n");
}

/* -------------------------------------------------------- */
void Tracker_ActivateBuzzer(uint8_t on)
/* -------------------------------------------------------- */
{
    if(on)
    {
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);
        printf("BUZZER ON\r\n");
    }
    else
    {
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
        printf("BUZZER OFF\r\n");
    }
}

/* -------------------------------------------------------- */
void Tracker_CheckIgnition(TrackerState_t *state)
/* -------------------------------------------------------- */
{
    GPIO_PinState pin = HAL_GPIO_ReadPin(BUTTON_SW1_GPIO_PORT, BUTTON_SW1_PIN);

    if(pin == GPIO_PIN_RESET)
    {
        /* IGNITION ON */
        if(state->ignition_on == 0)
        {
            state->ignition_on      = 1;
            state->mode             = MODE_LIVE_TRACKING;
            state->publish_interval = 10000; /* 10s */
            printf("IGNITION ON - Live tracking\r\n");
        }
    }
    else
    {
        /* IGNITION OFF */
        if(state->ignition_on == 1)
        {
            state->ignition_on      = 0;
            state->mode             = MODE_PARKING;
            state->publish_interval = 60000; /* 60s */
            printf("IGNITION OFF - Parking mode\r\n");

            /* Save parking location only if GPS valid */
            if(state->gps.fix_valid)
            {
                state->last_lat = state->gps.latitude;
                state->last_lon = state->gps.longitude;
                printf("Parking location saved\r\n");
            }
        }
    }
}

/* -------------------------------------------------------- */
static float Tracker_CalcDistance(float lat1, float lon1,
                                  float lat2, float lon2)
/* -------------------------------------------------------- */
{
    float dlat = (lat2 - lat1) * 111320.0f;
    float dlon = (lon2 - lon1) * 111320.0f * 0.85f;

    return sqrtf(dlat * dlat + dlon * dlon);
}

/* -------------------------------------------------------- */
void Tracker_CheckTheft(TrackerState_t *state)
/* -------------------------------------------------------- */
{
    if(state->mode != MODE_PARKING) return;
    if(state->gps.fix_valid == 0) return;
    if(state->last_lat == 0.0f && state->last_lon == 0.0f) return;

    float dist = Tracker_CalcDistance(
                     state->last_lat,
                     state->last_lon,
                     state->gps.latitude,
                     state->gps.longitude);

    printf("Distance from park: %.1f m\r\n", dist);

    if(dist > MOVEMENT_THRESHOLD_M)
    {
        if(state->theft_detected == 0)
        {
            state->theft_detected   = 1;
            state->mode             = MODE_THEFT;
            state->publish_interval = 5000; /* 5s */

            printf("THEFT DETECTED! Dist=%.1f m\r\n", dist);
            Tracker_ActivateBuzzer(1);
        }
    }
    else
    {
        if(state->theft_detected == 1)
        {
            state->theft_detected   = 0;
            state->mode             = MODE_PARKING;
            state->publish_interval = 60000;

            printf("Back to safe zone\r\n");
            Tracker_ActivateBuzzer(0);
        }
    }
}

/* -------------------------------------------------------- */
static void Tracker_BuildPayload(TrackerState_t *state,
                                 char *payload,
                                 uint16_t size)
/* -------------------------------------------------------- */
{
    const char *mode_str;

    switch(state->mode)
    {
        case MODE_LIVE_TRACKING: mode_str = "live"; break;
        case MODE_PARKING:       mode_str = "parking"; break;
        case MODE_THEFT:         mode_str = "THEFT"; break;
        default:                 mode_str = "unknown"; break;
    }

    if(state->gps.fix_valid)
    {
        int lat_d = (int)state->gps.latitude;
        int lat_f = (int)((state->gps.latitude - lat_d) * 1000000);
        if(lat_f < 0) lat_f = -lat_f;

        int lon_d = (int)state->gps.longitude;
        int lon_f = (int)((state->gps.longitude - lon_d) * 1000000);
        if(lon_f < 0) lon_f = -lon_f;

        int spd_d = (int)state->gps.speed_kmh;
        int spd_f = (int)((state->gps.speed_kmh - spd_d) * 10);
        if(spd_f < 0) spd_f = -spd_f;

        snprintf(payload, size,
            "{\"device\":\"AVEN_001\","
            "\"mode\":\"%s\","
            "\"lat\":%d.%06d,"
            "\"lon\":%d.%06d,"
            "\"spd\":%d.%01d,"
            "\"ign\":%d,"
            "\"theft\":%d,"
            "\"fix\":1,"
            "\"ts\":\"%s\"}",
            mode_str,
            lat_d, lat_f,
            lon_d, lon_f,
            spd_d, spd_f,
            state->ignition_on,
            state->theft_detected,
            state->gps.timestamp);
    }
    else
    {
        snprintf(payload, size,
            "{\"device\":\"AVEN_001\","
            "\"mode\":\"%s\","
            "\"fix\":0,"
            "\"ign\":%d,"
            "\"theft\":%d}",
            mode_str,
            state->ignition_on,
            state->theft_detected);
    }
}

/* -------------------------------------------------------- */
void Tracker_Run(TrackerState_t *state,
                 UART_HandleTypeDef *huart)
/* -------------------------------------------------------- */
{
    uint32_t now = HAL_GetTick();
    char payload[256];
    char alert[256];

    /* 1. Ignition */
    Tracker_CheckIgnition(state);

    /* 2. GPS */
    if(GNSS_GetLocation(huart, &state->gps) != AVEN_OK)
    {
        printf("GPS not ready\r\n");
    }

    /* 3. Theft detection */
    Tracker_CheckTheft(state);

    /* 4. Periodic publish */
    if((now - state->last_publish_time) >= state->publish_interval)
    {
        state->last_publish_time = now;

        Tracker_BuildPayload(state, payload, sizeof(payload));
        printf("Mode:%d Payload:%s\r\n", state->mode, payload);

        if(state->mqtt_connected)
        {
            if(LTE_PublishData(huart, MQTT_TOPIC_GPS, payload) == AVEN_OK)
            {
                printf("Published OK\r\n");
            }
            else
            {
                printf("Publish failed\r\n");
                state->mqtt_connected = 0;
            }
        }

        /* Theft alert */
        if(state->theft_detected)
        {
            snprintf(alert, sizeof(alert),
                "{\"device\":\"AVEN_001\","
                "\"alert\":\"THEFT_DETECTED\","
                "\"ign\":%d}",
                state->ignition_on);

            LTE_PublishData(huart, MQTT_TOPIC_ALERT, alert);
            printf("ALERT SENT\r\n");
        }
    }

    /* 5. MQTT health */
    if(LTE_CheckMQTTAlive(huart) != AVEN_OK)
    {
        printf("MQTT dropped, reconnecting\r\n");

        if(LTE_ConnectMQTT(huart) == AVEN_OK)
        {
            state->mqtt_connected = 1;
            printf("MQTT reconnected\r\n");
        }
        else
        {
            state->mqtt_connected = 0;
            printf("MQTT reconnect failed\r\n");
        }
    }
}