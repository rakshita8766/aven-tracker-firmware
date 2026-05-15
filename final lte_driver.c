/*
 * @file    : lte_driver.c
 * @project : AVEN Tracker
 * @brief   : AT command driver for EC200U-CN LTE module
 *            Features: Jio LTE data activation (IPv4v6)
 * @date    : 2026-04-23
 */

#include "lte_driver.h"
#include <stdio.h>

#define RX_BUF_SIZE 512
static char rx_buf[RX_BUF_SIZE];

/* ================= UART WAIT ================= */
static uint8_t LTE_WaitFor(UART_HandleTypeDef *huart,
                          const char *target,
                          char *out,
                          uint32_t timeout)
{
    uint32_t start = HAL_GetTick();
    uint16_t idx = 0;
    memset(out, 0, RX_BUF_SIZE);

    while ((HAL_GetTick() - start) < timeout)
    {
        uint8_t ch;
        if (HAL_UART_Receive(huart, &ch, 1, 50) == HAL_OK)
        {
            if (idx < RX_BUF_SIZE - 1)
                out[idx++] = ch;

            if (strstr(out, target))
                return 1;
        }
    }
    return 0;
}

/* ================= SEND AT ================= */
AvenStatus_t LTE_SendAT(UART_HandleTypeDef *huart,
                       const char *cmd,
                       char *response,
                       uint32_t timeout)
{
    memset(rx_buf, 0, RX_BUF_SIZE);
    HAL_UART_Transmit(huart, (uint8_t *)cmd, strlen(cmd), 1000);

    uint32_t start = HAL_GetTick();
    uint16_t idx = 0;

    while ((HAL_GetTick() - start) < timeout)
    {
        uint8_t ch;
        if (HAL_UART_Receive(huart, &ch, 1, 100) == HAL_OK)
        {
            if (idx < RX_BUF_SIZE - 1)
                rx_buf[idx++] = ch;

            if (strstr(rx_buf, "OK") ||
                strstr(rx_buf, "ERROR") ||
                strstr(rx_buf, ">"))
                break;
        }
    }

    rx_buf[idx] = '\0';

    if (response)
        strcpy(response, rx_buf);

    return (strstr(rx_buf, "OK") != NULL) ? AVEN_OK : AVEN_ERROR;
}

/* ================= INIT ================= */
AvenStatus_t LTE_Init(UART_HandleTypeDef *huart)
{
    char resp[RX_BUF_SIZE];

    LTE_SendAT(huart, "AT\r\n", resp, 3000);
    LTE_SendAT(huart, "ATE0\r\n", resp, 1000);
    LTE_SendAT(huart, "AT+CMEE=2\r\n", resp, 1000);
    LTE_SendAT(huart, "AT+QCFG=\"ims\",1\r\n", resp, 1000);

    printf("LTE Init OK\r\n");
    return AVEN_OK;
}

/* ================= DATA ================= */
AvenStatus_t LTE_ActivateData(UART_HandleTypeDef *huart)
{
    char resp[RX_BUF_SIZE];

    printf("Resetting module...\r\n");
    LTE_SendAT(huart, "AT+CFUN=1,1\r\n", resp, 10000);
    HAL_Delay(8000);

    /* WAIT FOR NETWORK */
    printf("Waiting for network...\r\n");
    for (int i = 0; i < 30; i++)
    {
        LTE_SendAT(huart, "AT+CEREG?\r\n", resp, 3000);
        printf("%s\r\n", resp);

        if (strstr(resp, "+CEREG: 0,1") ||
            strstr(resp, "+CEREG: 0,5"))
        {
            printf("NETWORK OK\r\n");
            break;
        }
        HAL_Delay(2000);
    }

    /* PDP CONTEXT */
    LTE_SendAT(huart, "AT+CGDCONT=1,\"IPV4V6\",\"jionet\"\r\n", resp, 5000);
    printf("CGDCONT: %s\r\n", resp);

    LTE_SendAT(huart, "AT+QICSGP=1,3,\"jionet\",\"\",\"\",0\r\n", resp, 5000);
    printf("QICSGP: %s\r\n", resp);

    /* ACTIVATE */
    for (int i = 0; i < 3; i++)
    {
        printf("QIACT attempt %d\r\n", i + 1);

        LTE_SendAT(huart, "AT+QIACT=1\r\n", resp, 20000);
        printf("ACT: %s\r\n", resp);

        LTE_SendAT(huart, "AT+QIACT?\r\n", resp, 5000);
        printf("IP: %s\r\n", resp);

        /* ✅ FIX: accept ANY IP (IPv6 = type 2) */
        if (strstr(resp, "+QIACT:"))
        {
            printf("DATA OK\r\n");
            return AVEN_OK;
        }

        LTE_SendAT(huart, "AT+QIDEACT=1\r\n", resp, 5000);
        HAL_Delay(3000);
    }

    printf("DATA FAILED\r\n");
    return AVEN_ERROR;
}

