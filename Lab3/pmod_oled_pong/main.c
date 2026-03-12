/******************************************************************************/
/* ECE 315 - Winter 2026
 * Lab 3 Part 2: Pong Game on PmodOLED with FreeRTOS
 *
 * Created by: Claude (AI Assistant)
 * Modified by: Antonio Andara Lara, Winter 2026
 *
 * HARDWARE MAPPINGS (based on provided example):
 * - Pushbuttons: XPAR_GPIO_INPUTS_BASEADDR (bits 0-3 for BTN0-BTN3)
 * - Switches: XPAR_GPIO_INPUTS_BASEADDR (bits 8-11 for SW0-SW3, if available)
 * - Green LEDs: XPAR_GPIO_LEDS_BASEADDR (LD0-LD2 show remaining lives)
 * - RGB LED: XPAR_GPIO_LEDS_BASEADDR, channel 2 (for game state)
 * - OLED: XPAR_GPIO_OLED_BASEADDR, XPAR_SPI_OLED_BASEADDR
 *
 * GAME CONTROLS:
 * - BTN0 (or button 0): Move paddle LEFT
 * - BTN1 (or button 1): Move paddle RIGHT
 * - SW0 (switch 0): Toggle difficulty (LOW=slow, HIGH=fast)
 * - SW1 (switch 1) OR BTN2: Pause/Resume game
 * - BTN3: Reset game (when game over)
 *
 * PERIPHERALS USED (minimum 3):
 * 1. PmodOLED - Display (text + graphics)
 * 2. Pushbuttons - Input for paddle control
 * 3. Switches - Input for difficulty/pause
 * 4. Green LEDs - Output for lives
 * 5. RGB LED - Output for game state
/******************************************************************************/

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

// Xilinx Libraries
#include "xparameters.h"
#include "xgpio.h"
#include "xscugic.h"
#include "xil_exception.h"
#include "xil_printf.h"
#include "xil_cache.h"

// Other libraries
#include <stdlib.h>
#include <stdio.h>
#include "sleep.h"
#include "PmodOLED.h"
#include "OLEDControllerCustom.h"
#include "rgb_led.h"

/******************************************************************************/
/* Device Definitions
/******************************************************************************/
// Button/Switch GPIO
#define BTN_DEVICE_ID XPAR_GPIO_INPUTS_BASEADDR
#define LED_DEVICE_ID XPAR_GPIO_LEDS_BASEADDR
#define BTN_CHANNEL 1
#define GREEN_LED_CHANNEL 1

// OLED Configuration
#define OLED_GPIO_ADDR XPAR_GPIO_OLED_BASEADDR
#define OLED_SPI_ADDR XPAR_SPI_OLED_BASEADDR

/******************************************************************************/
/* Game Constants
/******************************************************************************/
// OLED Display Dimensions
#define OLED_WIDTH OledColMax
#define OLED_HEIGHT OledRowMax

// Game boundaries (keeping 1 pixel margin for border)
#define BORDER_LEFT 1
#define BORDER_RIGHT (OLED_WIDTH - 2)
#define BORDER_TOP 1
#define BORDER_BOTTOM (OLED_HEIGHT - 2)

// Paddle dimensions
#define PADDLE_WIDTH 20
#define PADDLE_HEIGHT 3
#define PADDLE_Y (OLED_HEIGHT - 8) // Near bottom

// Ball dimensions
#define BALL_SIZE 3

// Game speeds (in milliseconds between updates)
#define SPEED_SLOW 80 // Lower difficulty
#define SPEED_FAST 40 // Higher difficulty

// Initial lives
#define INITIAL_LIVES 3

/******************************************************************************/
/* Game State Enumeration
/******************************************************************************/
typedef enum
{
    GAME_PLAYING,
    GAME_PAUSED,
    GAME_GAMEOVER
} GameState;

typedef struct
{
    GameState gameState;
    int lives;
} LedStateMessage;

typedef struct
{
    GameState gameState;
    int score;
    int lives;
    int difficulty;
    int paddleX;
    int ballX;
    int ballY;
} OledStateMessage;

typedef struct
{
    int moveLeft;
    int moveRight;
    int difficulty;
    int resetPressed;
    u32 pauseToggleSequence;
} InputStateMessage;

typedef struct
{
    GameState gameState;
    int score;
    int lives;
    int paddleX;
    int ballX;
    int ballY;
    int ballVelX;
    int ballVelY;
} GameData;

/******************************************************************************/
/* Global Variables
******************************************************************************/
// OLED Device
PmodOLED oledDevice;

