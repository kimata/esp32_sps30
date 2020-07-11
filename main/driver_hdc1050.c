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
#include "driver_hdc1050.h"

#include "driver/i2c.h"

#define HDC1050_DEV_ADDR    0x80

//////////////////////////////////////////////////////////////////////
static esp_err_t hdc1050_write(const uint8_t *data, uint8_t len)
{
    i2c_cmd_handle_t cmd;

    cmd = i2c_cmd_link_create();
    ESP_ERROR_CHECK(i2c_master_start(cmd));
    ESP_ERROR_CHECK(i2c_master_write_byte(cmd, HDC1050_DEV_ADDR|I2C_MASTER_WRITE, 1));

    for (uint8_t i = 0; i < len; i++) {
        ESP_ERROR_CHECK(i2c_master_write_byte(cmd, data[i], 1));
    }
    ESP_ERROR_CHECK(i2c_master_stop(cmd));
    ESP_ERROR_CHECK(i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000/portTICK_RATE_MS));
    i2c_cmd_link_delete(cmd);

    return ESP_OK;
}

static esp_err_t hdc1050_read(const uint8_t *data, uint8_t len, uint8_t *read_buf, uint8_t read_len)
{
    i2c_cmd_handle_t cmd;

    hdc1050_write(data, len);

    vTaskDelay(50/portTICK_RATE_MS);

    cmd = i2c_cmd_link_create();
    ESP_ERROR_CHECK(i2c_master_start(cmd));
    ESP_ERROR_CHECK(i2c_master_write_byte(cmd, HDC1050_DEV_ADDR|I2C_MASTER_READ, 1));
    for (uint8_t i = 0; i < read_len; i++) {
        ESP_ERROR_CHECK(i2c_master_read_byte(cmd, &(read_buf[i]), (i == (read_len-1))));
    }
    ESP_ERROR_CHECK(i2c_master_stop(cmd));
    ESP_ERROR_CHECK(i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000/portTICK_RATE_MS));
    i2c_cmd_link_delete(cmd);

    return ESP_OK;
}

esp_err_t hdc1050_sense(hdc1050_sense_data_t *hdc1050_sense_data)
{
    static const uint8_t HDC1050_REG_TEMP[] 	= { 0x00 };

    uint8_t buf[4];

    ESP_ERROR_CHECK(hdc1050_read(HDC1050_REG_TEMP, sizeof(HDC1050_REG_TEMP),
                                 buf, sizeof(buf)));

    float temp = (float)(buf[0]<<8 | buf[1])/(1 << 16)*165 - 40;
    float humi = (float)(buf[2]<<8 | buf[3])/(1 << 16)*100;

    hdc1050_sense_data->temp = temp;
    hdc1050_sense_data->humi = humi;

    return ESP_OK;
}
