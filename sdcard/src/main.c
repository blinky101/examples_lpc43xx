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

// LED blinks at half this frequency to indicate the mcu is running
#define SYSTICK_RATE_HZ (10)

// Desired CPU frequency in Hz
#define CPU_FREQ_HZ (60000000)

const GPIO *led_err;
const GPIO *led_blue;
const GPIO *led_green;
const GPIO *led_warn;

// Choose which test(s) to run.
// NOTE: tests may take up to several minutes each...
#define TEST_1
#define TEST_2
#define TEST_3
#define TEST_4
#define TEST_5
#define TEST_6

// Enable to write a file with all latency numbers of each test.
// NOTE: writing this file may take even longer than the tests themselves...
#define TIMES_TRACE

void SysTick_Handler(void)
{
    GPIO_HAL_toggle(led_blue);
}


static uint8_t g_buffer[32*1024];
static uint16_t g_write_times[16*1024]  __attribute__((section(".bss.$extra_bss")));


static void error(void)
{
    GPIO_HAL_set(led_err, HIGH);
    while(1);
}

static void prepare_test(void)
{
    // prepare buffer: dummy data to write to sdcard file
    for(size_t n=0;n<sizeof(g_buffer);n++) {
        g_buffer[n] = (n & 0xFF);
    }
    memset(g_write_times, 0, sizeof(g_write_times));

}

static void write_meta_files(unsigned int test_id,
        unsigned int total_time, unsigned int max_latency,
        uint16_t *write_times, size_t num_write_times)
{
    // Append to the meta file with the test result summaries
    char result_str[256];
    snprintf(result_str, sizeof(result_str), "%d: max=%u ms, total=%u ms\n",
            test_id, max_latency, total_time);
    sdcard_write_to_file("results.txt", result_str, strlen(result_str));

#ifdef TIMES_TRACE
    // Write the timing numbers to a file (one number per line)
    char fname[16];
    snprintf(fname, sizeof(fname), "times-%d.csv", test_id);

    // delete previous results if any
    sdcard_delete_file(fname);

    // assume iterations is a multiple of 8 for a bit more speed of this
    // naive implementation..
    for(size_t i=0;i<num_write_times;i+=8) {
        snprintf(result_str, sizeof(result_str), "%u\n%u\n%u\n%u\n%u\n%u\n%u\n%u\n",
                (unsigned int)write_times[i],
                (unsigned int)write_times[i+1],
                (unsigned int)write_times[i+2],
                (unsigned int)write_times[i+3],
                (unsigned int)write_times[i+4],
                (unsigned int)write_times[i+5],
                (unsigned int)write_times[i+6],
                (unsigned int)write_times[i+7]);
        sdcard_write_to_file(fname, result_str, strlen(result_str));
    }
#endif
}



int main(void) {
    memset(g_write_times, 0, sizeof(g_write_times));
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
        error();
    }
    if(status == SDCARD_NOT_FOUND) {
        GPIO_HAL_set(led_warn, HIGH);
        while(1);
    }
	SysTick_Config(SystemCoreClock/SYSTICK_RATE_HZ);

    sdcard_delete_file("results.txt");

#ifdef TEST_1
    {
        const size_t sizeof_buffer = 512;
        const size_t iterations = 16*1024;

        prepare_test();

        // Perform write test in small blocks of data.
        // The file is opened and closed for each block...
        uint64_t max_latency = 0;
        const uint64_t t_start = delay_get_timestamp();

        for(size_t i=0;i<iterations;i++) {
            const size_t offset = i * sizeof_buffer;

            const uint64_t t_pre = delay_get_timestamp();
            sdcard_write_to_file_offset("perf-1.bin", (char*)g_buffer, sizeof_buffer, offset);
            const uint64_t t_post = delay_get_timestamp();

            // Calculate latency, saturate to UINT16_MAX to avoid overflow
            uint32_t latency = (delay_calc_time_us(t_pre, t_post) / 1000);
            latency = min(latency, UINT16_MAX);
            g_write_times[i] = latency;

            // Keep track of maximum time spent in one write
            max_latency = max(max_latency, latency);
        }

        const uint64_t t_end = delay_get_timestamp();
        const uint64_t total_time = delay_calc_time_us(t_start, t_end) / 1000;

        write_meta_files(1, total_time, max_latency, g_write_times, iterations);
    }
