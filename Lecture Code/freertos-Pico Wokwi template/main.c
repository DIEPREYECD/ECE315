/**
 * ECE 315 Computer Interfacing
 *
 * @file main.c
 * @details A simple FreeRTOS template to start projects.
 *          A producer-consumer task pair blinks the built-in LED.
 * @author Steven Knudsen, CCID knud
 * @copyright 2025, University of Alberta
 * @version   1.0
 * @licence   MIT
 */
#include "main.h"

/**
 * uncomment to enable the blocking task
 */
// #define WITH_BLOCKING 1

/*
 * GLOBALS
 */
// This is the inter-task queue
volatile QueueHandle_t queue = NULL;

// Set a delay time of exactly 500ms
const TickType_t ms_delay = 500 / portTICK_PERIOD_MS;

// Our tasks
TaskHandle_t pico_producer_task_handle = NULL;
TaskHandle_t pico_consumer_task_handle = NULL;

TaskHandle_t blocking_task_handle = NULL;

/*
 * FUNCTIONS
 */

/**
 * @brief Return number of milliseconds since program start
 */
uint64_t get_ms_freertos()
{
    return xTaskGetTickCount() / portTICK_PERIOD_MS;
}

/**
 * @brief Block to show preemption
 */
void blocking_task(void *unused_arg)
{
    while (true)
    {
        printf("%-9llums blocking task\n", get_ms_freertos());
        vTaskDelay(ms_delay);
    }
}

/**
 * @brief Repeatedly flash the Pico's built-in LED.
 */
void led_task_consumer(void *unused_arg)
{

    // This variable will take a copy of the value
    // added to the FreeRTOS xQueue
    uint8_t passed_value_buffer = 0;

    // Configure the Pico's on-board LED
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    while (true)
    {
        // Check for an item in the FreeRTOS xQueue
        if (xQueueReceive(queue, &passed_value_buffer, portMAX_DELAY) == pdPASS)
        {
            // Received a value so flash the GPIO LED accordingly
            // (NOT the sent value)

            printf("%-9llums consume %d\n", get_ms_freertos(), passed_value_buffer);
            gpio_put(PICO_DEFAULT_LED_PIN, passed_value_buffer == 1 ? 0 : 1);
        }
    }
}

/**
 * @brief Repeatedly flash the Pico's built-in LED.
 */
void led_task_producer(void *unused_arg)
{

    // Store the Pico LED state
    uint8_t pico_led_state = 0;

    while (true)
    {
        // Add the LED state to the FreeRTOS xQUEUE
        pico_led_state = !pico_led_state; // toggle LED state
        printf("%-9llums produce %d\n", get_ms_freertos(), pico_led_state);
        xQueueSendToBack(queue, &pico_led_state, 0);
        vTaskDelay(ms_delay);
    }
}

/*
 * RUNTIME START
 */
int main()
{

    // Enable STDIO
    stdio_init_all();

    // Set up two tasks
    // Store handles referencing the tasks; get return values
    // NOTE Arg 3 is the stack depth -- in words, not bytes
    BaseType_t prod_status = xTaskCreate(led_task_producer,
                                         "PICO_PRODUCER_TASK",
                                         1024,
                                         NULL,
                                         1,
                                         &pico_producer_task_handle);
    BaseType_t con_status = xTaskCreate(led_task_consumer,
                                        "PICO_CONSUMER_TASK",
                                        1024,
                                        NULL,
                                        1,
                                        &pico_consumer_task_handle);
#if WITH_BLOCKING
    BaseType_t block_status = xTaskCreate(blocking_task,
                                          "BLOCKING_TASK",
                                          1024,
                                          NULL,
                                          1,
                                          &blocking_task_handle);
#else
    BaseType_t block_status = pdPASS;
#endif

    // Set up the event queue
    queue = xQueueCreate(4, sizeof(uint8_t));

    // Start the FreeRTOS scheduler
    // Only proceed with valid tasks
    if (prod_status == pdPASS || con_status == pdPASS || block_status == pdPASS)
    {
        vTaskStartScheduler();
    }

    // We should never get here, but just in case...
    while (true)
    {
        // NOP
    }
}