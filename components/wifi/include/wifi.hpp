#ifndef COMPONENT_WIFI_HPP
#define COMPONENT_WIFI_HPP

#include "wifi.h"

namespace nvs {
	inline esp_err_t init() { return nvs_init(); }
} // namespace nvs

namespace wifi {
	inline esp_err_t init() { return wifi_init(); }
}; // namespace wifi

#endif // COMPONENT_WIFI_HPP
