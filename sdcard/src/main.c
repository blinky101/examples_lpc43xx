#include "board.h"
#include "board_GPIO_ID.h"

#include <chip.h>
#include <lpc_tools/boardconfig.h>
#include <lpc_tools/GPIO_HAL.h>
#include <lpc_tools/clock.h>
#include <mcu_timing/delay.h>
#include <mcu_sdcard/sdcard.h>
#include <c_utils/max.h>

#include <string.h>
#include <stdio.h>

// startup code needs this
unsigned int stack_value = 0xA5A55A5A;

// LED blinks at half this frequency
#define SYSTICK_RATE_HZ (10)

// Desired CPU frequency in Hz
#define CPU_FREQ_HZ (60000000)

const GPIO *led_err;
const GPIO *led_blue;
const GPIO *led_green;
const GPIO *led_warn;


void SysTick_Handler(void)
{
    GPIO_HAL_toggle(led_blue);
}


int main(void) {
    // board-specific setup
    board_setup();
    board_setup_NVIC();
    board_setup_pins();

    // fpu & system clock setup
    fpuInit();
    clock_set_frequency(CPU_FREQ_HZ);

    delay_init();

    led_err = board_get_GPIO(GPIO_ID_LED_ERR);
    led_blue = board_get_GPIO(GPIO_ID_LED_BLUE);
    led_warn = board_get_GPIO(GPIO_ID_LED_WARN);
    led_green = board_get_GPIO(GPIO_ID_LED_GREEN);

    GPIO_HAL_set(led_err, LOW);
    GPIO_HAL_set(led_blue, LOW);
    GPIO_HAL_set(led_green, LOW);
    GPIO_HAL_set(led_warn, LOW);

    sdcard_init(NULL, NULL, NULL);
    int retries = 0;
    const enum SDCardStatus status = sdcard_enable(&retries);

    if((status == SDCARD_ERROR) || retries) {
        GPIO_HAL_set(led_err, HIGH);
        while(1);
    }
    if(status == SDCARD_NOT_FOUND) {
        GPIO_HAL_set(led_warn, HIGH);
        while(1);
    }
	SysTick_Config(SystemCoreClock/SYSTICK_RATE_HZ);

    // buffer: dummy data to write to sdcard file
    uint8_t buffer[512];
    for(size_t n=0;n<512;n++) {
        buffer[n] = (n & 0xFF);
    }

    // Perform write test in blocks of 512 bytes
    uint64_t max_latency = 0;
    const uint64_t t_start = delay_get_timestamp();

    for(size_t i=0;i<16*1024;i++) {
        const size_t offset = i * sizeof(buffer);

        const uint64_t t_pre = delay_get_timestamp();
        sdcard_write_to_file_offset("perf.bin", (char*)buffer, sizeof(buffer), offset);
        const uint64_t t_post = delay_get_timestamp();

        // Keep track of maximum time spent in one write
        max_latency = max(max_latency, delay_calc_time_us(t_pre, t_post));
    }

    const uint64_t t_end = delay_get_timestamp();
    const uint64_t total_time = delay_calc_time_us(t_start, t_end);

    // Write a meta file with the test results
    char result_str[256];
    snprintf(result_str, sizeof(result_str), "max=%u ms, total=%u ms",
            (unsigned int)max_latency / 1000,
            (unsigned int)total_time / 1000);
    sdcard_delete_file("results.txt");
    sdcard_write_to_file("results.txt", result_str, strlen(result_str));

    // Test finished
    sdcard_disable();
    GPIO_HAL_set(led_green, HIGH);

    while(1);

    return 0;
}

