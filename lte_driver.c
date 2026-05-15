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
    HAL_UART_Transmit(huart, (uint8_t *)cmd,
                      strlen(cmd), timeout_ms);

    /* Receive response */
    HAL_UART_Receive(huart, (uint8_t *)rx_buf,
                     RX_BUF_SIZE - 1, timeout_ms);

    /* Copy to caller buffer if provided */
    if (response != NULL) {
        strncpy(response, rx_buf, RX_BUF_SIZE - 1);
    }

    /* Check for OK */
    if (strstr(rx_buf, "OK") != NULL) {
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
    if (LTE_SendAT(huart, "AT\r\n",
                   response, 1000) != AVEN_OK) {
        return AVEN_ERROR;
    }

    /* Step 2: Disable echo */
    LTE_SendAT(huart, "ATE0\r\n", response, 1000);

    /* Step 3: Check SIM present */
    if (LTE_SendAT(huart, "AT+CIMI\r\n",
                   response, 2000) != AVEN_OK) {
        return AVEN_ERROR;
    }

    return AVEN_OK;
}

AvenStatus_t LTE_ActivateData(UART_HandleTypeDef *huart)
{
    char response[256];

    /* Step 1: Close any existing connection */
    memset(response, 0, sizeof(response));
    LTE_SendAT(huart, "AT+QIACT=0\r\n",
               response, 5000);
    printf("Deact: %s\r\n", response);
    HAL_Delay(2000);

    /* Step 2: Set APN for Jio */
    memset(response, 0, sizeof(response));
    LTE_SendAT(huart,
        "AT+CGDCONT=1,\"IP\",\"jionet\"\r\n",
        response, 5000);
    printf("CGDCONT: %s\r\n", response);
    HAL_Delay(1000);

    /* Step 3: Activate PDP */
    memset(response, 0, sizeof(response));
    LTE_SendAT(huart, "AT+CGACT=1,1\r\n",
               response, 15000);
    printf("CGACT: %s\r\n", response);
    HAL_Delay(3000);

    /* Step 4: Check IP address */
    memset(response, 0, sizeof(response));
    LTE_SendAT(huart, "AT+CGPADDR=1\r\n",
               response, 5000);
    printf("IP: %s\r\n", response);
    HAL_Delay(1000);

    /* If we got an IP address = success */
    if(strstr(response, "+CGPADDR:") != NULL)
    {
        printf("DATA ACTIVE OK\r\n");
        return AVEN_OK;
    }

    /* Step 5: Try alternate method */
    memset(response, 0, sizeof(response));
    LTE_SendAT(huart,
        "AT+QICSGP=1,1,\"jionet\",\"\",\"\",0\r\n",
        response, 5000);
    printf("QICSGP: %s\r\n", response);
    HAL_Delay(1000);

    memset(response, 0, sizeof(response));
    LTE_SendAT(huart, "AT+QIACT=1\r\n",
               response, 15000);
    printf("QIACT: %s\r\n", response);
    HAL_Delay(5000);

    memset(response, 0, sizeof(response));
    LTE_SendAT(huart, "AT+QIACT?\r\n",
               response, 5000);
    printf("QIACT check: %s\r\n", response);

    if(strstr(response, "+QIACT:") != NULL)
    {
        printf("DATA ACTIVE OK\r\n");
        return AVEN_OK;
    }

    return AVEN_ERROR;
}

AvenStatus_t LTE_PublishData(UART_HandleTypeDef *huart,
                              const char *topic,
                              const char *payload)
{
    char cmd[256];
    char response[256];

    /* Build publish command */
    snprintf(cmd, sizeof(cmd),
        "AT+QMTPUBEX=0,0,0,\"%s\",%d\r\n",
        topic, (int)strlen(payload));

    /* Send publish command */
    memset(response, 0, sizeof(response));
    LTE_SendAT(huart, cmd, response, 5000);
    printf("Pub cmd: %s\r\n", response);
    HAL_Delay(1000);

    /* Send payload */
    memset(response, 0, sizeof(response));
    HAL_UART_Transmit(huart,
        (uint8_t *)payload,
        strlen(payload), 3000);

    /* Wait for response */
    HAL_Delay(2000);
    HAL_UART_Receive(huart,
        (uint8_t *)response,
        255, 5000);
    printf("Pub result: %s\r\n", response);

    if(strstr(response, "OK") != NULL ||
       strstr(response, "+QMTPUBEX") != NULL)
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

    /* Step 1: Force close */
    memset(response, 0, sizeof(response));
    LTE_SendAT(huart, "AT+QMTCLOSE=0\r\n",
               response, 5000);
    HAL_Delay(3000);

    /* Step 2: Open fresh connection */
    memset(response, 0, sizeof(response));
    LTE_SendAT(huart,
        "AT+QMTOPEN=0,\"broker.hivemq.com\",1883\r\n",
        response, 15000);
    printf("Open result: %s\r\n", response);
    HAL_Delay(5000);

    /* Wait for +QMTOPEN: 0,0 */
    memset(response, 0, sizeof(response));
    HAL_UART_Receive(huart,
                     (uint8_t *)response,
                     255, 8000);
    printf("Open URC: %s\r\n", response);
    HAL_Delay(1000);

    /* Step 3: Connect */
    memset(response, 0, sizeof(response));
    snprintf(cmd, sizeof(cmd),
        "AT+QMTCONN=0,\"AVEN%lu\"\r\n",
        HAL_GetTick());
    LTE_SendAT(huart, cmd, response, 10000);
    printf("Conn result: %s\r\n", response);
    HAL_Delay(5000);

    /* Wait for +QMTCONN URC */
    memset(response, 0, sizeof(response));
    HAL_UART_Receive(huart,
                     (uint8_t *)response,
                     255, 8000);
    printf("Conn URC: %s\r\n", response);

    /* Check connected */
    if(strstr(response, "+QMTCONN: 0,0,0") != NULL)
    {
        printf("MQTT TRULY CONNECTED\r\n");
        return AVEN_OK;
    }

    /* Try direct check */
    memset(response, 0, sizeof(response));
    LTE_SendAT(huart, "AT+QMTCONN?\r\n",
               response, 5000);
    printf("Status: %s\r\n", response);

    if(strstr(response, "3") != NULL)
    {
        printf("MQTT CONNECTED VIA STATUS\r\n");
        return AVEN_OK;
    }

    return AVEN_ERROR;
}