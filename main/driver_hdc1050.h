#include "esp_err.h"

typedef struct hdc1050_sense_data {
    float temp;
    float humi;
} hdc1050_sense_data_t;

esp_err_t hdc1050_sense(hdc1050_sense_data_t *hdc1050_sense_data);