#endif
#ifdef TEST_2
    {
        // buffer: dummy data to write to sdcard file
        const size_t sizeof_buffer = 2048;
        const size_t iterations = 16*1024;

        prepare_test();

        // Perform write test in small blocks of data
        // The file is opened and closed for each block...
        uint64_t max_latency = 0;
        const uint64_t t_start = delay_get_timestamp();

        for(size_t i=0;i<iterations;i++) {
            const size_t offset = i * sizeof_buffer;

            const uint64_t t_pre = delay_get_timestamp();
            sdcard_write_to_file_offset("perf-2.bin", (char*)g_buffer, sizeof_buffer, offset);
            const uint64_t t_post = delay_get_timestamp();

            // Calculate latency, saturate to UINT16_MAX to avoid overflow
            uint32_t latency = (delay_calc_time_us(t_pre, t_post) / 1000);
            latency = min(latency, UINT16_MAX);
            g_write_times[i] = latency;

            // Keep track of maximum time spent in one write
            max_latency = max(max_latency, latency);
        }

        const uint64_t t_end = delay_get_timestamp();
        const uint64_t total_time = delay_calc_time_us(t_start, t_end) / 1000;

        write_meta_files(2, total_time, max_latency, g_write_times, iterations);
    }
#endif
#ifdef TEST_3
    {
        // g_buffer: dummy data to write to sdcard file
        const size_t sizeof_buffer = 512;
        const size_t iterations = 16*1024;

        prepare_test();

        // Perform write test in small blocks of data.
        // File is opened, data is written in blocks, file is closed
        uint64_t max_latency = 0;
        const uint64_t t_start = delay_get_timestamp();

        // Open file
        FIL file;
        if(FR_OK != f_open(&file, "perf-3.bin", FA_WRITE | FA_CREATE_ALWAYS)) {
            error();
        }

        for(size_t i=0;i<iterations;i++) {
            const uint64_t t_pre = delay_get_timestamp();

            // Write a block of data to file
            UINT bw;
            if(FR_OK != f_write(&file, g_buffer, sizeof_buffer, &bw)) {
                error();
            }

            const uint64_t t_post = delay_get_timestamp();

            // Calculate latency, saturate to UINT16_MAX to avoid overflow
            uint32_t latency = (delay_calc_time_us(t_pre, t_post) / 1000);
            latency = min(latency, UINT16_MAX);
            g_write_times[i] = latency;

            // Keep track of maximum time spent in one write
            max_latency = max(max_latency, latency);
        }

        if(FR_OK != f_close(&file)) {
            error();
        }

        const uint64_t t_end = delay_get_timestamp();
        const uint64_t total_time = delay_calc_time_us(t_start, t_end) / 1000;

        write_meta_files(3, total_time, max_latency, g_write_times, iterations);
    }
#endif
#ifdef TEST_4
    {
        // g_buffer: dummy data to write to sdcard file
        const size_t sizeof_buffer = 2048;
        const size_t iterations = 16*1024;

        prepare_test();

        // Perform write test in small blocks of data.
        // File is opened, data is written in blocks, file is closed
        uint64_t max_latency = 0;
        const uint64_t t_start = delay_get_timestamp();

        // Open file
        FIL file;
        if(FR_OK != f_open(&file, "perf-4.bin", FA_WRITE | FA_CREATE_ALWAYS)) {
            error();
        }

        for(size_t i=0;i<iterations;i++) {
            const uint64_t t_pre = delay_get_timestamp();

            // Write a block of data to file
            UINT bw;
            if(FR_OK != f_write(&file, g_buffer, sizeof_buffer, &bw)) {
                error();
            }

            const uint64_t t_post = delay_get_timestamp();

            // Calculate latency, saturate to UINT16_MAX to avoid overflow
            uint32_t latency = (delay_calc_time_us(t_pre, t_post) / 1000);
            latency = min(latency, UINT16_MAX);
            g_write_times[i] = latency;

            // Keep track of maximum time spent in one write
            max_latency = max(max_latency, latency);
        }

        if(FR_OK != f_close(&file)) {
            error();
        }

        const uint64_t t_end = delay_get_timestamp();
        const uint64_t total_time = delay_calc_time_us(t_start, t_end) / 1000;

        write_meta_files(4, total_time, max_latency, g_write_times, iterations);
    }
