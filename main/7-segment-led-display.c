/*
 * 7 segment led display API
 *
 * Design concept:
 * 使用移位寄存器 595 作为数码管的输入信号驱动，并且动态的显示 4 个 bit。
 * 1. 使用 hw_timer 做为时钟计时，并且完成必要回调
 * 2. 使用 GPIO 作为 595 的信号输入
 */

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "driver/gpio.h"
#include "driver/hw_timer.h"

#include "esp_log.h"
#include "esp_system.h"

#include "7-segment-led-display.h"

static const char *TAG = "7-segment-led-display";

#define DIS_595_SCLK	12
#define DIS_595_RCLK	13
#define DIS_595_DIO	14

#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<DIS_595_SCLK) | (1ULL<<DIS_595_RCLK) | (1ULL<<DIS_595_DIO))
#define TIMER_RELOAD      true

dis_val_t dis_val[4] = { DIS_Blank, DIS_Blank, DIS_Blank, DIS_Blank };
dis_val_t seg_dis_val[4] = { DIS_Blank, DIS_Blank, DIS_Blank, DIS_Blank };

extern SemaphoreHandle_t xSemaphore;

static void esp_gpio_init()
{
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
}

#define SEG_DALEY   1

static void gpio_send_to_595(uint8_t byte)
{
    int i;

    for (i = 8; i > 0; i--) {
	gpio_set_level(DIS_595_DIO, (byte >> 7));
	/*
	 * Setting gpio must be delay 2 us, but we don't have us delay.
	 * Thus we print log to delay, don't remove it. :)
	 */
	//ESP_LOGI(TAG, "Set gpio delay");
	os_delay_us(SEG_DALEY);

	/* 595 rise edge, indicate one data bit is sent to 595 */
	gpio_set_level(DIS_595_SCLK, 0);
	//ESP_LOGI(TAG, "Set gpio delay");
	os_delay_us(SEG_DALEY);
	gpio_set_level(DIS_595_SCLK, 1);
	//ESP_LOGI(TAG, "Set gpio delay");
	os_delay_us(SEG_DALEY);

	byte <<= 1;
    }
}

static void lock_595_data()
{
    /* lock data to 595 register */
    gpio_set_level(DIS_595_RCLK, 0);
    //ESP_LOGI(TAG, "Set gpio delay");
    os_delay_us(SEG_DALEY);
    gpio_set_level(DIS_595_RCLK, 1);
    //ESP_LOGI(TAG, "Set gpio delay");
    os_delay_us(SEG_DALEY);
}

static void display_one_bit(dis_val_t dis_bit, uint8_t pos)
{
    uint8_t byte = dis_bit;

    gpio_send_to_595(byte);
    gpio_send_to_595(0x08 >> pos);
    lock_595_data();
}

#if 0
static void display_four_bit(dis_val_t *dis_bits)
{
    int i;
    for (i = 0; i < 4; i++) {
	display_one_bit(dis_bits[i], i);
    }
}
#else
static void display_four_bit(dis_val_t *dis_bits)
{
    static int c = 0;
    int i, n;
    n = (c + 1) % 4;
    for (i = 0; i < 4; i++) {
	display_one_bit(dis_bits[c], c);
	c = (c + 1) % 4;
    }
    c = n;
}
#endif

static void display_scan(void *arg)
{
    dis_val_t *dis_val = (dis_val_t *) arg;
    while (1) {
	xSemaphoreTake(xSemaphore, portMAX_DELAY);
	display_four_bit(dis_val);
	xSemaphoreGive(xSemaphore);
	vTaskDelay(1);
    }
}

void display_init()
{
#define TASK_STACK_SIZE 2048
#define TASK_PRIORTY 15		// Low priority numbers denote low priority tasks
    esp_gpio_init();
    xTaskCreate(&display_scan, "seg_display_task",
		TASK_STACK_SIZE, (void *) seg_dis_val, TASK_PRIORTY, NULL);
}
