#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "esp_tls.h"

#include "esp_http_client.h"

#include "7-segment-led-display.h"

#define MAX_HTTP_OUTPUT_BUFFER 4096
static const char *TAG = "HTTP_CLIENT";

#define WEB_SERVER "www.js-lottery.com"
#define WEB_PORT 443
#define WEB_URL "https://www.js-lottery.com"

static const char *REQUEST = "GET " WEB_URL " HTTP/1.0\r\n"
    "Host: "WEB_SERVER"\r\n"
    "User-Agent: esp-idf/1.0 esp32\r\n"
    "\r\n";

extern const unsigned char js_lottery_com_root_cert_pem_start[]
asm("_binary_js_lottery_com_root_cert_pem_start");
extern const unsigned char js_lottery_com_root_cert_pem_end[]
asm("_binary_js_lottery_com_root_cert_pem_end");

static uint8_t led_v[] = {
    DIS_0, DIS_1, DIS_2, DIS_3, DIS_4, DIS_5, DIS_6, DIS_7,
    DIS_8, DIS_9
};

static int crawl_lottery_web(char *buf)
{
	int ret, len;

	esp_tls_cfg_t *cfg = malloc(sizeof(esp_tls_cfg_t));
	cfg->cacert_pem_buf = js_lottery_com_root_cert_pem_start;
	cfg->cacert_pem_bytes = js_lottery_com_root_cert_pem_end - js_lottery_com_root_cert_pem_start;

        //esp_tls_cfg_t cfg = {
        //    .cacert_pem_buf  = js_lottery_com_root_cert_pem_start,
        //    .cacert_pem_bytes = js_lottery_com_root_cert_pem_end - js_lottery_com_root_cert_pem_start,
        //};
        
        struct esp_tls *tls = esp_tls_conn_http_new(WEB_URL, cfg);
        
        if(tls != NULL) {
            ESP_LOGI(TAG, "Connection established...");
        } else {
            ESP_LOGE(TAG, "Connection failed...");
	    ret = -1;
            goto exit;
        }
        
        size_t written_bytes = 0;
        do {
            ret = esp_tls_conn_write(tls, 
                                     REQUEST + written_bytes, 
                                     strlen(REQUEST) - written_bytes);
            if (ret >= 0) {
                ESP_LOGI(TAG, "%d bytes written", ret);
                written_bytes += ret;
            } else if (ret != ESP_TLS_ERR_SSL_WANT_READ  && ret != ESP_TLS_ERR_SSL_WANT_WRITE)
            {
                ESP_LOGE(TAG, "esp_tls_conn_write  returned 0x%x", ret);
                goto exit;
            }
        } while(written_bytes < strlen(REQUEST));

        ESP_LOGI(TAG, "Reading HTTP response...");

        do
        {
            len = MAX_HTTP_OUTPUT_BUFFER - 1;
            bzero(buf, MAX_HTTP_OUTPUT_BUFFER);
            ret = esp_tls_conn_read(tls, (char *)buf, len);

            if (ret == ESP_TLS_ERR_SSL_WANT_READ  || ret == ESP_TLS_ERR_SSL_WANT_WRITE)
                continue;
            
            if(ret < 0)
            {
                ESP_LOGE(TAG, "esp_tls_conn_read  returned -0x%x", -ret);
                break;
            }

            if(ret == 0)
            {
                ESP_LOGI(TAG, "connection closed");
                break;
            }

            len = ret;
            ESP_LOGD(TAG, "%d bytes read", len);
            /* Print response directly to stdout as it is read */
            for(int i = 0; i < len; i++) {
                putchar(buf[i]);
            }
	    ret = 0;
        } while(1);

    exit:
	free(cfg);
        esp_tls_conn_delete(tls);    
        putchar('\n'); // JSON output doesn't have a newline at end

	return ret;
}


// return the amount e.g. 8.76
static void extract_lottery_amount(char *web_page, char *amount)
{
    //...
    strcpy(amount, "8.36");
}

// e.g. amount is 8.76 or 10.33
static void amount_to_led_bits(char *amount, dis_val_t * dis_val)
{
    // "8.76"   
    int i;
    char b;
    int dot = 0;
    int len = strlen(amount);

    for (i = 0; i < len; i++) {
	b = amount[len - 1 - i - dot];
	if (b == '.') {
	    dot = 1;
	    b = amount[len - 1 - i - dot];	// if it has dot, it must have number
	    dis_val[i] = led_v[b - 0x30] | DIS_Dot;
	} else {
	    dis_val[i] = led_v[b - 0x30];
	}
    }
    if (len == 4)
	dis_val[3] = led_v[0];
}

static void crawl_lottery_data_task(void *arg)
{
    dis_val_t *dis_val = (dis_val_t *) arg;
    char amount[8] = { 0 };
    // task stack is not sufficient, use heap by malloc
    char *web_page = malloc(MAX_HTTP_OUTPUT_BUFFER);
    memset(web_page, 0, MAX_HTTP_OUTPUT_BUFFER);

    /* loop forever per 1s */
    while (1) {
	//
	if (crawl_lottery_web(web_page)) {
	    continue;
	}
	extract_lottery_amount(web_page, amount);
	amount_to_led_bits(amount, dis_val);
        ESP_LOG_BUFFER_HEX(TAG, dis_val, 4);
	// delay 1s
	vTaskDelay(1000 / portTICK_RATE_MS);
    }

    free(web_page);
}

void crawl_lottery_data(dis_val_t * dis_val)
{
#define TASK_STACK_SIZE 4096
#define TASK_PRIORTY 10		// Low priority numbers denote low priority tasks
    //TaskHandle_t task_handle = NULL;

    xTaskCreate(&crawl_lottery_data_task, "crawl_lottery_data_task",
		TASK_STACK_SIZE, (void *) dis_val, TASK_PRIORTY, NULL);
}
