#include "wifi.h"

#include <esp_log.h>

#include <esp_wifi.h>
#include <nvs_flash.h>

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

const char *TAG = "WIFI";

static EventGroupHandle_t wifi_event_group;

static esp_netif_t *wifi_sta = NULL;

static uint8_t retry_num = 0;

void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
				   void *event_data) {
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
		esp_wifi_connect();
	} else if (event_base == WIFI_EVENT &&
			   event_id == WIFI_EVENT_STA_DISCONNECTED) {
		if (retry_num < CONFIG_WIFI_MAXIMUM_RETRY) {
			esp_wifi_connect();
			retry_num++;
			ESP_LOGI(TAG, "Retrying connection (%" PRId8 ")", retry_num);
		} else {
			ESP_LOGE(TAG,
					 "Failed to connect after %" PRId8 " attempts, giving up",
					 CONFIG_WIFI_MAXIMUM_RETRY);
			xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
		}
	} else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
		ESP_LOGI(TAG, "connected to: " IPSTR, IP2STR(&event->ip_info.ip));
		retry_num = 0;
		xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
	}
}

esp_err_t nvs_init() {
	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
		err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		err = nvs_flash_erase();
		if (err == ESP_ERR_NOT_FOUND) {
			ESP_LOGE(TAG,
					 "Failed to erase nvs partition: no NVS partition labeled "
					 "\"nvs\" in the partition table (%s)",
					 esp_err_to_name(err));
			return err;
		} else if (err != ESP_OK) {
			ESP_LOGE(TAG,
					 "Failed to erase nvs partition: different error in case "
					 "de-initialization fails (shouldn't happen) (%s)",
					 esp_err_to_name(err));
			return err;
		}
		err = nvs_flash_init();
	} else if (err == ESP_ERR_NOT_FOUND) {
		ESP_LOGE(TAG,
				 "Failed to init nvs partition: no partition with label "
				 "\"nvs\" is found in the partition table (%s)",
				 esp_err_to_name(err));
		return err;
	} else if (err == ESP_ERR_NO_MEM) {
		ESP_LOGE(TAG,
				 "Failed to init nvs partition: memory could not be allocated "
				 "for the internal structures (%s)",
				 esp_err_to_name(err));
		return err;
	} else if (err != ESP_OK) {
		ESP_LOGE(TAG, "Failed to init nvs partition: other error (%s)",
				 esp_err_to_name(err));
		return err;
	}

	return err;
}

