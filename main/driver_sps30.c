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
#include "driver_sps30.h"

#include "driver/i2c.h"

#define SPS30_DEV_ADDR      0xD2

static uint8_t crc8(const uint8_t *data, uint8_t len)
{
    static const uint8_t poly = 0x31;
    uint8_t crc = 0xFF;

    for (uint8_t i = 0 ; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1 ) ^ poly;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

static inline void be32_to_cpu(void *mem)
{
    uint32_t val = ((uint32_t *)mem)[0];

    val = ((val & 0x000000FF) << 24) |
          ((val & 0x0000FF00) <<  8) |
          ((val & 0x00FF0000) >>  8) |
          ((val & 0xFF000000) >> 24);

    ((uint32_t *)mem)[0] = val;
}

static esp_err_t sps30_write(const uint8_t *data, uint8_t len)
{
    i2c_cmd_handle_t cmd;

    cmd = i2c_cmd_link_create();
    ESP_ERROR_CHECK(i2c_master_start(cmd));
    ESP_ERROR_CHECK(i2c_master_write_byte(cmd, SPS30_DEV_ADDR|I2C_MASTER_WRITE, 1));

    for (uint8_t i = 0; i < len; i++) {
        ESP_ERROR_CHECK(i2c_master_write_byte(cmd, data[i], 1));
    }
    if (len > 2) {
        uint8_t crc = crc8(data+2, len-2);
        ESP_ERROR_CHECK(i2c_master_write_byte(cmd, crc, 1));
    }
    ESP_ERROR_CHECK(i2c_master_stop(cmd));
    ESP_ERROR_CHECK(i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000/portTICK_RATE_MS));
    i2c_cmd_link_delete(cmd);

    return ESP_OK;
}
    
static esp_err_t sps30_read(const uint8_t *data, uint8_t len, uint8_t *buf, uint8_t buf_len)
{
    i2c_cmd_handle_t cmd;
    uint8_t read_len = buf_len / 2 * 3;
    uint8_t *read_buf = (uint8_t *)alloca(read_len);
    
    sps30_write(data, len);

    cmd = i2c_cmd_link_create();
    ESP_ERROR_CHECK(i2c_master_start(cmd));
    ESP_ERROR_CHECK(i2c_master_write_byte(cmd, SPS30_DEV_ADDR|I2C_MASTER_READ, 1));
    for (uint8_t i = 0; i < read_len; i++) {
        ESP_ERROR_CHECK(i2c_master_read_byte(cmd, &(read_buf[i]), (i == (read_len-1))));
    }
    ESP_ERROR_CHECK(i2c_master_stop(cmd));
    ESP_ERROR_CHECK(i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000/portTICK_RATE_MS));
    i2c_cmd_link_delete(cmd);

    for (uint8_t i = 0; i < read_len / 3; i++) {
        uint8_t crc = crc8(read_buf+(i*3), 2);
        if (crc != read_buf[(i*3)+2]) {
            ESP_LOGW(TAG, "CRC unmatch (exp: 0x%02X, act: 0x%02X)", crc, read_buf[(i*3)+2]);
            return ESP_FAIL;
        }
        buf[(i*2)+0] = read_buf[(i*3)+0];
        buf[(i*2)+1] = read_buf[(i*3)+1];
    }

    return ESP_OK;
}

esp_err_t sps30_start()
{
    static const uint8_t SPS30_CMD_START[]      = { 0x00, 0x10, 0x03, 0x00 };

    ESP_ERROR_CHECK(sps30_write(SPS30_CMD_START, sizeof(SPS30_CMD_START)));
    vTaskDelay(100/portTICK_RATE_MS);

    return ESP_OK;
}

esp_err_t sps30_sense(sps30_sense_data_t *sense_data)
{
    static const uint8_t SPS30_CMD_READY[]      = { 0x02, 0x02 };
    static const uint8_t SPS30_CMD_MEASURE[]    = { 0x03, 0x00 };
    uint8_t buf[2];
    
    ESP_ERROR_CHECK(sps30_read(SPS30_CMD_READY, sizeof(SPS30_CMD_READY), buf, 2));

    if (buf[1] != 1) {
        ESP_LOGW(TAG, "Data is not ready");
        return ESP_FAIL;
    }

    ESP_ERROR_CHECK(sps30_read(SPS30_CMD_MEASURE, sizeof(SPS30_CMD_MEASURE),
                               (uint8_t *)sense_data, sizeof(sps30_sense_data_t)));

    // convert big endian to litte endian
    for (uint8_t i = 0; i < 10; i++) {
        be32_to_cpu(((uint32_t *)sense_data) + i);
    }

    return ESP_OK;
}