#endif
#ifdef TEST_5
    {
        // g_buffer: dummy data to write to sdcard file
        const size_t sizeof_buffer = 32*1024;
        const size_t iterations = 4*1024;

        prepare_test();

        // Perform write test in small blocks of data.
        // File is opened, data is written in blocks, file is closed
        uint64_t max_latency = 0;
        const uint64_t t_start = delay_get_timestamp();

        // Open file
        FIL file;
        if(FR_OK != f_open(&file, "perf-5.bin", FA_WRITE | FA_CREATE_ALWAYS)) {
            error();
        }

        for(size_t i=0;i<iterations;i++) {
            const uint64_t t_pre = delay_get_timestamp();

            // Write a block of data to file
            UINT bw;
            if(FR_OK != f_write(&file, g_buffer, sizeof_buffer, &bw)) {
                error();
            }

            const uint64_t t_post = delay_get_timestamp();

            // Calculate latency, saturate to UINT16_MAX to avoid overflow
            uint32_t latency = (delay_calc_time_us(t_pre, t_post) / 1000);
            latency = min(latency, UINT16_MAX);
            g_write_times[i] = latency;

            // Keep track of maximum time spent in one write
            max_latency = max(max_latency, latency);
        }

        if(FR_OK != f_close(&file)) {
            error();
        }

        const uint64_t t_end = delay_get_timestamp();
        const uint64_t total_time = delay_calc_time_us(t_start, t_end) / 1000;

        write_meta_files(5, total_time, max_latency, g_write_times, iterations);
    }
#endif
#ifdef TEST_6
    {
        // g_buffer: dummy data to write to sdcard file
        const size_t sizeof_buffer = 2*1024;
        const size_t iterations = 16*1024;

        prepare_test();

        // Perform write test in small blocks of data.
        // File is opened, repeat(write data block, write to other file), file is closed
        uint64_t max_latency = 0;
        const uint64_t t_start = delay_get_timestamp();

        sdcard_delete_file("test.txt");

        // Open file
        FIL file;
        if(FR_OK != f_open(&file, "perf-6.bin", FA_WRITE | FA_CREATE_ALWAYS)) {
            error();
        }

        for(size_t i=0;i<iterations;i++) {
            const uint64_t t_pre = delay_get_timestamp();

            // Write a block of data to file
            UINT bw;
            if(FR_OK != f_write(&file, g_buffer, sizeof_buffer, &bw)) {
                error();
            }

            const uint64_t t_post = delay_get_timestamp();

            // Calculate latency, saturate to UINT16_MAX to avoid overflow
            uint32_t latency = (delay_calc_time_us(t_pre, t_post) / 1000);
            latency = min(latency, UINT16_MAX);
            g_write_times[i] = latency;

            // Keep track of maximum time spent in one write
            max_latency = max(max_latency, latency);


            // Write a different file in between
            const char dummy_data[] = "Hello World!\n";
            sdcard_write_to_file("test.txt", dummy_data, strlen(dummy_data));
        }

        if(FR_OK != f_close(&file)) {
            error();
        }

        const uint64_t t_end = delay_get_timestamp();
        const uint64_t total_time = delay_calc_time_us(t_start, t_end) / 1000;

        write_meta_files(6, total_time, max_latency, g_write_times, iterations);
    }
#endif

    // Test finished
    sdcard_disable();
    GPIO_HAL_set(led_green, HIGH);

    while(1);

    return 0;
}

