#ifndef __WIFI_INFO_H
#define __WIFI_INFO_H

#define WIFI_INFO_SIZE 65
struct wifi_info {
    char ssid[WIFI_INFO_SIZE];
    char passwd[WIFI_INFO_SIZE];
};


int fs_init();
void fs_deinit();
int fs_read_wifi_info(struct wifi_info *wi);
int fs_write_wifi_info(struct wifi_info *wi);

#endif
