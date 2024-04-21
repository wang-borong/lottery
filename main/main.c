/* Esptouch example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "esptouch.h"
#include "wifi_info.h"
#include "wifi_station.h"
#include "7-segment-led-display.h"
#include "crawl_web.h"

static const char *TAG = "MAIN";
extern dis_val_t dis_val[4];
extern dis_val_t seg_dis_val[4];

SemaphoreHandle_t xSemaphore = NULL;

void app_main()
{
    struct wifi_info wi;
    int fs_ok = 1;

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());

    xSemaphore = xSemaphoreCreateMutex();

    // connect to wifi by data stored in fs
    if (fs_init()) {
	fs_ok = 0;
	ESP_LOGI(TAG, "fs init failed, go to esptouch");
	goto esptouch;
    }
    if (fs_read_wifi_info(&wi)) {
	ESP_LOGI(TAG, "can not read wifi info from fs, go to esptouch");
	goto esptouch;
    } else {
	// connect to wifi with stored ap data
	if (wifi_init_sta(&wi))
	    goto esptouch;
	ESP_LOGI(TAG, "read wifi info from fs, go to connected");
	goto connected;
    }

  esptouch:
    // connect to wifi by esptouch
    ESP_LOGI(TAG, "fs_ok: %d", fs_ok);
    if (!fs_ok)
	fs_init();
    initialise_wifi(&wi);

  connected:
    crawl_lottery_data(dis_val);
    
    display_init();

    return;
}

// :set tabstop=4 shiftwidth=4 smarttab