// GPIO Instances
XGpio btnInst;
XGpio ledInst;
XGpio swInst;

// Task handles
static TaskHandle_t xGameTaskHandle = NULL;
static TaskHandle_t xOledTaskHandle = NULL;

// Queues carrying the latest task snapshots.
static QueueHandle_t xInputStateQueue = NULL;
static QueueHandle_t xLedStateQueue = NULL;
static QueueHandle_t xOledStateQueue = NULL;

/******************************************************************************/
/* Function Prototypes
******************************************************************************/
static void vInputTask(void *pvParameters);
static void vGameTask(void *pvParameters);
static void vOledTask(void *pvParameters);
static void vLedTask(void *pvParameters);

static void initGame(GameData *game);
static void resetBall(GameData *game);
static void updatePaddle(GameData *game, int direction);
static void updateBall(GameData *game);
static void drawBorder(void);
static void drawPaddle(const OledStateMessage *state);
static void drawBall(const OledStateMessage *state);
static void drawScore(const OledStateMessage *state);
static void drawState(const OledStateMessage *state);

/******************************************************************************/
/* Main Function
******************************************************************************/
int main()
{
    int status;
    GameData initialGame;
    InputStateMessage inputState = {0, 0, 0, 0, 0};
    LedStateMessage ledState;
    OledStateMessage oledState;

    xil_printf("Initializing Pong Game...\r\n");

    // Initialize OLED
    // orientation: 0 = normal, invert: 0 = normal colors (black background)
    OLED_Begin(&oledDevice, OLED_GPIO_ADDR, OLED_SPI_ADDR, 1, 0);
    OLED_SetDrawMode(&oledDevice, 0);
    OLED_SetCharUpdate(&oledDevice, 0); // Manual update

    // Initialize Buttons
    status = XGpio_Initialize(&btnInst, BTN_DEVICE_ID);
    if (status != XST_SUCCESS)
    {
        xil_printf("GPIO Button Initialization failed.\r\n");
        return XST_FAILURE;
    }
    XGpio_SetDataDirection(&btnInst, BTN_CHANNEL, 0xFF); // btn inputs

    // Initialize LEDs
    status = XGpio_Initialize(&ledInst, LED_DEVICE_ID);
    if (status != XST_SUCCESS)
    {
        xil_printf("GPIO LED Initialization failed.\r\n");
        return XST_FAILURE;
    }
    XGpio_SetDataDirection(&ledInst, GREEN_LED_CHANNEL, 0x0);
    XGpio_SetDataDirection(&ledInst, RGB_CHANNEL, 0x0);
    XGpio_DiscreteWrite(&ledInst, GREEN_LED_CHANNEL, 0x0);
    XGpio_DiscreteWrite(&ledInst, RGB_CHANNEL, RGB_OFF);

    // Initialize SWITCHs
    status = XGpio_Initialize(&swInst, BTN_DEVICE_ID);
    if (status != XST_SUCCESS)
    {
        xil_printf("GPIO SWITCH Initialization failed.\r\n");
        return XST_FAILURE;
    }

    XGpio_SetDataDirection(&swInst, 2, 0xFF); // Switch inputs

    xInputStateQueue = xQueueCreate(1, sizeof(InputStateMessage));
    if (xInputStateQueue == NULL)
    {
        xil_printf("Input state queue creation failed.\r\n");
        return XST_FAILURE;
    }

    xLedStateQueue = xQueueCreate(1, sizeof(LedStateMessage));
    if (xLedStateQueue == NULL)
    {
        xil_printf("LED state queue creation failed.\r\n");
        return XST_FAILURE;
    }

    xOledStateQueue = xQueueCreate(1, sizeof(OledStateMessage));
    if (xOledStateQueue == NULL)
    {
        xil_printf("OLED state queue creation failed.\r\n");
        return XST_FAILURE;
    }

    xil_printf("Initialization Complete!\r\n");
    xil_printf("Controls: BTN0=Left, BTN1=Right, SW0=Difficulty, SW1/BTN2=Pause, BTN3=Reset\r\n");

    // Create tasks
    xTaskCreate(vInputTask,
                "Input Task",
                configMINIMAL_STACK_SIZE * 2,
                NULL,
                tskIDLE_PRIORITY + 2,
                NULL);

    xTaskCreate(vGameTask,
                "Game Task",
                configMINIMAL_STACK_SIZE * 2,
                NULL,
                tskIDLE_PRIORITY + 1,
                &xGameTaskHandle);

    xTaskCreate(vOledTask,
                "OLED Task",
                configMINIMAL_STACK_SIZE * 2,
                NULL,
                tskIDLE_PRIORITY + 1,
                &xOledTaskHandle);

    xTaskCreate(vLedTask,
                "LED Task",
                configMINIMAL_STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY + 1,
                NULL);

    initGame(&initialGame);

    xQueueOverwrite(xInputStateQueue, &inputState);

    ledState.gameState = initialGame.gameState;
    ledState.lives = initialGame.lives;
    xQueueOverwrite(xLedStateQueue, &ledState);

    oledState.gameState = initialGame.gameState;
    oledState.score = initialGame.score;
    oledState.lives = initialGame.lives;
    oledState.difficulty = inputState.difficulty;
    oledState.paddleX = initialGame.paddleX;
    oledState.ballX = initialGame.ballX;
    oledState.ballY = initialGame.ballY;
    xQueueOverwrite(xOledStateQueue, &oledState);

    // Start scheduler
    vTaskStartScheduler();

    while (1)
        ; // Should never reach here

    return 0;
}

