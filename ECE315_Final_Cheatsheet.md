# ECE 315 Final Cheatsheet

## Use This Sheet For
- Memorize theory, setup order, and interface-selection logic.
- Look up exact register names, bit fields, and peripheral-specific limits in the datasheet or lecture notes only after you know the sequence.
- If asked "how would you set up X in FreeRTOS C?", think:
  `clock/reset -> pin mux -> peripheral config -> RTOS objects -> IRQ/DMA -> tasks -> test/timeout`

## Most-Tested Topics From Quizzes and Assignments
- Embedded systems, hard vs soft real-time, deterministic behavior, excess computational capacity
- Harvard / von Neumann / hybrid architectures, MCU vs MPU, SoC contents, memory-mapped I/O, AHB
- FreeRTOS scheduling, context switching, TCBs, mutexes, semaphores, queues, event groups
- Priority inversion, deadlock, watchdog timers, critical sections
- UART, SPI, I2C, CAN, USB, Ethernet, parallel buses, duplex, sync vs async vs semi-synchronous
- Clock ppm, drift, latency, jitter, transaction-time math, throughput
- Design-for-test, DMA, PIO, interrupts, ADC/DAC, motion-control basics

## Side 1 - Core Theory and RTOS

### Definitions
- Embedded system: a computer system designed to perform a dedicated function inside a larger system.
- Real-time: correctness depends on both the logical result and when the result is produced.
- Hard real-time: missing a deadline is unacceptable and may mean failure.
- Soft real-time: deadline misses degrade quality, but the system may still operate.
- Deterministic behavior: response and timing are predictable and bounded.
- Latency: time from event to response.
- Jitter: variation in latency or timing.
- Throughput: amount of data/work completed per unit time.
- Excess computational capacity: timing margin for worst-case execution, burst load, ISR overhead, retries, and future changes.
- Walking skeleton: minimal end-to-end version of the system that proves the architecture early.
- Iterative and incremental development: build small working slices, then extend/refine.

### Architectures and Platforms
- Harvard: separate instruction and data paths/memories.
- von Neumann: instructions and data share memory/path.
- Hybrid: some Harvard-like separation with unified behavior elsewhere.
- Microcontroller: CPU + memory + peripherals on one chip.
- Microprocessor: CPU-centric; often needs external memory/peripherals.
- Common MCU-based SoC blocks: CPU core(s), RAM/Flash, GPIO, timers, PWM, UART, SPI, I2C, ADC/DAC, DMA, interrupt logic.
- Memory-mapped I/O: peripheral registers live at addresses; CPU uses normal loads/stores to access them.
- AHB matrix: high-speed interconnect that lets bus masters reach slaves efficiently; benefit is higher concurrency/throughput.
- Zynq PS vs PL:
  PS = control, OS, protocols, software flexibility.
  PL = deterministic, highly parallel, custom hardware pipelines.
- PetaLinux: embedded Linux environment/tool flow for Xilinx devices.

### Scheduling and Tasking
- FreeRTOS single-core default: fixed-priority preemptive scheduling with round-robin time slicing among equal-priority ready tasks.
- Cooperative multitasking: tasks switch only when they yield/block.
  Pros: simpler, lower overhead, can be easier to reason about.
  Cons: a badly written task can starve the rest of the system.
- Foreground-background:
  Foreground = ISR-driven urgent work.
  Background = main loop or lower-urgency processing.
- Single non-preemptive loop: worst-case latency is set by the loop/frame timing or longest time before the relevant code runs.
- Software-generated timing can be problematic because scheduling and interrupts add latency/jitter.

### TCB and Context Switching
- Context switch: save current task context and restore another task's context so multitasking works.
- Why needed: lets tasks block, resume, and share one CPU safely.
- Task states: Running, Ready, Blocked, Suspended.
- TCB may contain: stack pointer/context, task priority, task state, delay/timeout info, stack bounds, task name/ID, scheduler list links.

### Synchronization
- Mutex:
  Use for mutual exclusion on a shared resource.
  Has an owner.
  Supports priority inheritance.
  Not the normal choice for ISR-to-task signaling.
- Binary semaphore:
  Use for event signaling or task synchronization.
  No owner.
  Good for ISR -> task signaling.
- Counting semaphore:
  Use to count identical resources or repeated events.
- Queue:
  Use when you must pass data and synchronize at the same time.
- Event group:
  Use for multiple conditions, rendezvous, and wait-on-bits logic.
- Critical section:
  `taskENTER_CRITICAL(); ... taskEXIT_CRITICAL();`
  Use only for very short code.
  Do not block inside a critical section.

### Classical Problems
- Priority inversion:
  Low-priority task holds a mutex needed by a high-priority task; a medium-priority task can delay both.
  Fix: priority inheritance.
