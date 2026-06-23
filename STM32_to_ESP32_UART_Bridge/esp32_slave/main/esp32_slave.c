#include <stdio.h>
#include <string.h>
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define UART_NUM   UART_NUM_2
#define TX_PIN     GPIO_NUM_17   // TX2
#define RX_PIN     GPIO_NUM_16   // RX2
#define LED_PIN    GPIO_NUM_2    // onboard LED on this breakout
#define BUF_SIZE   256

void app_main(void)
{
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM, &uart_config);
    uart_set_pin(UART_NUM, TX_PIN, RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_flush(UART_NUM);   // discard anything that queued up before we got here

    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

    uint8_t data[BUF_SIZE];

    while (1) {
        int len = uart_read_bytes(UART_NUM, data, BUF_SIZE - 1, pdMS_TO_TICKS(1000));
        if (len > 0) {
            data[len] = '\0';
            gpio_set_level(LED_PIN, 1);

            printf("Got %d bytes: %s", len, data);

            if (strncmp((char *)data, "PING", 4) == 0) {
                const char *reply = "PONG\n";
                uart_write_bytes(UART_NUM, reply, strlen(reply));
                printf("Sent PONG\n");
            }

            vTaskDelay(pdMS_TO_TICKS(100));
            gpio_set_level(LED_PIN, 0);
        }
    }
}
