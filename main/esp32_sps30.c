/*
 * Sensing Sensirion SPS30 using ESP32
 *
 * Copyright (C) 2020 KIMATA Tetsuya <kimata@green-rabbit.net>
 *
 * This program is free software ; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2.
 */

#include "app.h"

#include "esp_ota_ops.h"
#include "esp_event.h"
#include "esp_spi_flash.h"
#include "esp_wifi.h"

#include "driver/i2c.h"

#include "nvs_flash.h"
#include "lwip/sockets.h"

#include "cJSON.h"

#include <alloca.h>
#include <string.h>

#include "wifi_config.h"
// wifi_config.h should define followings.
// #define WIFI_SSID "XXXXXXXX"             // WiFi SSID
// #define WIFI_PASS "XXXXXXXX"             // WiFi Password

#include "wifi_task.h"
#include "http_task.h"
#include "part_info.h"
#include "driver_hdc1050.h"
#include "driver_sps30.h"

////////////////////////////////////////////////////////////
// Configuration
#define FLUENTD_IP          "192.168.2.20"  // IP address of Fluentd
#define FLUENTD_PORT        8888            // Port of FLuentd
#define FLUENTD_TAG         "/sensor"       // Fluentd tag

#define SENSE_INTERVAL      10              // sensing interval

#define I2C_SCL             GPIO_NUM_25
#define I2C_SDA             GPIO_NUM_33
#define I2C_FREQ            100000          // 100kHz

SemaphoreHandle_t wifi_start = NULL;

//////////////////////////////////////////////////////////////////////

#define EXPECTED_RESPONSE "HTTP/1.1 200 OK"
#define REQUEST "POST http://" FLUENTD_IP FLUENTD_TAG " HTTP/1.0\r\n" \
    "Content-Type: application/x-www-form-urlencoded\r\n" \
    "Content-Length: %d\r\n" \
    "\r\n" \
    "json=%s"

//////////////////////////////////////////////////////////////////////
// Fluentd Function
static int connect_server()
{
    struct sockaddr_in server;
    int sock;

    sock = socket(AF_INET, SOCK_STREAM, 0);

    server.sin_family = AF_INET;
    server.sin_port = htons(FLUENTD_PORT);
    server.sin_addr.s_addr = inet_addr(FLUENTD_IP);

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) != 0) {
        ESP_LOGE(TAG, "FLUENTD CONNECT FAILED errno=%d", errno);
        return -1;
    }
    ESP_LOGI(TAG, "FLUENTD CONNECT SUCCESS");

    return sock;
}

static cJSON *sense_json(sps30_sense_data_t *sps30_sense_data,
                         hdc1050_sense_data_t *hdc1050_sense_data,
                         wifi_ap_record_t *ap_info)
{
    cJSON *root = cJSON_CreateArray();
    cJSON *item = cJSON_CreateObject();
    
    cJSON_AddNumberToObject(item, "pm10", sps30_sense_data->mass_pm1r0);
    cJSON_AddNumberToObject(item, "pm25", sps30_sense_data->mass_pm2r5);
    cJSON_AddNumberToObject(item, "pm40", sps30_sense_data->mass_pm4r0);
    cJSON_AddNumberToObject(item, "pm100", sps30_sense_data->mass_pm10r0);

    cJSON_AddNumberToObject(item, "temp", hdc1050_sense_data->temp);
    cJSON_AddNumberToObject(item, "humi", hdc1050_sense_data->humi);

    cJSON_AddStringToObject(item, "hostname", WIFI_HOSTNAME);
    cJSON_AddNumberToObject(item, "wifi_ch", ap_info->primary);
    cJSON_AddNumberToObject(item, "wifi_rssi", ap_info->rssi);

    cJSON_AddNumberToObject(item, "self_time", 0); // for Fluentd

    cJSON_AddItemToArray(root, item);

    return root;
}

static bool process_sense_data(sps30_sense_data_t *sps30_sense_data,
                               hdc1050_sense_data_t *hdc1050_sense_data,
                               wifi_ap_record_t *ap_info)
{
    char buffer[sizeof(EXPECTED_RESPONSE)];
    bool result = false;

    int sock = connect_server();
    if (sock == -1) {
        return false;
    }

    cJSON *json = sense_json(sps30_sense_data, hdc1050_sense_data, ap_info);
    char *json_str = cJSON_PrintUnformatted(json);

    do {
        if (dprintf(sock, REQUEST, strlen("json=") + strlen(json_str), json_str) < 0) {
            ESP_LOGE(TAG, "FLUENTD POST FAILED");
            break;
        }

        bzero(buffer, sizeof(buffer));
        read(sock, buffer, sizeof(buffer)-1);

        if (strcmp(buffer, EXPECTED_RESPONSE) != 0) {
            ESP_LOGE(TAG, "FLUENTD POST FAILED");
            break;
        }
        ESP_LOGI(TAG, "FLUENTD POST SUCCESSFUL");

        result = true;
    } while (0);

    close(sock);
    cJSON_Delete(json);

    return result;
}

//////////////////////////////////////////////////////////////////////
// GPIO Function
static void init_gpio()
{
    i2c_config_t conf;

    // I2C
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_SDA;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = I2C_SCL;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_FREQ;
    i2c_param_config(I2C_NUM_0, &conf);
    i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);
}

//////////////////////////////////////////////////////////////////////
void app_main()
{
    wifi_ap_record_t ap_info;
    sps30_sense_data_t sps30_sense_data;
    hdc1050_sense_data_t hdc1050_sense_data;

    esp_log_level_set("wifi", ESP_LOG_ERROR);

    part_info_show("Running", esp_ota_get_running_partition());

    vSemaphoreCreateBinary(wifi_start);
    xSemaphoreTake(wifi_start, portMAX_DELAY);

    wifi_task_start(wifi_start);

    xSemaphoreTake(wifi_start, portMAX_DELAY);
    http_task_start();
    
    init_gpio();
    ESP_ERROR_CHECK(sps30_start());
    
    while (1) {
        ESP_ERROR_CHECK(sps30_sense(&sps30_sense_data));
        ESP_ERROR_CHECK(hdc1050_sense(&hdc1050_sense_data));

        ESP_ERROR_CHECK(esp_wifi_sta_get_ap_info(&ap_info));
        
        process_sense_data(&sps30_sense_data, &hdc1050_sense_data, 
                           &ap_info);

        ESP_LOGI(TAG, "Wait %d sec...", SENSE_INTERVAL);
        vTaskDelay(SENSE_INTERVAL*1000/portTICK_RATE_MS);
    }
}