esp_err_t wifi_init() {
	if (sizeof(CONFIG_WIFI_SSID) < 1 || sizeof(CONFIG_WIFI_SSID) > 32) {
		ESP_LOGE(TAG, "Invalid SSID: %s", CONFIG_WIFI_SSID);
		return ESP_ERR_WIFI_SSID;
	}

	if (CONFIG_WIFI_AUTH_MODE == WIFI_AUTH_WEP) {
		if (sizeof(CONFIG_WIFI_PASSWORD) < 5 ||
			sizeof(CONFIG_WIFI_PASSWORD) > 13) {
			ESP_LOGE(TAG, "Invalid password: %s", CONFIG_WIFI_PASSWORD);
			return ESP_ERR_WIFI_PASSWORD;
		}
	} else if (CONFIG_WIFI_AUTH_MODE == WIFI_AUTH_WPA_PSK ||
			   CONFIG_WIFI_AUTH_MODE == WIFI_AUTH_WPA2_PSK ||
			   CONFIG_WIFI_AUTH_MODE == WIFI_AUTH_WPA_WPA2_PSK) {
		if (sizeof(CONFIG_WIFI_PASSWORD) < 8 ||
			sizeof(CONFIG_WIFI_PASSWORD) > 63) {
			ESP_LOGE(TAG, "Invalid password: %s", CONFIG_WIFI_PASSWORD);
			return ESP_ERR_WIFI_PASSWORD;
		}
	} else if (CONFIG_WIFI_AUTH_MODE == WIFI_AUTH_WAPI_PSK) {
		if (sizeof(CONFIG_WIFI_PASSWORD) < 8 ||
			sizeof(CONFIG_WIFI_PASSWORD) > 88) {
			ESP_LOGE(TAG, "Invalid password: %s", CONFIG_WIFI_PASSWORD);
			return ESP_ERR_WIFI_PASSWORD;
		}
	}

	esp_err_t err;

	wifi_event_group = xEventGroupCreate();

	err = esp_netif_init();
	if (err == ESP_FAIL) {
		ESP_LOGE(TAG,
				 "Failed to initialize TCP/IP stack: initializing failed (%s)",
				 esp_err_to_name(err));
		return err;
	}

	err = esp_event_loop_create_default();
	if (err == ESP_ERR_NO_MEM) {
		ESP_LOGE(TAG,
				 "Failed to create default event loop: cannot allocate memory "
				 "for event loops list (%s)",
				 esp_err_to_name(err));
		return err;
	} else if (err == ESP_ERR_INVALID_STATE) {
		ESP_LOGE(TAG,
				 "Failed to create default event loop: default event loop has "
				 "already been created (%s)",
				 esp_err_to_name(err));
		return err;
	} else if (err == ESP_FAIL) {
		ESP_LOGE(TAG,
				 "Failed to create default event loop: failed to create task "
				 "loop (%s)",
				 esp_err_to_name(err));
		return err;
	}

	wifi_sta = esp_netif_create_default_wifi_sta();

	wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();

	esp_wifi_init(&wifi_init_cfg);

	esp_event_handler_instance_t instance_any_id;
	esp_event_handler_instance_t instance_got_ip;

	esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
										&event_handler, NULL, &instance_any_id);
	esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
										&event_handler, NULL, &instance_got_ip);

	wifi_config_t wifi_cfg = {.sta = {
								  .ssid = CONFIG_WIFI_SSID,
								  .password = CONFIG_WIFI_PASSWORD,
								  .threshold.authmode = CONFIG_WIFI_AUTH_MODE,
							  }};

	err = esp_wifi_set_mode(WIFI_MODE_STA);
	if (err == ESP_ERR_WIFI_NOT_INIT) {
		ESP_LOGE(TAG,
				 "Failed to set wifi sta mode: WiFi is not initialized by "
				 "esp_wifi_init (%s)",
				 esp_err_to_name(err));
		return err;
	} else if (err == ESP_ERR_INVALID_ARG) {
		ESP_LOGE(TAG, "Failed to set wifi sta mode: invalid arg (%s)",
				 esp_err_to_name(err));
		return err;
	} else if (err != ESP_OK) {
		ESP_LOGE(TAG, "Failed to set wifi sta mode: other error (%s)",
				 esp_err_to_name(err));
		return err;
	}

	err = esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
	if (err == ESP_ERR_WIFI_NOT_INIT) {
		ESP_LOGE(TAG,
				 "Failed to set wifi config: WiFi is not initialized by "
				 "esp_wifi_init (%s)",
				 esp_err_to_name(err));
		return err;
	} else if (err == ESP_ERR_INVALID_ARG) {
		ESP_LOGE(TAG, "Failed to set wifi config: invalid argument (%s)",
				 esp_err_to_name(err));
		return err;
	} else if (err == ESP_ERR_WIFI_IF) {
		ESP_LOGE(TAG, "Failed to set wifi config: invalid interface (%s)",
				 esp_err_to_name(err));
		return err;
	} else if (err == ESP_ERR_WIFI_MODE) {
		ESP_LOGE(TAG, "Failed to set wifi config: invalid mode (%s)",
				 esp_err_to_name(err));
		return err;
	} else if (err == ESP_ERR_WIFI_PASSWORD) {
		ESP_LOGE(TAG, "Failed to set wifi config: invalid password (%s)",
				 esp_err_to_name(err));
		return err;
	} else if (err == ESP_ERR_WIFI_NVS) {
		ESP_LOGE(TAG, "Failed to set wifi config: WiFi internal NVS error (%s)",
				 esp_err_to_name(err));
		return err;
	} else if (err == ESP_ERR_WIFI_STATE) {
		ESP_LOGE(TAG,
				 "Failed to set wifi config: WiFi still connecting when invoke "
				 "esp_wifi_set_config (%s)",
				 esp_err_to_name(err));
		return err;
	} else if (err != ESP_OK) {
		ESP_LOGE(TAG, "Failed to set wifi config: other error (%s)",
				 esp_err_to_name(err));
		return err;
	}

	err = esp_wifi_start();
	if (err == ESP_ERR_WIFI_NOT_INIT) {
		ESP_LOGE(TAG,
				 "Failed to start wifi: WiFi is not initialized by "
				 "esp_wifi_init (%s)",
				 esp_err_to_name(err));
		return err;
	} else if (err == ESP_ERR_INVALID_ARG) {
		ESP_LOGE(TAG,
				 "Failed to start wifi: It doesn't normally happen, the "
				 "function called inside the API was passed invalid argument, "
				 "user should check if the WiFi related config is correct (%s)",
				 esp_err_to_name(err));
		return err;
	} else if (err == ESP_ERR_NO_MEM) {
		ESP_LOGE(TAG, "Failed to start wifi: out of memory (%s)",
				 esp_err_to_name(err));
		return err;
	} else if (err == ESP_ERR_WIFI_CONN) {
		ESP_LOGE(TAG,
				 "Failed to start wifi: WiFi internal error, station or "
				 "soft-AP control block wrong (%s)",
				 esp_err_to_name(err));
		return err;
	} else if (err == ESP_FAIL) {
		ESP_LOGE(TAG, "Failed to start wifi: other WiFi internal errors (%s)",
				 esp_err_to_name(err));
		return err;
	}

	EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
										   WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
										   pdFALSE, pdFALSE, portMAX_DELAY);

	if (bits & WIFI_CONNECTED_BIT) {
		ESP_LOGI(TAG, "connected in wifi SSID: %s password: %s",
				 CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWORD);
	} else if (bits & WIFI_FAIL_BIT) {
		ESP_LOGE(TAG, "failed to connect in wifi SSID: %s password: %s",
				 CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWORD);
	} else {
		ESP_LOGE(TAG, "Unexpected event");
	}

	return ESP_OK;
}
