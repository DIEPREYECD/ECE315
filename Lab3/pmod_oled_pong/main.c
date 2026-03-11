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
#define BTN_DEVICE_ID       XPAR_GPIO_INPUTS_BASEADDR
#define LED_DEVICE_ID       XPAR_GPIO_LEDS_BASEADDR
#define BTN_CHANNEL          1
#define GREEN_LED_CHANNEL    1

// OLED Configuration
#define OLED_GPIO_ADDR       XPAR_GPIO_OLED_BASEADDR
#define OLED_SPI_ADDR        XPAR_SPI_OLED_BASEADDR

/******************************************************************************/
/* Game Constants
/******************************************************************************/
// OLED Display Dimensions
#define OLED_WIDTH   OledColMax
#define OLED_HEIGHT OledRowMax

// Game boundaries (keeping 1 pixel margin for border)
#define BORDER_LEFT     1
#define BORDER_RIGHT    (OLED_WIDTH - 2)
#define BORDER_TOP      1
#define BORDER_BOTTOM   (OLED_HEIGHT - 2)

// Paddle dimensions
#define PADDLE_WIDTH    20
#define PADDLE_HEIGHT   3
#define PADDLE_Y        (OLED_HEIGHT - 8)  // Near bottom

// Ball dimensions
#define BALL_SIZE       3

// Game speeds (in milliseconds between updates)
#define SPEED_SLOW      80   // Lower difficulty
#define SPEED_FAST      40   // Higher difficulty

// Initial lives
#define INITIAL_LIVES   3

/******************************************************************************/
/* Game State Enumeration
/******************************************************************************/
typedef enum {
    GAME_PLAYING,
    GAME_PAUSED,
    GAME_GAMEOVER
} GameState;

/******************************************************************************/
/* Global Variables
******************************************************************************/
// OLED Device
PmodOLED oledDevice;

// GPIO Instances
XGpio btnInst;
XGpio ledInst;

// Note: LED channel 1 = Green LEDs (LD0-LD2 for lives)
//       LED channel 2 = RGB LED (for game state)
static volatile GameState gameState = GAME_PLAYING;
static volatile int score = 0;
static volatile int lives = INITIAL_LIVES;
static volatile int difficulty = 0;  // 0 = slow, 1 = fast

// Paddle position
static volatile int paddleX = (OLED_WIDTH - PADDLE_WIDTH) / 2;

// Ball position and velocity
static volatile int ballX = OLED_WIDTH / 2;
static volatile int ballY = OLED_HEIGHT / 2;
static volatile int ballVelX = 1;
static volatile int ballVelY = -1;

// Input flags (set by input task, consumed by game task)
static volatile int moveLeft = 0;
static volatile int moveRight = 0;
static volatile int pauseToggle = 0;
static volatile int resetGame = 0;

// Task handles
static TaskHandle_t xGameTaskHandle = NULL;
static TaskHandle_t xOledTaskHandle = NULL;

/******************************************************************************/
/* Function Prototypes
******************************************************************************/
static void vInputTask(void *pvParameters);
static void vGameTask(void *pvParameters);
static void vOledTask(void *pvParameters);
static void vLedTask(void *pvParameters);

static void initGame(void);
static void resetBall(void);
static void updatePaddle(int direction);
static void updateBall(void);
static int checkPaddleCollision(void);
static void drawBorder(void);
static void drawPaddle(void);
static void drawBall(void);
static void drawScore(void);
static void drawState(void);

/******************************************************************************/
/* Main Function
******************************************************************************/
int main()
{
    int status;

    xil_printf("Initializing Pong Game...\r\n");

    // Initialize OLED
    // orientation: 0 = normal, invert: 0 = normal colors (black background)
    OLED_Begin(&oledDevice, OLED_GPIO_ADDR, OLED_SPI_ADDR, 0, 0);
    OLED_SetDrawMode(&oledDevice, 0);
    OLED_SetCharUpdate(&oledDevice, 0);  // Manual update

    // Initialize Buttons
    status = XGpio_Initialize(&btnInst, BTN_DEVICE_ID);
    if (status != XST_SUCCESS) {
        xil_printf("GPIO Button Initialization failed.\r\n");
        return XST_FAILURE;
    }
    XGpio_SetDataDirection(&btnInst, BTN_CHANNEL, 0xFF);  // All inputs

    // Initialize LEDs
    status = XGpio_Initialize(&ledInst, LED_DEVICE_ID);
    if (status != XST_SUCCESS) {
        xil_printf("GPIO LED Initialization failed.\r\n");
        return XST_FAILURE;
    }
    XGpio_SetDataDirection(&ledInst, GREEN_LED_CHANNEL, 0x0);
    XGpio_SetDataDirection(&ledInst, RGB_CHANNEL, 0x0);
    XGpio_DiscreteWrite(&ledInst, GREEN_LED_CHANNEL, 0x0);
    XGpio_DiscreteWrite(&ledInst, RGB_CHANNEL, RGB_OFF);

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

    // Initialize game
    initGame();

    // Start scheduler
    vTaskStartScheduler();

    while(1);  // Should never reach here

    return 0;
}

