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

#define MAX_HTTP_OUTPUT_BUFFER 2048
static const char *TAG = "HTTP_CLIENT";

extern const char js_lottery_com_root_cert_pem_start[]
asm("_binary_js_lottery_com_root_cert_pem_start");
extern const char js_lottery_com_root_cert_pem_end[]
asm("_binary_js_lottery_com_root_cert_pem_end");

static uint8_t led_v[] = {
    DIS_0, DIS_1, DIS_2, DIS_3, DIS_4, DIS_5, DIS_6, DIS_7,
    DIS_8, DIS_9
};

esp_err_t _http_event_handler(esp_http_client_event_t * evt)
{
    static char *output_buffer;	// Buffer to store response of http request from event handler
    static int output_len;	// Stores number of bytes read
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
	ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
	break;
    case HTTP_EVENT_ON_CONNECTED:
	ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
	break;
    case HTTP_EVENT_HEADER_SENT:
	ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
	break;
    case HTTP_EVENT_ON_HEADER:
	ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s",
		 evt->header_key, evt->header_value);
	break;
    case HTTP_EVENT_ON_DATA:
	ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
	/*
	 *  Check for chunked encoding is added as the URL for chunked encoding used in this example returns binary data.
	 *  However, event handler can also be used in case chunked encoding is used.
	 */
	if (!esp_http_client_is_chunked_response(evt->client)) {
	    // If user_data buffer is configured, copy the response into the buffer
	    if (evt->user_data) {
		memcpy(evt->user_data + output_len, evt->data,
		       evt->data_len);
	    } else {
		if (output_buffer == NULL) {
		    output_buffer =
			(char *)
			malloc(esp_http_client_get_content_length
			       (evt->client));
		    output_len = 0;
		    if (output_buffer == NULL) {
			ESP_LOGE(TAG,
				 "Failed to allocate memory for output buffer");
			return ESP_FAIL;
		    }
		}
		memcpy(output_buffer + output_len, evt->data,
		       evt->data_len);
	    }
	    output_len += evt->data_len;
	}

	break;
    case HTTP_EVENT_ON_FINISH:
	ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
	if (output_buffer != NULL) {
	    // Response is accumulated in output_buffer. Uncomment the below line to print the accumulated response
	    // ESP_LOG_BUFFER_HEX(TAG, output_buffer, output_len);
	    free(output_buffer);
	    output_buffer = NULL;
	    output_len = 0;
	}
	break;
    case HTTP_EVENT_DISCONNECTED:
	ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
	int mbedtls_err = 0;
	esp_err_t err =
	    esp_tls_get_and_clear_last_error(evt->data, &mbedtls_err,
					     NULL);
	if (err != 0) {
	    if (output_buffer != NULL) {
		free(output_buffer);
		output_buffer = NULL;
		output_len = 0;
	    }
	    ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
	    ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
	}
	break;
    }
    return ESP_OK;
}

static int crawl_lottery_web(char *buf)
{
    esp_http_client_config_t config = {
	.url = "https://www.js-lottery.com",
	.event_handler = _http_event_handler,
	.cert_pem = js_lottery_com_root_cert_pem_start,
	.user_data = buf,	// Pass address of local buffer to get response
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    // GET
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
	ESP_LOGI(TAG, "HTTPS Status = %d, content_length = %d",
		 esp_http_client_get_status_code(client),
		 esp_http_client_get_content_length(client));
    } else {
	ESP_LOGE(TAG, "Error perform http request %s",
		 esp_err_to_name(err));
    }
    ESP_LOG_BUFFER_HEX(TAG, buf, strlen(buf));

    esp_http_client_cleanup(client);

    return 0;
}

// return the amount e.g. 8.76
static void extract_lottery_amount(char *web_page, char *amount)
{
    //...
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
    // 可能需要适当的调整 stack size
    char web_page[MAX_HTTP_OUTPUT_BUFFER] = { 0 };

    /* loop forever per 1s */
    while (1) {
	//
	if (crawl_lottery_web(web_page)) {
	    continue;
	}
	extract_lottery_amount(web_page, amount);
	amount_to_led_bits(amount, dis_val);
	// delay 1s
	vTaskDelay(1000 / portTICK_RATE_MS);
    }
}

void crawl_lottery_data(dis_val_t * dis_val)
{
#define TASK_STACK_SIZE 4096
#define TASK_PRIORTY 10		// Low priority numbers denote low priority tasks
    //TaskHandle_t task_handle = NULL;

    xTaskCreate(&crawl_lottery_data_task, "crawl_lottery_data_task",
		TASK_STACK_SIZE, (void *) dis_val, TASK_PRIORTY, NULL);
}