/******************************************************************************/
/* Input Task - Polls pushbuttons and switches
/******************************************************************************/
static void vInputTask(void *pvParameters)
{
    u32 buttonVal;
    u32 swVal;
    const TickType_t xDelay = pdMS_TO_TICKS(20);
    InputStateMessage inputState = {0, 0, 0, 0, 0};
    int pausePrev = 0;

    while (1)
    {
        // Read button/switch values
        buttonVal = XGpio_DiscreteRead(&btnInst, BTN_CHANNEL);
        swVal = XGpio_DiscreteRead(&swInst, 2);

        // Extract individual button/switch states
        // Bits 0-3: Buttons BTN0-BTN3
        // Bits 8-11: Switches SW0-SW3 (if available in hardware)

        // BTN0 (bit 0): Move paddle left
        inputState.moveLeft = (buttonVal & 0x01) ? 1 : 0;

        // BTN1 (bit 1): Move paddle right
        inputState.moveRight = (buttonVal & 0x02) ? 1 : 0;

        // SW0: Difficulty toggle
        inputState.difficulty = (swVal & 0x01) ? 1 : 0;

        // SW1 OR BTN2 (bit 2): Pause toggle
        // Use edge detection for pause (only toggle once per press)
        int pause_now = ((swVal & 0x02) || (buttonVal & 0x04)) ? 1 : 0;
        if (pause_now && !pausePrev)
        {
            inputState.pauseToggleSequence++;
        }
        pausePrev = pause_now;

        // BTN3 (bit 3): Reset game (only when game over)
        inputState.resetPressed = (buttonVal & 0x08) ? 1 : 0;

        xQueueOverwrite(xInputStateQueue, &inputState);

        vTaskDelay(xDelay);
    }
}

/******************************************************************************/
/* Game Task - Updates game logic
/******************************************************************************/
static void vGameTask(void *pvParameters)
{
    TickType_t xDelay;
    int speed;
    GameData game;
    InputStateMessage inputState = {0, 0, 0, 0, 0};
    InputStateMessage latestInput;
    LedStateMessage ledState;
    OledStateMessage oledState;
    u32 lastPauseToggleSequence = 0;

    initGame(&game);

    while (1)
    {
        if (xQueueReceive(xInputStateQueue, &latestInput, 0) == pdTRUE)
        {
            inputState = latestInput;
        }

        // Determine speed based on difficulty
        speed = inputState.difficulty ? SPEED_FAST : SPEED_SLOW;
        xDelay = pdMS_TO_TICKS(speed);

        // Handle pause toggle
        if (inputState.pauseToggleSequence != lastPauseToggleSequence)
        {
            if (((inputState.pauseToggleSequence - lastPauseToggleSequence) & 1U) != 0U)
            {
                if (game.gameState == GAME_PLAYING)
                {
                    game.gameState = GAME_PAUSED;
                    xil_printf("Game Paused\r\n");
                }
                else if (game.gameState == GAME_PAUSED)
                {
                    game.gameState = GAME_PLAYING;
                    xil_printf("Game Resumed\r\n");
                }
            }

            lastPauseToggleSequence = inputState.pauseToggleSequence;
        }

        // Handle reset (only when game over)
        if (inputState.resetPressed && game.gameState == GAME_GAMEOVER)
        {
            initGame(&game);
            xil_printf("Game Reset\r\n");
        }

        // Only update game when playing
        if (game.gameState == GAME_PLAYING)
        {
            // Update paddle position
            if (inputState.moveLeft)
            {
                updatePaddle(&game, -1);
            }
            if (inputState.moveRight)
            {
                updatePaddle(&game, 1);
            }

            // Update ball
            updateBall(&game);
        }

        ledState.gameState = game.gameState;
        ledState.lives = game.lives;
        xQueueOverwrite(xLedStateQueue, &ledState);

        oledState.gameState = game.gameState;
        oledState.score = game.score;
        oledState.lives = game.lives;
        oledState.difficulty = inputState.difficulty;
        oledState.paddleX = game.paddleX;
        oledState.ballX = game.ballX;
        oledState.ballY = game.ballY;
        xQueueOverwrite(xOledStateQueue, &oledState);

        vTaskDelay(xDelay);
    }
}

