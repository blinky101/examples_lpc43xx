#include "board.h"
#include "board_GPIO_ID.h"

#include <lpc_tools/boardconfig.h>
#include <lpc_tools/GPIO_HAL.h>
#include <c_utils/static_assert.h>

#include <chip.h>

// Oscillator frequency, needed by chip libraries
const uint32_t OscRateIn = 12000000;
const uint32_t ExtRateIn = 0;

static const NVICConfig NVIC_config[] = {
    {TIMER2_IRQn,       1},     // Delay timer: should be correct in any context
    {SysTick_IRQn,      2},     // systick timer: high priority for now?

    {SDIO_IRQn,         3},     // SD card: probably not timing sensitive
};

static const PinMuxConfig pinmuxing[] = {


        // Blinky101 board
        {2, 10, (SCU_MODE_FUNC0)}, // GPIO0[14]
        {2, 11, (SCU_MODE_FUNC0)}, // GPIO1[11]
        {2, 12, (SCU_MODE_FUNC0)}, // GPIO1[12]
        {2, 13, (SCU_MODE_FUNC0)}, // GPIO1[13]

        // SD Card
        {1, 6, (SCU_MODE_FUNC7
                | SCU_MODE_INBUFF_EN
                | SCU_MODE_PULLUP)},    // SDCARD_CMD
        {1, 9, (SCU_MODE_FUNC7
                | SCU_MODE_INBUFF_EN
                | SCU_MODE_PULLUP)},    // SDCARD_DATA0
        {1, 10, (SCU_MODE_FUNC7
                | SCU_MODE_INBUFF_EN
                | SCU_MODE_PULLUP)},    // SDCARD_DATA1
        {1, 11, (SCU_MODE_FUNC7
                | SCU_MODE_INBUFF_EN
                | SCU_MODE_PULLUP)},    // SDCARD_DATA2
        {1, 12, (SCU_MODE_FUNC7
                | SCU_MODE_INBUFF_EN
                | SCU_MODE_PULLUP)},    // SDCARD_DATA3
};

static const GPIOConfig pin_config[] = {
    [GPIO_ID_LED_ERR]       = {{0,  14}, GPIO_CFG_DIR_OUTPUT_LOW},
    [GPIO_ID_LED_GREEN]     = {{1,  11}, GPIO_CFG_DIR_OUTPUT_LOW},
    [GPIO_ID_LED_BLUE]      = {{1,  12}, GPIO_CFG_DIR_OUTPUT_LOW},
    [GPIO_ID_LED_WARN]      = {{1,  13}, GPIO_CFG_DIR_OUTPUT_LOW},
};

// pin config struct should match GPIO_ID enum
STATIC_ASSERT( (GPIO_ID_MAX == (sizeof(pin_config)/sizeof(GPIOConfig))));

static const BoardConfig config = {
    .nvic_configs = NVIC_config,
    .nvic_count = sizeof(NVIC_config) / sizeof(NVIC_config[0]),

    .pinmux_configs = pinmuxing,
    .pinmux_count = sizeof(pinmuxing) / sizeof(pinmuxing[0]),

    .GPIO_configs = pin_config,
    .GPIO_count = sizeof(pin_config) / sizeof(pin_config[0]),

    .ADC_configs = NULL,
    .ADC_count = 0
};

void board_setup(void)
{
    board_set_config(&config);

    Chip_SCU_ClockPinMuxSet(0, (SCU_PINIO_FAST | SCU_MODE_FUNC4)); //SD CLK
}

