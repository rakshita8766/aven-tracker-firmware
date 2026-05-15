/*
 * @file    : lte_driver.c
 * @project : AVEN Tracker
 * @brief   : AT command driver for EC200U-CN LTE module
 * @author  : [Your Name]
 * @date    : 2026-04-20
 */

#include "lte_driver.h"
#include <stdio.h>

#define RX_BUF_SIZE  256

static char rx_buf[RX_BUF_SIZE];

/* --------------------------------------------------------
 * LTE_SendAT
 * Send an AT command and wait for response
 * -------------------------------------------------------- */
AvenStatus_t LTE_SendAT(UART_HandleTypeDef *huart,
                        const char *cmd,
                        char *response,
                        uint32_t timeout_ms)
{
    memset(rx_buf, 0, RX_BUF_SIZE);

    /* Send command */
    HAL_UART_Transmit(huart, (uint8_t *)cmd, strlen(cmd), timeout_ms);

    uint32_t start = HAL_GetTick();
    uint16_t idx = 0;

    /* Receive response byte-by-byte */
    while ((HAL_GetTick() - start) < timeout_ms)
    {
        uint8_t ch;

        if (HAL_UART_Receive(huart, &ch, 1, 100) == HAL_OK)
        {
            if (idx < RX_BUF_SIZE - 1)
            {
                rx_buf[idx++] = ch;
            }

            /* Stop when response ends */
            if (strstr(rx_buf, "OK") != NULL ||
                strstr(rx_buf, "ERROR") != NULL)
            {
                break;
            }
        }
    }

    /* Null terminate string (VERY IMPORTANT) */
    rx_buf[idx] = '\0';

    /* Copy response */
    if (response != NULL)
    {
        strncpy(response, rx_buf, RX_BUF_SIZE - 1);
        response[RX_BUF_SIZE - 1] = '\0';
    }

    /* Check result */
    if (strstr(rx_buf, "OK") != NULL)
    {
        return AVEN_OK;
    }

    return AVEN_ERROR;
}

/* --------------------------------------------------------
 * LTE_Init
 * Initialize and verify EC200U-CN module
 * -------------------------------------------------------- */
AvenStatus_t LTE_Init(UART_HandleTypeDef *huart)
{
    char response[RX_BUF_SIZE];

    /* Step 1: Basic alive check */
    if (LTE_SendAT(huart, "AT\r\n", response, 1000) != AVEN_OK) {
        return AVEN_ERROR;
    }

    /* Step 2: Disable echo */
    LTE_SendAT(huart, "ATE0\r\n", response, 1000);

    /* Step 3: Check SIM present */
    if (LTE_SendAT(huart, "AT+CIMI\r\n", response, 2000) != AVEN_OK) {
        return AVEN_ERROR;
    }
    /* --- ADD THIS STEP FOR JIO --- */
    /* Step 4: Enable IMS/VoLTE (Crucial for Jio data services) */
    LTE_SendAT(huart, "AT+QCFG=\"ims\",1\r\n", response, 1000);

    return AVEN_OK;
}