- Deadlock conditions (Coffman):
  Mutual exclusion, hold-and-wait, no preemption, circular wait.
  Break any one condition to prevent deadlock.
- Watchdog timer:
  If software fails to refresh the timer in time, hardware/software forces recovery or reset.

## Side 2 - FreeRTOS and Peripheral Setup Patterns

### Generic Bring-Up Pattern
```c
int main(void) {
    board_clock_init();
    board_pinmux_init();

    peripheral_reset_disable();
    peripheral_init(&cfg);          // baud, mode, addr, sample rate, etc.

    xBusMutex = xSemaphoreCreateMutex();     // if shared resource
    xDoneSem  = xSemaphoreCreateBinary();    // if IRQ/DMA completion
    xRxQueue  = xQueueCreate(N, sizeof(item));

    xTaskCreate(acquire_task, "ACQ", stack, NULL, prio, NULL);
    xTaskCreate(process_task, "PROC", stack, NULL, prio, NULL);

    enable_irq_or_dma_if_used();
    vTaskStartScheduler();
}
```

### Shared Peripheral Task Pattern
```c
void peripheral_task(void *arg) {
    for (;;) {
        wait_for_period_or_trigger();

        xSemaphoreTake(xBusMutex, timeout);

        start_transaction();

        if (uses_irq_or_dma) {
            xSemaphoreTake(xDoneSem, timeout);
        } else {
            poll_status_with_timeout();
        }

        read_data_and_check_errors();
        xSemaphoreGive(xBusMutex);

        xQueueSend(xRxQueue, &item, timeout);
    }
}
```

### ISR Pattern
```c
void ISR(void) {
    BaseType_t hpw = pdFALSE;

    clear_irq_flag();
    move_minimal_data_to_or_from_fifo();

    xSemaphoreGiveFromISR(xDoneSem, &hpw);
    // or xQueueSendFromISR(...), xTaskNotifyFromISR(...),
    // or xEventGroupSetBitsFromISR(...)

    portYIELD_FROM_ISR(hpw);
}
```

### DMA Pattern
```c
configure_peripheral_first();

claim_dma_channel();
set_READ_ADDR(src);
set_WRITE_ADDR(dst);
set_TRANS_COUNT(count);
set_CTRL(data_size, incr_read, incr_write, dreq_or_timer, chain_to, enable);

enable_dma_completion_irq();
start_dma();

// task waits on semaphore or task notification
```

### Peripheral-Setup Answer Template
1. Identify the interface and required throughput/latency.
2. Choose pins and configure pin mux/pulls/electrical mode.
3. Enable clock and take peripheral out of reset.
4. Configure protocol settings.
   UART: baud, data bits, parity, stop bits.
   SPI: mode, bit rate, frame size, chip-select behavior.
   I2C: bus speed, address, pull-ups, read/write sequence.
   ADC: channel, reference, sample rate, trigger.
5. Decide polling vs interrupt vs DMA.
6. Create needed RTOS objects.
   Mutex if shared bus/resource.
   Semaphore/notification if completion event.
   Queue if data moves between tasks.
7. Create tasks and set priorities.
8. Enable peripheral and verify with timeout/error handling.

### What To Say If Asked "Why FreeRTOS Here?"
- Lets time-critical work block instead of busy-waiting.
- Separates acquisition, processing, and output into clearer tasks.
- Gives synchronization tools for shared buses/peripherals.
- Makes interrupt-to-task handoff cleaner.
- Still needs careful priority, timeout, and ISR design.

## Side 3 - Interfaces, Signals, and Timing Math

### Signal and Interface Basics
- Simplex: one-way only.
- Half-duplex: both directions, but not at the same time.
- Full-duplex: both directions at the same time.
- Synchronous: shared clock defines timing.
- Asynchronous: no shared clock; framing/timing recovery used.
- Semi-synchronous: more timing structure than asynchronous, but less strict than a fully shared continuous clock.

### UART
- Asynchronous, usually full-duplex, point-to-point.
- Typical signals: TX, RX, GND.
- Frame uses start bit, data bits, optional parity, stop bits.
- Good for debug, console, GPS/Bluetooth/modem-style modules.
- Clock mismatch must stay within tolerance or framing errors happen.
- Common hardware errors: parity, framing, overrun.
- Polling or interrupts can be used.
- RS-422 / RS-485 use differential signaling for better noise immunity and longer links; RS-485 also supports multi-drop.

### SPI
- Synchronous, full-duplex.
- Typical signals: SCLK, MOSI/SDI, MISO/SDO, CS/nCS.
- High speed, simple protocol, common for sensors, ADCs, DACs, displays.
- Each device normally needs its own chip select.
- If chip-select pins are scarce: use a decoder, shift register, GPIO expander, or daisy-chain if devices support it.
- Daisy-chain is possible if supported by the devices.
- Reads often require transmitting dummy bytes while clocking data back.
- Must choose CPOL/CPHA mode correctly.

