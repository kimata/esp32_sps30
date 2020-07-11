#include "esp_err.h"

typedef struct sps30_sense_data {
    float mass_pm1r0;
    float mass_pm2r5;
    float mass_pm4r0;
    float mass_pm10r0;
    float num_pm0r5;
    float num_pm1r0;
    float num_pm2r5;
    float num_pm4r0;
    float num_pm10r0;
    float typ_size;
} sps30_sense_data_t;

esp_err_t sps30_start();
esp_err_t sps30_sense(sps30_sense_data_t *sense_data);
