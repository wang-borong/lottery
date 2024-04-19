#include <stdio.h>
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

#define CRAWL_PER_SEC 10
#define MAX_HTTP_OUTPUT_BUFFER 2048
static const char *TAG = "HTTP_CLIENT";

#define WEB_SERVER "www.js-lottery.com"
#define WEB_PORT 443
#define WEB_URL "https://www.js-lottery.com"

static const char *REQUEST = "GET " WEB_URL " HTTP/1.0\r\n"
    "Host: " WEB_SERVER "\r\n" "User-Agent: esp-idf/1.0 esp32\r\n" "\r\n";

extern const unsigned char js_lottery_com_root_cert_pem_start[]
asm("_binary_js_lottery_com_root_cert_pem_start");
extern const unsigned char js_lottery_com_root_cert_pem_end[]
asm("_binary_js_lottery_com_root_cert_pem_end");

static uint8_t led_v[] = {
    DIS_0, DIS_1, DIS_2, DIS_3, DIS_4, DIS_5, DIS_6, DIS_7,
    DIS_8, DIS_9
};

static char *crawl_lottery_web(char *buf)
{
    int ret, len;

    char *data = NULL;
    const char dest[] = "fa fa-star";

    esp_tls_cfg_t *cfg = malloc(sizeof(esp_tls_cfg_t));
    cfg->cacert_pem_buf = js_lottery_com_root_cert_pem_start;
    cfg->cacert_pem_bytes =
	js_lottery_com_root_cert_pem_end -
	js_lottery_com_root_cert_pem_start;

    //esp_tls_cfg_t cfg = {
    //    .cacert_pem_buf  = js_lottery_com_root_cert_pem_start,
    //    .cacert_pem_bytes = js_lottery_com_root_cert_pem_end - js_lottery_com_root_cert_pem_start,
    //};

    struct esp_tls *tls = esp_tls_conn_http_new(WEB_URL, cfg);

    if (tls != NULL) {
	ESP_LOGI(TAG, "Connection established...");
    } else {
	ESP_LOGE(TAG, "Connection failed...");
	data = NULL;
	goto exit;
    }

    size_t written_bytes = 0;
    do {
	ret =
	    esp_tls_conn_write(tls, REQUEST + written_bytes,
			       strlen(REQUEST) - written_bytes);
	if (ret >= 0) {
	    ESP_LOGI(TAG, "%d bytes written", ret);
	    written_bytes += ret;
	} else if (ret != ESP_TLS_ERR_SSL_WANT_READ
		   && ret != ESP_TLS_ERR_SSL_WANT_WRITE) {
	    ESP_LOGE(TAG, "esp_tls_conn_write  returned 0x%x", ret);
	    goto exit;
	}
    } while (written_bytes < strlen(REQUEST));

    ESP_LOGI(TAG, "Reading HTTP response...");

    do {
	len = MAX_HTTP_OUTPUT_BUFFER - 1;
	bzero(buf, MAX_HTTP_OUTPUT_BUFFER);
	ret = esp_tls_conn_read(tls, (char *) buf, len);

	if (ret == ESP_TLS_ERR_SSL_WANT_READ
	    || ret == ESP_TLS_ERR_SSL_WANT_WRITE)
	    continue;

	if (ret < 0) {
	    ESP_LOGE(TAG, "esp_tls_conn_read  returned -0x%x", -ret);
	    data = NULL;
	    break;
	}

	if (ret == 0) {
	    ESP_LOGI(TAG, "connection closed");
	    break;
	}

	len = ret;
	ESP_LOGD(TAG, "%d bytes read", len);
	/* Print response directly to stdout as it is read */
	// for(int i = 0; i < len; i++) {
	//     putchar(buf[i]);
	// }
	if ((data = strstr(buf, dest)) != NULL) {
	    ESP_LOGI(TAG, "lottery data have been fetched");
	    for (int i = 0; i < 100; i++) {
		putchar(data[i]);
	    }
	    break;
	}
    } while (1);

  exit:
    free(cfg);
    esp_tls_conn_delete(tls);
    putchar('\n');		// JSON output doesn't have a newline at end

    return data;
}

// return the amount e.g. 8.76
static char *extract_lottery_amount(char *lottery_data)
{
    const char *color_id = "color";
    char *color_pos = NULL;
    char *amount = NULL;
    int len = 0 if ((color_pos = strstr(lottery_data, color_id)) != NULL) {
	ESP_LOGI(TAG, "got color identifier");

	/* I analyzed it, no more 50 bytes to get amount data */
	for (int i = 0; i < 50; i++) {
	    if (color_pos[i] == '>') {
		if (color_pos[i + 1] >= '0' && color_pos[i + 1] <= '9')
		    amount = color_pos + i + 1;
		/*
		 * I suppose '亿' is encoded as UTF-8, that is [0xe4, 0xba, 0xbf].
		 * Because esp8266's SDK is good enough! :)
		 */
	    } else if (color_pos[i] == 0xe4 && color_pos[i + 1] == 0xba
		       && color_pos[i + 2] == 0xbf) {
		color_pos[i] = '\0';
		// Tell me it's right.
		ESP_LOGI(TAG, "the amount is '%s'", amount);
		return amount;
	    } else {
	    }
	}
    } else {
	ESP_LOGE(TAG, "wrong lottery_data");
	printf("%s\n", lottery_data);
	return NULL;
    }
}

static void amount_to_led_bits(char *amount, dis_val_t *dis_val)
{
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
    char *amount;
    char *lottery_data;
    // task stack is not sufficient, use heap by malloc
    char *web_page = malloc(MAX_HTTP_OUTPUT_BUFFER);
    memset(web_page, 0, MAX_HTTP_OUTPUT_BUFFER);

    /* loop forever per CRAWL_PER_SEC */
    while (1) {
	if ((lottery_data = crawl_lottery_web(web_page)) == NULL) {
	    continue;		// shouldn't occur
	}
	if ((amount = extract_lottery_amount(lottery_data)) == NULL) {
	    continue;		// shouldn't occur
	}
	amount_to_led_bits(amount, dis_val);
	// show me the led code data
	ESP_LOG_BUFFER_HEX(TAG, dis_val, 4);
	memset(web_page, 0, MAX_HTTP_OUTPUT_BUFFER);
	vTaskDelay(CRAWL_PER_SEC * 1000 / portTICK_RATE_MS);
    }

    free(web_page);
}

void crawl_lottery_data(dis_val_t *dis_val)
{
#define TASK_STACK_SIZE 4096
#define TASK_PRIORTY 10		// Low priority numbers denote low priority tasks

    xTaskCreate(&crawl_lottery_data_task, "crawl_lottery_data_task",
		TASK_STACK_SIZE, (void *) dis_val, TASK_PRIORTY, NULL);
}
