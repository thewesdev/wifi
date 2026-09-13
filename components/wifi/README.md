# WiFi

## Features

- nvs flash init with automatic erase and reinit on version mismatch or out of free pages
- wifi station (STA) mode init with connection retry and Wi-Fi/IP event handling
- SSID, password, auth mode and max retry count configurable via menuconfig
- optional c++ wrapper

## Menuconfig

```bash
idf.py menuconfig
```

### Path

Component config -> WIFI

### Default Configs

| WIFI          | Type   | Macro                        | Values   |
| ------------- | ------ | ---------------------------- | -------- |
| SSID          | string | CONFIG_WIFI_SSID             | ""       |
| Password      | string | CONFIG_WIFI_PASSWORD         | ""       |
| Auth Mode     | choice | CONFIG_WIFI_AUTH_MODE        | WPA2 PSK |
| Maximum Retry | int    | CONFIG_WIFI_MAXIMUM_RETRY    | 10       |

## Installation

### IDF Component Registry

```bash
idf.py add-dependency thewesdev/wifi
```

### Github

```bash
git clone https://github.com/thewesdev/wifi
```

## Code Examples

### C

```c
#include "wifi.h"

void app_main(void) {
	nvs_init();
	wifi_init();
}
```

### Cpp

```cpp
#include "wifi.hpp"

extern "C" void app_main(void) {
	nvs::init();
	wifi::init();
}
```
