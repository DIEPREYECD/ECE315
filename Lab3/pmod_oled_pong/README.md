# Pong Game - ECE 315 Lab 3 Part 2

## Project Overview

This is a simple Pong-like game for the Zybo board with FreeRTOS, using the PmodOLED for display.

## Hardware Requirements

- Zybo Z7-10 or compatible board
- PmodOLED display
- Pushbuttons (4 buttons)
- Switches (at least 2 switches)
- LEDs (4 user LEDs + RGB LED)

## Peripherals Used (minimum 3 required)

1. **PmodOLED** - Display (text + graphics) - 128x32 pixels
2. **Pushbuttons** - Input for paddle movement (BTN0=left, BTN1=right)
3. **Switches** - Input for difficulty (SW0) and pause (SW1)
4. **Green LEDs** - Output showing remaining lives (LD0-LD2)
5. **RGB LED** - Output showing game state

## Controls

| Input | Action |
|-------|--------|
| BTN0 | Move paddle LEFT |
| BTN1 | Move paddle RIGHT |
| SW0 | Toggle difficulty (LOW=slow, HIGH=fast) |
| SW1 or BTN2 | Pause/Resume game |
| BTN3 | Reset game (when game over) |

## Gameplay

- Paddle is positioned near the bottom of the screen
- Ball bounces off left, right, and top walls
- If ball hits paddle, it bounces back up and score increases
- If ball misses paddle (hits bottom), player loses one life
- Game ends when lives reach 0
- Green LEDs show remaining lives (3 LEDs = 3 lives)
- RGB LED indicates game state:
  - Green = Playing
  - Blue = Paused
  - Red = Game Over

## File Structure

```
pmod_oled_pong/
├── main.c              # Main game code
├── README.md           # This file
└── (uses OLED drivers from pmod_oled_example/)
```

## Required Source Files (from pmod_oled_example)

The following files should be included in the SDK project:

- PmodOLED.c / PmodOLED.h
- OLEDControllerCustom.c / OLEDControllerCustom.h
- OledDriver.c
- OledChar.c
- OledGrph.c
- ChrFont0.c / ChrFont0.h
- FillPat.c / FillPat.h

## Building

1. Open Vivado and create a new SDK project or use existing hardware design
2. Import the source files from both:
   - `pmod_oled_example/` (OLED drivers)
   - `pmod_oled_pong/` (main.c)
3. Build and program the FPGA
4. Run the application on the board

## Notes

- The game uses FreeRTOS tasks for input polling, game logic, and display updates
- OLED update rate is ~20 FPS (50ms delay)
- Game speed varies from 40ms (fast) to 80ms (slow) per frame
- Score increases by 1 for each paddle hit