### I2C
- Synchronous, half-duplex shared bus.
- Two wires: SDA and SCL.
- Open-drain/open-collector behavior with pull-ups.
- Uses START, address + R/W, ACK/NACK, data bytes, STOP.
- Good for many slower peripherals when pin count is limited.
- Supports 7-bit and 10-bit addressing.
- Multiple devices share the same bus; addresses must be unique.

### CAN, USB, Ethernet, Parallel
- CAN:
  Multi-master, differential, strong for deterministic arbitration.
  Lower numeric ID means higher priority.
  Good for robust distributed embedded systems.
- USB:
  Host-controlled.
  Transfer types: control, bulk, interrupt, isochronous.
  Isochronous guarantees bandwidth but not delivery.
- Ethernet:
  High throughput LAN connectivity, but higher software/protocol complexity.
- Parallel bus:
  High throughput and simple local interfaces, but many wires, skew, and poorer scalability.
  Handshaking helps coordinate sender/receiver timing.

### Interface Selection Heuristics
- Need few wires and many low/medium-speed devices -> I2C.
- Need high speed and simple controller/peripheral timing -> SPI.
- Need async point-to-point stream or serial console -> UART.
- Need long/noisy shared wiring and arbitration -> CAN or differential serial.
- Need very high data rate from a converter -> parallel or dedicated high-speed interface.
- Need to move lots of data with little CPU overhead -> DMA.
- Need custom waveforms/protocol timing -> PIO or programmable logic.

### Signal Integrity and Isolation
- All real signals are analog in the physical world.
- Noise, DC offsets, skew, attenuation, reflections, and jitter can corrupt digital communication.
- Ground loops can add unwanted current/noise and shift references.
- Ways to break ground-loop problems or improve isolation:
  differential signaling, opto-isolation, transformer isolation, careful grounding.

### Timing and Formula Reminders
- `fractional_error = ppm / 1e6`
- `drift = elapsed_time * fractional_error`
- `time_to_drift = desired_drift / fractional_error`
- For two free-running clocks, worst-case relative drift often uses the sum of their absolute ppm errors.
- `transaction_time = fixed_overhead + bits / bit_rate`
- `max_sample_rate = 1 / transaction_time`
- If RTOS or ISR jitter adds `t_jitter`, use:
  `effective_period >= transaction_time + worst_case_jitter`
- Nyquist:
  `fs >= 2 * fmax`
  If not, aliasing occurs.
- ADC resolution:
  `levels = 2^N`
  Approximate `LSB ~= Vref / 2^N` (or `Vref / (2^N - 1)` depending convention)

### Design/Test Checklist
- Compute data rate, latency, and worst-case timing first.
- State assumptions clearly in any calculation answer.
- Add test points for power, ground, SCLK, MOSI, MISO, TX, RX, etc.
- Include programming/debug header (SWD/JTAG/UART as appropriate).
- Add simple status indicators (heartbeat LED, debug prints, loopback, self-test).
- Leave physical room for probes and debugging.

## Side 4 - Memory, DMA, Interrupts, ADC/DAC, Motion

### HAL and Memory-Mapped I/O
- HAL layers:
  Application -> HAL -> MCU port -> BSP/board specifics.
- HAL goal:
  hide register details, improve portability, keep application code cleaner.
- Memory-mapped I/O advantages:
  same load/store model as memory, easy C access, no separate I/O instruction style, low software overhead.
- Common register-level pattern:
  disable peripheral -> configure registers -> clear status -> enable -> transfer -> check flags/errors

### DMA and PIO
- DMA basics:
  CPU sets source, destination, count, and control; DMA moves data in parallel with CPU work.
- DMA is best when:
  data rates are high, transfers are repetitive, or CPU should not waste cycles moving bytes.
- RP2040 DMA concepts stressed in lecture:
  READ_ADDR, WRITE_ADDR, TRANS_COUNT, CTRL
  DATA_SIZE, INCR_READ, INCR_WRITE, TREQ/DREQ pacing, CHAIN_TO, HIGH_PRIORITY
- DREQ/TREQ:
  pace DMA so a peripheral and DMA stay in sync.
- PIO:
  offloads custom timing/protocol generation from the CPU.
- DMA + PIO together:
  powerful for streaming, VGA/video-like timing, PWM-like generation, or custom interfaces.

### Interrupts and Events
- Use interrupts for external asynchronous events, tight timing, or high-priority unpredictable arrivals.
- Keep ISRs short, deterministic, and non-blocking.
- Do the minimum in ISR:
  clear flag, move minimal data, wake a task.
