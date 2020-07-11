#include <stdlib.h>
#include <string.h>

#include "esp_ota_ops.h"
#include "driver/gpio.h"
#include "cJSON.h"

#include "app.h"
#include "http_task.h"
#include "http_ota_handler.h"

httpd_handle_t http_task_start(void)
{
    ESP_LOGI(TAG, "Start HTTP server.");

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_ERROR_CHECK(httpd_start(&server, &config));

    http_ota_handler_install(server);

    return server;
}

void http_task_stop(httpd_handle_t server)
{
    ESP_ERROR_CHECK(httpd_stop(server));
}