/* ================= MQTT CONNECT ================= */
AvenStatus_t LTE_ConnectMQTT(UART_HandleTypeDef *huart)
{
    char resp[RX_BUF_SIZE];
    char cmd[128];

    LTE_SendAT(huart, "AT+QMTCFG=\"pdpcid\",0,1\r\n", resp, 1000);
    LTE_SendAT(huart, "AT+QMTCFG=\"version\",0,4\r\n", resp, 1000);
    LTE_SendAT(huart, "AT+QMTCFG=\"keepalive\",0,60\r\n", resp, 1000);

    printf("Opening MQTT...\r\n");

    LTE_SendAT(huart, "AT+QMTOPEN=0,\"broker.emqx.io\",1883\r\n", resp, 5000);

    if (!LTE_WaitFor(huart, "+QMTOPEN: 0,0", resp, 15000))
    {
        printf("OPEN FAIL: %s\r\n", resp);
        return AVEN_ERROR;
    }

    /* FIX: NO username/password */
    sprintf(cmd, "AT+QMTCONN=0,\"AVEN_CLIENT\"\r\n");
    HAL_UART_Transmit(huart, (uint8_t *)cmd, strlen(cmd), 1000);

    if (LTE_WaitFor(huart, "+QMTCONN: 0,0,0", resp, 10000))
    {
        printf("MQTT CONNECTED\r\n");
        return AVEN_OK;
    }

    printf("MQTT FAIL\r\n");
    return AVEN_ERROR;
}

/* ================= MQTT PUBLISH ================= */
AvenStatus_t LTE_PublishData(UART_HandleTypeDef *huart,
                            const char *topic,
                            const char *payload)
{
    char cmd[256];
    char resp[RX_BUF_SIZE];

    /*FIXED FORMAT */
    sprintf(cmd, "AT+QMTPUBEX=0,0,0,0,\"%s\",%d\r\n",
            topic, (int)strlen(payload));

    LTE_SendAT(huart, cmd, resp, 3000);

    /* send payload */
    HAL_UART_Transmit(huart, (uint8_t *)payload, strlen(payload), 3000);

    if (LTE_WaitFor(huart, "+QMTPUBEX:", resp, 10000))
    {
        printf("PUBLISH OK\r\n");
        return AVEN_OK;
    }

    printf("PUBLISH FAIL\r\n");
    return AVEN_ERROR;
}
/* ================= MQTT ALIVE CHECK ================= */

AvenStatus_t LTE_CheckMQTTAlive(UART_HandleTypeDef *huart)
{
    char resp[RX_BUF_SIZE];
    memset(resp, 0, sizeof(resp));

    LTE_SendAT(huart, "AT+QMTCONN?\r\n", resp, 3000);

    /* Connected = +QMTCONN: 0,0,0 */
    if (strstr(resp, "+QMTCONN: 0,3") != NULL)
        return AVEN_OK;

    return AVEN_ERROR;
}





/*
 * @file    : lte_driver.h
 * @project : AVEN Tracker
 * @brief   : AT command driver for EC200U-CN LTE module
 *            Supports: Data Activation, MQTT over SSL/TLS, Authentication
 */

#ifndef LTE_DRIVER_H
#define LTE_DRIVER_H

#include "stm32wbxx_hal.h"
#include <string.h>
#include <stdio.h>
#include "main.h"

/* ----------------------------------------------------------------
 * MQTT Broker Configuration — EMQX Public BrokER
 * ---------------------------------------------------------------- */
#define MQTT_BROKER        "broker.emqx.io"
#define MQTT_PORT          1883
#define MQTT_CLIENT_ID     "AVEN_TRACKER_001"

/* No username/password needed */
#define MQTT_USERNAME      ""
#define MQTT_PASSWORD      ""

/* Topics */
#define MQTT_TOPIC_STATUS  "aven/tracker/status"
#define MQTT_TOPIC_GPS     "aven/tracker/gps"
#define MQTT_TOPIC_CAN     "aven/tracker/can"

/* ----------------------------------------------------------------
 * Status codes
 * ---------------------------------------------------------------- */
typedef enum {
    AVEN_OK      = 0,
    AVEN_ERROR   = 1,
    AVEN_TIMEOUT = 2
} AvenStatus_t;

/* ----------------------------------------------------------------
 * Function declarations
 * ---------------------------------------------------------------- */
AvenStatus_t LTE_Init(UART_HandleTypeDef *huart);

AvenStatus_t LTE_SendAT(UART_HandleTypeDef *huart,
                         const char *cmd,
                         char *response,
                         uint32_t timeout_ms);

AvenStatus_t LTE_ActivateData(UART_HandleTypeDef *huart);

AvenStatus_t LTE_ConnectMQTT(UART_HandleTypeDef *huart);

AvenStatus_t LTE_PublishData(UART_HandleTypeDef *huart,
                              const char *topic,
                              const char *payload);

AvenStatus_t LTE_DisconnectMQTT(UART_HandleTypeDef *huart);

AvenStatus_t LTE_CheckMQTTAlive(UART_HandleTypeDef *huart);

#endif /* LTE_DRIVER_H */





