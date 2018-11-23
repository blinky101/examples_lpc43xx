#include "board.h"
#include "board_GPIO_ID.h"

#include <chip.h>
#include <lpc_tools/boardconfig.h>
#include <lpc_tools/GPIO_HAL.h>
#include <lpc_tools/clock.h>

// startup code needs this
unsigned int stack_value = 0xA5A55A5A;

// LED blinks at half this frequency
#define SYSTICK_RATE_HZ (10)

// Desired CPU frequency in Hz
#define CPU_FREQ_HZ (60000000)

const GPIO *led_red;
const GPIO *led_blue;
const GPIO *led_green;
const GPIO *led_yellow;

volatile int green_count = 0;
volatile int yellow_count = 0;

void SysTick_Handler(void)
{
    GPIO_HAL_toggle(led_red);
    GPIO_HAL_toggle(led_blue);
    if ((yellow_count % (yellow_count % 6)) == 0) {
        GPIO_HAL_toggle(led_yellow);
    }
    yellow_count++;
    if ((green_count % 5) == 0) {
        GPIO_HAL_toggle(led_green);
    }
    green_count++;
}


int main(void) {
    // board-specific setup
    board_setup();
    board_setup_NVIC();
    board_setup_pins();

    // fpu & system clock setup
    fpuInit();
    clock_set_frequency(CPU_FREQ_HZ);

    led_red = board_get_GPIO(GPIO_ID_LED_RED);
    led_blue = board_get_GPIO(GPIO_ID_LED_BLUE);
    led_yellow = board_get_GPIO(GPIO_ID_LED_YELLOW);
    led_green = board_get_GPIO(GPIO_ID_LED_GREEN);

    GPIO_HAL_set(led_red, HIGH);
    GPIO_HAL_set(led_blue, LOW);
    GPIO_HAL_set(led_green, HIGH);
    GPIO_HAL_set(led_yellow, LOW);

	SysTick_Config(SystemCoreClock/SYSTICK_RATE_HZ);

    while(1);

    return 0;
}

