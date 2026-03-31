/**
 * ECE 315 Computer Interfacing
 *
 * @file main.c
 * @details Connect a Pico to a custom SPI chip
 * @author Steven Knudsen
 * @copyright 2025, University of Alberta
 * @version   1.0
 * @licence   MIT
 */
#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pico/binary_info.h"
#include "hardware/spi.h"

#define BUF_LEN 0x02 // byte; a 16-bit ADC result is expected

// Set a delay time of exactly 100ms -- 10 Hz
const TickType_t ms_delay = 100 / portTICK_PERIOD_MS;

// References to the tasks
TaskHandle_t adc_task_handle = NULL;

/*
 * ADC task parameters struct
 */
typedef struct adc_config
{
    TickType_t sample_period_ms;
} adc_config_t;

adc_config_t adc_config = {
    100 / portTICK_PERIOD_MS,
};

#ifdef PICO_DEFAULT_SPI_CSN_PIN
static inline void cs_select()
{
    asm volatile("nop \n nop \n nop");
    gpio_put(PICO_DEFAULT_SPI_CSN_PIN, 0); // Active low
    asm volatile("nop \n nop \n nop");
}

static inline void cs_deselect()
{
    asm volatile("nop \n nop \n nop");
    gpio_put(PICO_DEFAULT_SPI_CSN_PIN, 1);
    asm volatile("nop \n nop \n nop");
}
#endif

/**
 * @brief The ADC task. Configure the SPI interface,
 *        then read the ADC 16-bit value every
 *        sample_period_ms milliseconds
 */
void adc_task(void *params)
{

    adc_config_t cfg = *((adc_config_t *)params);

#if !defined(spi_default) || !defined(PICO_DEFAULT_SPI_SCK_PIN) || !defined(PICO_DEFAULT_SPI_TX_PIN) || !defined(PICO_DEFAULT_SPI_RX_PIN)
#warning spi/spi_master example requires a board with SPI pins
    printf("Default SPI pins were not defined\n");
#else
    spi_init(spi_default, 500 * 1000); // SPI clock is 500 kHz
    gpio_set_function(PICO_DEFAULT_SPI_RX_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PICO_DEFAULT_SPI_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PICO_DEFAULT_SPI_TX_PIN, GPIO_FUNC_SPI);
    // We need to manage the CSN pin ourselves, so make it a plain old GPIO
    gpio_init(PICO_DEFAULT_SPI_CSN_PIN);
    gpio_set_dir(PICO_DEFAULT_SPI_CSN_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_SPI_CSN_PIN, 1);

    uint8_t in_buf[BUF_LEN];

    const float ADC_full_scale = ((float)0x3FFF) / 5.0;
    const float scale = (953. / 4.65 + 115. / 0.563569) / 2.0;
    const float BETA = 3950; // should match the Beta Coefficient of the thermistor

    while (true)
    {
        // Write the output buffer to MOSI, and at the same time read from MISO.
        cs_select();
        spi_read_blocking(spi_default, 0xff, in_buf, BUF_LEN);
        cs_deselect();

        // turn the received bits into the 14-bit ADC value
        uint16_t adc_val = ((in_buf[1] << 8) & 0x3F00) | in_buf[0];
        // check to be sure we don't process a startup or spurious value
        if (adc_val > 0)
        {
            printf("adc_val 0x%04x\n", adc_val);
            printf("adc_val %ld\n", adc_val);
            float adc_f = (double)(adc_val / ADC_full_scale * scale);
            // Convert the scaled adc value to degrees Celsius
            float celsius = (1.0 / (log(1.0 / (1023. / adc_f - 1.0)) / BETA + 1.0 / 298.15) - 273.15);
            printf("celsius %ld\n", (long)celsius);
        }
        vTaskDelay(cfg.sample_period_ms);
    }

#endif
}

/*
 * RUNTIME START
 */
int main()
{

    // Enable STDIO
    stdio_init_all();

    // Set up two tasks
    BaseType_t adc_status =
        xTaskCreate(adc_task, "ADC_TASK", 128, &adc_config, 1, &adc_task_handle);

    // Start the FreeRTOS scheduler
    // Only proceed with valid tasks
    if (adc_status == pdPASS)
    {
        vTaskStartScheduler();
    }

    // We should never get here, but just in case...
    while (true)
    {
        // NOP
    }
}