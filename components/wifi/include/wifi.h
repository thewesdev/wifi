#ifndef COMPONENT_WIFI_H
#define COMPONENT_WIFI_H

#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t nvs_init();
esp_err_t wifi_init();

#ifdef __cplusplus
}
#endif

#endif // COMPONENT_WIFI_H
