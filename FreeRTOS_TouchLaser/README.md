# FreeRTOS_TouchLaser

Real-world FreeRTOS pipeline using a touch sensor, laser module, and UART output.
Demonstrates multi-consumer architecture with two independent queues, change detection
to avoid queue flooding, and clean task separation between input, output, and reporting.

## What it does

- touchTask polls TTP223B touch sensor on PA1 every 50ms, detects state changes
- laserTask receives touch state from laserQueue, turns KY-008 laser on PA4 on/off
- uartTask receives touch state from uartQueue, prints status to minicom over USART2
- Laser turns on when touched, off when released - status visible on terminal

## Hardware

- TTP223B touch sensor: VCC → 3.3V, GND → GND, SIG → PA1
- KY-008 laser module: + → 3.3V, - → GND, S → PA4
- Waveshare FT232RNL: PA2 (USART2_TX) → RXD, PA3 (USART2_RX) → TXD, GND → GND

## Why two queues

A FreeRTOS queue is one-to-one. Once a value is removed by one consumer it is gone -
the second consumer would never see it. With one queue, either laserTask or uartTask
would get each message, never both.

Two queues solves this cleanly - touchTask puts the same state value into both
laserQueue and uartQueue simultaneously. Each consumer owns its queue and never
competes with the other.

> [!NOTE]
> This is a deliberate architectural decision, not a workaround. In production
> firmware, fan-out to multiple consumers is handled either with multiple queues
> or with a publish-subscribe event system. Multiple queues is the simpler and
> more explicit approach.

## Change detection in touchTask

```c
uint32_t lastState = 0;
for(;;)
{
    uint32_t state = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1);
    if(state != lastState)
    {
        osMessageQueuePut(laserQueueHandle, &state, 0, 0);
        osMessageQueuePut(uartQueueHandle, &state, 0, 0);
        lastState = state;
    }
    osDelay(50);
}
```

Without change detection, touchTask floods both queues with the current state every
50ms regardless of whether anything changed. Terminal output becomes unreadable and
queues fill unnecessarily. Only sending on state change keeps output clean and queues
nearly empty at all times.

> [!TIP]
> This pattern - poll, compare to last state, only act on change - is fundamental
> in embedded input handling. Works the same for buttons, encoders, and any digital
> input without hardware interrupts.

## Gotchas

> [!WARNING]
> HAL_GPIO_WritePin expects GPIO_PinState not uint32_t. Passing raw state from
> the queue works because 0 and 1 map correctly to GPIO_PIN_RESET and GPIO_PIN_SET,
> but cast explicitly for correctness: (GPIO_PinState)state

> [!IMPORTANT]
> touchTask sends to both queues before the delay. Both puts use timeout 0 - if
> either queue is full the put is dropped silently. Queue size 5 with 50ms polling
> gives plenty of headroom, but in production check the return value of
> osMessageQueuePut and handle failures.

## Key config decisions

- Two queues: laserQueue and uartQueue, both size 5, uint32_t. Separate queues per
  consumer - no sharing, no competition.
- Poll interval 50ms on touchTask. Fast enough for responsive feel, slow enough to
  not flood queues. Natural debounce for the capacitive touch sensor.
- All three tasks osPriorityNormal. No priority needed - touchTask naturally paces
  the pipeline via its 50ms delay.

## Lessons Learned

- FreeRTOS queues are one-to-one. Fan-out to multiple consumers requires multiple
  queues. This is by design - each consumer owns its data independently.
- Change detection is not optional for polled inputs in RTOS. Without it, queues
  flood and consumers process identical repeated values doing redundant work.
- Separating concerns into dedicated tasks makes the architecture readable. touchTask
  only reads, laserTask only actuates, uartTask only reports. Adding a fourth consumer
  means adding one queue and one task - nothing else changes.
- Real hardware debugging is part of firmware development. Isolate with known-good
  pins and hardcoded values before blaming code.

## Logic analyzer

Probe PA2 (USART2_TX) at 115200 baud to verify UART frames on touch events.
Probe PA4 to verify laser signal transitions on touch state changes.
Both signals should transition within one 50ms poll cycle of a touch event.
