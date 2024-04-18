/* SPIFFS filesystem example.
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"

#include "wifi_info.h"

static const char *TAG = "STORAGE_SPIFFS";

int fs_init()
{
    size_t total = 0, used = 0;

    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
	.base_path = "/spiffs",
	.partition_label = NULL,
	.max_files = 5,
	.format_if_mount_failed = true
    };

    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
	if (ret == ESP_FAIL) {
	    ESP_LOGE(TAG, "Failed to mount or format filesystem");
	} else if (ret == ESP_ERR_NOT_FOUND) {
	    ESP_LOGE(TAG, "Failed to find SPIFFS partition");
	} else {
	    ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)",
		     esp_err_to_name(ret));
	}
	return -1;
    }

    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
	ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)",
		 esp_err_to_name(ret));
    } else {
	ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

    return 0;
}

int fs_read_wifi_info(struct wifi_info *wi)
{
    // Check if destination file exists before operating
    struct stat st;
    if (stat("/spiffs/wifi_info.txt", &st) != 0)
	return -1;

    ESP_LOGI(TAG, "Opening WiFi info file");
    FILE *f = fopen("/spiffs/wifi_info.txt", "w");
    if (f == NULL) {
	ESP_LOGE(TAG, "Failed to open WiFi info file for writing");
	return -1;
    }

    fgets(wi->ssid, WIFI_INFO_SIZE, f);
    fgets(wi->passwd, WIFI_INFO_SIZE, f);
    fclose(f);
    // strip newline
    char *pos = strchr(wi->ssid, '\n');
    if (pos) {
	*pos = '\0';
    }
    ESP_LOGI(TAG, "WiFi ssid from file: '%s'", wi->ssid);
    pos = strchr(wi->passwd, '\n');
    if (pos) {
	*pos = '\0';
    }
    ESP_LOGI(TAG, "WiFi passwd from file: '%s'", wi->passwd);

    return 0;
}

int fs_write_wifi_info(struct wifi_info *wi)
{
    // Check if destination file exists before operating
    struct stat st;
    if (stat("/spiffs/wifi_info.txt", &st) != 0)
	return -1;

    ESP_LOGI(TAG, "Opening WiFi info file");
    FILE *f = fopen("/spiffs/wifi_info.txt", "w");
    if (f == NULL) {
	ESP_LOGE(TAG, "Failed to open WiFi info file for writing");
	return -1;
    }
    fprintf(f, "%s\n", wi->ssid);
    fprintf(f, "%s\n", wi->passwd);
    fclose(f);
    ESP_LOGI(TAG, "WiFi info is written");

    return 0;
}

void fs_deinit()
{
    // All done, unmount partition and disable SPIFFS
    esp_vfs_spiffs_unregister(NULL);
    ESP_LOGI(TAG, "SPIFFS unmounted");
}