/******************************************************************************/
/* OLED Task - Draws everything to the display
/******************************************************************************/
static void vOledTask(void *pvParameters)
{
    const TickType_t xDelay = pdMS_TO_TICKS(50);
    OledStateMessage oledState = {
        GAME_PLAYING,
        0,
        INITIAL_LIVES,
        0,
        (OLED_WIDTH - PADDLE_WIDTH) / 2,
        OLED_WIDTH / 2,
        OLED_HEIGHT / 3};
    
    OledStateMessage dummyState;

    while (1)
    {
        if (xQueueReceive(xOledStateQueue, &dummyState, 0) == pdTRUE)
        {
            oledState = dummyState;
        }

        // Clear buffer
        OLED_ClearBuffer(&oledDevice);

        // Draw game elements
        drawBorder();
        drawPaddle(&oledState);
        drawBall(&oledState);
        drawScore(&oledState);
        drawState(&oledState);

        // Update display
        OLED_Update(&oledDevice);

        vTaskDelay(xDelay);
    }
}

/******************************************************************************/
/* LED Task - Controls LEDs based on game state
/******************************************************************************/
static void vLedTask(void *pvParameters)
{
    u32 greenLedValue;
    u32 rgbValue;
    const TickType_t xDelay = pdMS_TO_TICKS(100);
    LedStateMessage ledState = {GAME_PLAYING, INITIAL_LIVES};
    LedStateMessage dummyState;

    while (1)
    {
        if (xQueueReceive(xLedStateQueue, &dummyState, 0) == pdTRUE)
        {
            ledState = dummyState;
        }

        // Green LEDs show remaining lives on channel 1.
        greenLedValue = ((u32)ledState.lives) & 0x07;

        // RGB LED shows game state on channel 2 using 3-bit color codes.
        rgbValue = RGB_OFF;
        if (ledState.gameState == GAME_PLAYING)
        {
            rgbValue = RGB_GREEN;
        }
        else if (ledState.gameState == GAME_PAUSED)
        {
            rgbValue = RGB_BLUE;
        }
        else if (ledState.gameState == GAME_GAMEOVER)
        {
            rgbValue = RGB_RED;
        }

        XGpio_DiscreteWrite(&ledInst, GREEN_LED_CHANNEL, greenLedValue);
        XGpio_DiscreteWrite(&ledInst, RGB_CHANNEL, rgbValue);

        vTaskDelay(xDelay);
    }
}

/******************************************************************************/
/* Game Helper Functions
******************************************************************************/

// Initialize/Reset game state
static void initGame(GameData *game)
{
    game->gameState = GAME_PLAYING;
    game->score = 0;
    game->lives = INITIAL_LIVES;
    game->paddleX = (OLED_WIDTH - PADDLE_WIDTH) / 2;
    resetBall(game);
}

// Reset ball to starting position
static void resetBall(GameData *game)
{
    game->ballX = OLED_WIDTH / 2;
    game->ballY = OLED_HEIGHT / 3;
    // Randomize horizontal direction
    game->ballVelX = (rand() % 2 == 0) ? 1 : -1;
    game->ballVelY = -1; // Always start moving up
}

// Update paddle position
static void updatePaddle(GameData *game, int direction)
{
    game->paddleX += direction * 2; // Move 2 pixels at a time

    // Clamp to boundaries (accounting for paddle width)
    if (game->paddleX < BORDER_LEFT)
    {
        game->paddleX = BORDER_LEFT;
    }
    if (game->paddleX + PADDLE_WIDTH > BORDER_RIGHT)
    {
        game->paddleX = BORDER_RIGHT - PADDLE_WIDTH;
    }
}