- Use only FromISR APIs inside an ISR.
- Do not use interrupts for slow periodic work that can be handled by timers/tasks.
- Event groups are useful when tasks wait for one or more bit conditions or a rendezvous point.

### ADC and DAC
- ADC:
  converts analog to digital.
  Watch sample rate, resolution, reference, channel selection, and acquisition timing.
- DAC:
  converts digital to analog.
  Real analog reconstruction may need smoothing/low-pass filtering.
- If sampling several channels:
  think about round-robin order, de-interleaving, buffering, and DMA.
- For high-rate capture:
  ADC + DMA is often the right pairing.

### Motion Control
- Stepper motor:
  open-loop positioning by commanded steps.
  Full-step and half-step sequences trade simplicity vs smoothness/resolution.
- H-bridge:
  allows bidirectional current through a load/motor winding.
- Encoder:
  provides feedback for closed-loop control.
- Servo:
  closed-loop position control, often commanded by PWM.
- DC motor:
  often needs PWM for speed and an H-bridge for direction.
- L298N vs A4988:
  L298N = drive coil sequence more directly, more control pins/logic.
  A4988 = STEP/DIR style interface, simpler MCU control, supports microstepping.

## Fast Lecture Lookup
- Real-time definitions, determinism, excess capacity -> L2, L4, L6, L7
- FreeRTOS scheduling, TCB, context switch, critical sections -> L5, L8, L10
- UART, async/sync/semi-sync, ppm/drift -> L12, L13
- SPI -> L15, L17
- I2C -> L18
- Interfaces, duplex, signal/noise/ground loops -> L11 to L14, L19
- Design-for-test and interface planning -> L20
- Memory-mapped I/O -> L21, L22
- DMA and PIO -> L23 to L27
- Interrupts and event groups -> L28
- ADC/DAC -> L27, L29
- Motion control -> L30 to L32

## Full Lecture-to-Concept Map
- L1: course intro and learning objectives
- L2: embedded systems overview, hard vs soft real-time, examples
- L3: Harvard/von Neumann/hybrid, MCU vs MPU, memory, registers, MMIO
- L4: OS vs RTOS, PetaLinux, basic scheduling, real-time framing
- L5: FreeRTOS scheduling, tasks, examples, preemption
- L6: embedded software process, iterative development, walking skeleton, timing constraints
- L7: bare metal, foreground-background, non-preemptive loops, ISRs, excess computational capacity
- L8: context switching, TCBs, cooperative vs preemptive multitasking, critical sections, mutex vs semaphore
- L9: quiz and review
- L10: synchronization, mutex setup, queues, deadlock, priority inversion, watchdog timers
- L11: interfacing basics, simplex/half/full duplex, analog vs digital reality, noise, DC offsets, ground loops, isolation
- L12: UART, RS-232/422/485, sync vs async vs semi-synchronous, clock accuracy and ppm
- L13: HAL structure, UART/HAL examples, pin mux and board support concepts
- L14: Ethernet, CAN, USB
- L15: SPI theory and architecture
- L16: quiz and review
- L17: SPI + FreeRTOS + RP2040 example, design example
- L18: I2C protocol, transactions, addressing, ACK/NACK
- L19: parallel interfaces, buses, handshaking, serial vs parallel tradeoffs
- L20: planning peripherals/interfaces, design-for-test, RTM habit
- L21: RTM continuation, memory mapping basics
- L22: memory-mapped I/O to peripherals, RP2040 memory map, direct-register examples
- L23: DMA fundamentals and RP2040 DMA registers/features
- L24: quiz and review
- L25: DMA example, PIO purpose and programming concepts
- L26: DMA + PIO examples, VGA/PWM-style generation
- L27: DMA for real-world data capture, ADC/DAC system examples
- L28: interrupts, events, FromISR APIs, event groups
- L29: ADCs, DACs, Nyquist, aliasing, sampling tradeoffs
- L30: stepper motors, H-bridges, motion-control basics
- L31: encoders, servos, DC motors, other actuators, open-loop vs closed-loop
- L32: RP2040 stepper examples, L298N, A4988, driver comparison

## Very Likely "Explain" Prompts
- Explain real-time:
  A system is real-time when timing deadlines are part of correctness, not just eventual correctness.
- Explain deterministic behavior:
  The system responds in a predictable, bounded, repeatable way with controlled latency/jitter.
- Explain why excess computational capacity matters:
  It gives margin for worst-case execution, interrupts, burst loads, retries, and deadline protection.
- Explain context switching:
  The RTOS saves one task's CPU state and restores another task's state so the CPU can time-share tasks.
- Explain why a mutex is different from a semaphore:
  Mutex protects an owned shared resource and can inherit priority; semaphore is mainly signaling/counting and has no owner.
- Explain why ISR code must be short:
  ISRs preempt normal execution, so long ISRs increase latency/jitter everywhere else.
