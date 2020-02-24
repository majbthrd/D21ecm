#include <stdint.h>
#include <sam.h>
#include "time.h"

static volatile uint32_t ticks;

void time_init(void)
{
  SysTick_Config(SystemCoreClock / 1000);
}

void SysTick_Handler(void)
{
  ticks++;
}

uint32_t sys_now()
{
  return ticks;
}
