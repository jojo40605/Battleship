#include <driver/uart.h>
#include "hw.h"
#include "pin.h"
#include "com.h"

#define UART_PORT_NUM UART_NUM_2
#define BAUD_RATE 115200
#define DATA_BITS UART_DATA_8_BITS
#define PARITY UART_PARITY_DISABLE
#define STOP_BITS UART_STOP_BITS_1
#define FLOW_CTRL UART_HW_FLOWCTRL_DISABLE
#define RX_BUFFER_SIZE (UART_HW_FIFO_LEN(UART_PORT_NUM) * 2)
#define TX_BUFFER_SIZE 0

int32_t com_init(void) {
    // UART parameters configuration
    uart_config_t uart_config = {
        .baud_rate = BAUD_RATE,
        .data_bits = DATA_BITS,
        .parity = PARITY,
        .stop_bits = STOP_BITS,
        .flow_ctrl = FLOW_CTRL,
        .source_clk = UART_SCLK_DEFAULT
    };
    
    // Configure UART
    if (uart_param_config(UART_PORT_NUM, &uart_config) != ESP_OK) {
        return -1;
    }

    // Setting up TX and RX pins
    if (uart_set_pin(UART_PORT_NUM, HW_EX8, HW_EX7, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) {
        return -1;
    }

    // Installing the driver
    if (uart_driver_install(UART_PORT_NUM, RX_BUFFER_SIZE, TX_BUFFER_SIZE, 0, NULL, 0) != ESP_OK) {
        return -1;
    }

    // Enable pull-up on the RX pin (HW_EX7)
    pin_pullup(HW_EX7, true);

    return 0;
}

int32_t com_deinit(void) {
    if (uart_is_driver_installed(UART_PORT_NUM)) {
        if (uart_driver_delete(UART_PORT_NUM) == ESP_OK) {
            return 0;
        }
    }
    return -1;
}

int32_t com_write(const void *buf, uint32_t size) {
    // Non-blocking transmission
    int len = uart_tx_chars(UART_PORT_NUM, (const char *)buf, size);
    return len;
}

int32_t com_read(void *buf, uint32_t size) {
    // Non-blocking read, returns immediately
    int len = uart_read_bytes(UART_PORT_NUM, (uint8_t *)buf, size, 0);
    return len;
}