// Update ball position and handle collisions
static void updateBall(GameData *game)
{
    int newX, newY;

    newX = game->ballX + game->ballVelX;
    newY = game->ballY + game->ballVelY;

    // Wall collisions (left, right, top)
    if (newX <= BORDER_LEFT)
    {
        newX = BORDER_LEFT + 1;
        game->ballVelX = -game->ballVelX;
    }
    if (newX + BALL_SIZE >= BORDER_RIGHT)
    {
        newX = BORDER_RIGHT - BALL_SIZE - 1;
        game->ballVelX = -game->ballVelX;
    }
    if (newY <= BORDER_TOP)
    {
        newY = BORDER_TOP + 1;
        game->ballVelY = -game->ballVelY;
    }

    // Paddle collision
    if (newY + BALL_SIZE >= PADDLE_Y &&
        newY + BALL_SIZE <= PADDLE_Y + PADDLE_HEIGHT &&
        newX + BALL_SIZE >= game->paddleX &&
        newX <= game->paddleX + PADDLE_WIDTH)
    {

        // Ball hit paddle - bounce up
        game->ballVelY = -game->ballVelY;
        newY = PADDLE_Y - BALL_SIZE - 1;

        // Add some horizontal velocity variation based on where it hit paddle
        int hitPos = (newX + BALL_SIZE / 2) - (game->paddleX + PADDLE_WIDTH / 2);
        game->ballVelX = hitPos / 4; // Adjust horizontal speed

        // Keep minimum horizontal speed
        if (game->ballVelX == 0)
        {
            game->ballVelX = (rand() % 2 == 0) ? 1 : -1;
        }

        // Increase score
        game->score++;
    }

    // Ball went past paddle (bottom)
    if (newY > BORDER_BOTTOM)
    {
        // Lost a life
        game->lives--;

        xil_printf("Life lost! Lives: %d, Score: %d\r\n", game->lives, game->score);

        if (game->lives <= 0)
        {
            game->gameState = GAME_GAMEOVER;
            xil_printf("GAME OVER! Final Score: %d\r\n", game->score);
        }
        else
        {
            resetBall(game);
        }
    }

    // Update ball position
    game->ballX = newX;
    game->ballY = newY;
}

/******************************************************************************/
/* Drawing Functions
******************************************************************************/

// Draw game border
static void drawBorder(void)
{
    OLED_MoveTo(&oledDevice, BORDER_LEFT, BORDER_TOP);
    OLED_RectangleTo(&oledDevice, BORDER_RIGHT, BORDER_BOTTOM);
}

// Draw paddle
static void drawPaddle(const OledStateMessage *state)
{
    int i, j;

    // Draw filled rectangle for paddle
    for (i = 0; i < PADDLE_HEIGHT; i++)
    {
        OLED_MoveTo(&oledDevice, state->paddleX, PADDLE_Y + i);
        for (j = 0; j < PADDLE_WIDTH; j++)
        {
            OLED_DrawLineTo(&oledDevice, state->paddleX + j, PADDLE_Y + i);
        }
    }
}

// Draw ball
static void drawBall(const OledStateMessage *state)
{
    int i, j;

    // Draw ball as small filled square
    for (i = 0; i < BALL_SIZE; i++)
    {
        OLED_MoveTo(&oledDevice, state->ballX, state->ballY + i);
        for (j = 0; j < BALL_SIZE; j++)
        {
            OLED_DrawLineTo(&oledDevice, state->ballX + j, state->ballY + i);
        }
    }
}

// Draw score
static void drawScore(const OledStateMessage *state)
{
    char scoreStr[16];

    OLED_SetCursor(&oledDevice, 0, 0);
    snprintf(scoreStr, sizeof(scoreStr), "S:%d", state->score);
    OLED_PutString(&oledDevice, scoreStr);
}

// Draw game state
static void drawState(const OledStateMessage *state)
{
    char stateStr[16];

    if (state->gameState == GAME_PAUSED)
    {
        OLED_SetCursor(&oledDevice, 8, 1);
        OLED_PutString(&oledDevice, "PAUSED");
    }
    else if (state->gameState == GAME_GAMEOVER)
    {
        OLED_SetCursor(&oledDevice, 5, 1);
        OLED_PutString(&oledDevice, "GAME OVER");
        OLED_SetCursor(&oledDevice, 6, 2);
        snprintf(stateStr, sizeof(stateStr), "Score:%d", state->score);
        OLED_PutString(&oledDevice, stateStr);
    }
    else
    {
        // Show difficulty and lives
        OLED_SetCursor(&oledDevice, 10, 0);
        OLED_PutString(&oledDevice, state->difficulty ? "F" : "S");

        // Lives as hearts or simple indicator
        OLED_SetCursor(&oledDevice, 12, 0);
        snprintf(stateStr, sizeof(stateStr), "L:%d", state->lives);
        OLED_PutString(&oledDevice, stateStr);
    }
}