/******************************************************************************/
/* Input Task - Polls pushbuttons and switches
/******************************************************************************/
static void vInputTask(void *pvParameters)
{
    u32 buttonVal;
    const TickType_t xDelay = pdMS_TO_TICKS(20);

    while (1) {
        // Read button/switch values
        buttonVal = XGpio_DiscreteRead(&btnInst, BTN_CHANNEL);

        // Extract individual button/switch states
        // Bits 0-3: Buttons BTN0-BTN3
        // Bits 8-11: Switches SW0-SW3 (if available in hardware)

        // BTN0 (bit 0): Move paddle left
        if (buttonVal & 0x01) {
            moveLeft = 1;
        }

        // BTN1 (bit 1): Move paddle right
        if (buttonVal & 0x02) {
            moveRight = 1;
        }

        // SW0 (bit 8): Difficulty toggle
        if (buttonVal & 0x100) {
            difficulty = 1;  // Fast
        } else {
            difficulty = 0;  // Slow
        }

        // SW1 (bit 9) OR BTN2 (bit 2): Pause toggle
        // Use edge detection for pause (only toggle once per press)
        static int pause_prev = 0;
        int pause_now = ((buttonVal & 0x200) || (buttonVal & 0x04)) ? 1 : 0;
        if (pause_now && !pause_prev) {
            pauseToggle = 1;
        }
        pause_prev = pause_now;

        // BTN3 (bit 3): Reset game (only when game over)
        if (buttonVal & 0x08) {
            resetGame = 1;
        }

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

    while (1) {
        // Determine speed based on difficulty
        speed = difficulty ? SPEED_FAST : SPEED_SLOW;
        xDelay = pdMS_TO_TICKS(speed);

        // Handle pause toggle
        if (pauseToggle) {
            pauseToggle = 0;
            if (gameState == GAME_PLAYING) {
                gameState = GAME_PAUSED;
                xil_printf("Game Paused\r\n");
            } else if (gameState == GAME_PAUSED) {
                gameState = GAME_PLAYING;
                xil_printf("Game Resumed\r\n");
            }
        }

        // Handle reset (only when game over)
        if (resetGame && gameState == GAME_GAMEOVER) {
            resetGame = 0;
            initGame();
            xil_printf("Game Reset\r\n");
        }

        // Only update game when playing
        if (gameState == GAME_PLAYING) {
            // Update paddle position
            if (moveLeft) {
                updatePaddle(-1);
                moveLeft = 0;
            }
            if (moveRight) {
                updatePaddle(1);
                moveRight = 0;
            }

            // Update ball
            updateBall();
        }

        vTaskDelay(xDelay);
    }
}

/******************************************************************************/
/* OLED Task - Draws everything to the display
/******************************************************************************/
static void vOledTask(void *pvParameters)
{
    const TickType_t xDelay = pdMS_TO_TICKS(50);

    while (1) {
        // Clear buffer
        OLED_ClearBuffer(&oledDevice);

        // Draw game elements
        drawBorder();
        drawPaddle();
        drawBall();
        drawScore();
        drawState();

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

    while (1) {
        // Green LEDs show remaining lives on channel 1.
        greenLedValue = lives & 0x07;

        // RGB LED shows game state on channel 2 using 3-bit color codes.
        rgbValue = RGB_OFF;
        if (gameState == GAME_PLAYING) {
            rgbValue = RGB_GREEN;
        } else if (gameState == GAME_PAUSED) {
            rgbValue = RGB_BLUE;
        } else if (gameState == GAME_GAMEOVER) {
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
static void initGame(void)
{
    gameState = GAME_PLAYING;
    score = 0;
    lives = INITIAL_LIVES;
    paddleX = (OLED_WIDTH - PADDLE_WIDTH) / 2;
    resetBall();
}

// Reset ball to starting position
static void resetBall(void)
{
    ballX = OLED_WIDTH / 2;
    ballY = OLED_HEIGHT / 3;
    // Randomize horizontal direction
    ballVelX = (rand() % 2 == 0) ? 1 : -1;
    ballVelY = -1;  // Always start moving up
}

// Update paddle position
static void updatePaddle(int direction)
{
    paddleX += direction * 2;  // Move 2 pixels at a time

    // Clamp to boundaries (accounting for paddle width)
    if (paddleX < BORDER_LEFT) {
        paddleX = BORDER_LEFT;
    }
    if (paddleX + PADDLE_WIDTH > BORDER_RIGHT) {
        paddleX = BORDER_RIGHT - PADDLE_WIDTH;
    }
}

// Update ball position and handle collisions
static void updateBall(void)
{
    int newX, newY;

    newX = ballX + ballVelX;
    newY = ballY + ballVelY;

    // Wall collisions (left, right, top)
    if (newX <= BORDER_LEFT) {
        newX = BORDER_LEFT + 1;
        ballVelX = -ballVelX;
    }
    if (newX + BALL_SIZE >= BORDER_RIGHT) {
        newX = BORDER_RIGHT - BALL_SIZE - 1;
        ballVelX = -ballVelX;
    }
    if (newY <= BORDER_TOP) {
        newY = BORDER_TOP + 1;
        ballVelY = -ballVelY;
    }

    // Paddle collision
    if (newY + BALL_SIZE >= PADDLE_Y &&
        newY + BALL_SIZE <= PADDLE_Y + PADDLE_HEIGHT &&
        newX + BALL_SIZE >= paddleX &&
        newX <= paddleX + PADDLE_WIDTH) {

        // Ball hit paddle - bounce up
        ballVelY = -ballVelY;
        newY = PADDLE_Y - BALL_SIZE - 1;

        // Add some horizontal velocity variation based on where it hit paddle
        int hitPos = (newX + BALL_SIZE/2) - (paddleX + PADDLE_WIDTH/2);
        ballVelX = hitPos / 4;  // Adjust horizontal speed

        // Keep minimum horizontal speed
        if (ballVelX == 0) {
            ballVelX = (rand() % 2 == 0) ? 1 : -1;
        }

        // Increase score
        score++;
    }

    // Ball went past paddle (bottom)
    if (newY > BORDER_BOTTOM) {
        // Lost a life
        lives--;

        xil_printf("Life lost! Lives: %d, Score: %d\r\n", lives, score);

        if (lives <= 0) {
            gameState = GAME_GAMEOVER;
            xil_printf("GAME OVER! Final Score: %d\r\n", score);
        } else {
            resetBall();
        }
    }

    // Update ball position
    ballX = newX;
    ballY = newY;
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
static void drawPaddle(void)
{
    int i, j;

    // Draw filled rectangle for paddle
    for (i = 0; i < PADDLE_HEIGHT; i++) {
        OLED_MoveTo(&oledDevice, paddleX, PADDLE_Y + i);
        for (j = 0; j < PADDLE_WIDTH; j++) {
            OLED_DrawPixel(&oledDevice);
        }
    }
}

// Draw ball
static void drawBall(void)
{
    int i, j;

    // Draw ball as small filled square
    for (i = 0; i < BALL_SIZE; i++) {
        OLED_MoveTo(&oledDevice, ballX, ballY + i);
        for (j = 0; j < BALL_SIZE; j++) {
            OLED_DrawPixel(&oledDevice);
        }
    }
}

// Draw score
static void drawScore(void)
{
    char scoreStr[16];

    OLED_SetCursor(&oledDevice, 0, 0);
    snprintf(scoreStr, sizeof(scoreStr), "S:%d", score);
    OLED_PutString(&oledDevice, scoreStr);
}

// Draw game state
static void drawState(void)
{
    char stateStr[16];

    if (gameState == GAME_PAUSED) {
        OLED_SetCursor(&oledDevice, 8, 1);
        OLED_PutString(&oledDevice, "PAUSED");
    } else if (gameState == GAME_GAMEOVER) {
        OLED_SetCursor(&oledDevice, 5, 1);
        OLED_PutString(&oledDevice, "GAME OVER");
        OLED_SetCursor(&oledDevice, 6, 2);
        snprintf(stateStr, sizeof(stateStr), "Score:%d", score);
        OLED_PutString(&oledDevice, stateStr);
    } else {
        // Show difficulty and lives
        OLED_SetCursor(&oledDevice, 12, 0);
        OLED_PutString(&oledDevice, difficulty ? "F" : "S");

        // Lives as hearts or simple indicator
        OLED_SetCursor(&oledDevice, 14, 0);
        snprintf(stateStr, sizeof(stateStr), "L:%d", lives);
        OLED_PutString(&oledDevice, stateStr);
    }
}