AvenStatus_t LTE_ActivateData(UART_HandleTypeDef *huart)
{
    char response[256];

    /* Step 0: Force close any existing session */
    printf("Closing old session...\r\n");
    LTE_SendAT(huart, "AT+QIDEACT=1\r\n", response, 10000);
    HAL_Delay(2000);

    /* Step 1: Force LTE only mode (Essential for Jio) */
    printf("Setting LTE only mode...\r\n");
    LTE_SendAT(huart, "AT+QCFG=\"nwscanmode\",3,1\r\n", response, 2000);
    HAL_Delay(3000); /* Extra time to re-register on LTE */

    /* Step 2: Check LTE registration (Jio is LTE only) */
    memset(response, 0, sizeof(response));
    LTE_SendAT(huart, "AT+CEREG?\r\n", response, 2000);
    printf("CEREG: %s\r\n", response); /* Must show 0,1 or 0,5 */

    /* Step 3: Set APN for Jio */
    memset(response, 0, sizeof(response));
    LTE_SendAT(huart, "AT+CGDCONT=1,\"IP\",\"jionet\"\r\n", response, 5000);
    printf("Jio APN Set: %s\r\n", response);
    HAL_Delay(1000);

    /* Step 4: Set QICSGP with NO AUTH (0 not 1 — Jio does not use PAP) */
    memset(response, 0, sizeof(response));
    LTE_SendAT(huart, "AT+QICSGP=1,1,\"jionet\",\"\",\"\",0\r\n", response, 5000);
    printf("QICSGP: %s\r\n", response);
    HAL_Delay(1000);

    /* Step 5: Set DNS */
    LTE_SendAT(huart, "AT+QIDNSCFG=1,\"8.8.8.8\",\"8.8.4.4\"\r\n", response, 1000);

    /* Step 6: Activate data */
    memset(response, 0, sizeof(response));
    LTE_SendAT(huart, "AT+QIACT=1\r\n", response, 15000); /* 15s timeout */
    printf("Activation: %s\r\n", response);
    HAL_Delay(3000);

    /* Step 7: Read error code if activation failed */
    memset(response, 0, sizeof(response));
    LTE_SendAT(huart, "AT+QIGETERROR\r\n", response, 3000);
    printf("Error detail: %s\r\n", response);

    /* Step 8: Check IP */
    memset(response, 0, sizeof(response));
    LTE_SendAT(huart, "AT+QIACT?\r\n", response, 5000);
    printf("IP Check: %s\r\n", response);

    /* If we got an IP address = success */
    if(strstr(response, ":") != NULL)
    {
        printf("JIO DATA ACTIVE OK\r\n");
        return AVEN_OK;
    }
    return AVEN_ERROR;
}

AvenStatus_t LTE_PublishData(UART_HandleTypeDef *huart, const char *topic, const char *payload)
{
    char cmd[256];
    char response[256];

    /* Build publish command */
    snprintf(cmd, sizeof(cmd), "AT+QMTPUBEX=0,0,0,\"%s\",%d\r\n", topic, (int)strlen(payload));

    /* Send publish command */
    memset(response, 0, sizeof(response));
    LTE_SendAT(huart, cmd, response, 5000);
    printf("Pub cmd: %s\r\n", response);
    HAL_Delay(1000);

    /* Send payload */
    memset(response, 0, sizeof(response));
    HAL_UART_Transmit(huart, (uint8_t *)payload, strlen(payload), 3000);

    /* Wait for response */
    HAL_Delay(2000);
    HAL_UART_Receive(huart, (uint8_t *)response, 255, 5000);
    printf("Pub result: %s\r\n", response);

    if(strstr(response, "OK") != NULL || strstr(response, "+QMTPUBEX") != NULL)
    {
        printf("PUBLISHED OK\r\n");
        return AVEN_OK;
    }

    return AVEN_ERROR;
}
AvenStatus_t LTE_ConnectMQTT(UART_HandleTypeDef *huart)
{
    char response[256];
    char cmd[256];
    /*Step 0: Link MQTT to JIO context*/
    LTE_SendAT(huart, "AT+QMTCFG=\"pdpcid\",0,1\r\n", response, 1000);

    /* Step 1: Set MQTT version */
    LTE_SendAT(huart, "AT+QMTCFG=\"version\",0,4\r\n", response, 1000);

    /* Step 2: Open connection TO HiveMQ */
    memset(response, 0, sizeof(response));
    LTE_SendAT(huart, "AT+QMTOPEN=0,\"broker.hivemq.com\",1883\r\n", response, 15000);

    /* Wait for +QMTOPEN: 0,0 */
    HAL_UART_Receive(huart, (uint8_t *)response, 255, 10000);
    printf("HiveMQ Open: %s\r\n", response);

    /* Step 3: Connect */
    memset(response, 0, sizeof(response));
    snprintf(cmd, sizeof(cmd), "AT+QMTCONN=0,\"AVEN_TRACKER%lu\"\r\n", HAL_GetTick());
    LTE_SendAT(huart, cmd, response, 10000);

    /* Wait for +QMTCONN: 0,0,0 response */
    HAL_UART_Receive(huart, (uint8_t *)response, 255, 5000);

    /* Check connected */
    if(strstr(response, "+QMTCONN: 0,0,0") != NULL)
    {
        printf("HIVEMQ CONNECTED\r\n");
        return AVEN_OK;
    }

    return AVEN_ERROR;
}

