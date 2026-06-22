#include "tach.h"

static volatile uint32_t pulse_count = 0;

void Tach_IncrementPulse(void)
{
    pulse_count++;
}

uint32_t Tach_ComputeRPM(void)
{
    uint32_t count = pulse_count;
    pulse_count = 0; /* reset for the next 1-second window */

    return count * 60; /* 1 pulse per revolution, counted over 1 second */
}
