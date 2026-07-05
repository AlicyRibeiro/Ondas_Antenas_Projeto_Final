/**
 * @file    rgb.c
 * @brief   Implementação do controle Bare-Metal do LED RGB Ânodo Comum
 */

#include "rgb.h"

extern void delay_ms(uint32_t ms);

static uint32_t rgb_blink_last_tick = 0U;
static uint8_t  rgb_blink_state     = 0U;

void RGB_Init(void)
{
    GPIOA->CRL &= ~(0x00000FFFU);

    GPIOA->CRL |= (0x2U << 0U)   
               |  (0x2U << 4U)   
               |  (0x2U << 8U);  

    RGB_ALL_OFF();
}

void RGB_SetColor(uint8_t r, uint8_t g, uint8_t b)
{
    RGB_ALL_OFF();

    if (r) { RGB_R_ON(); }
    if (g) { RGB_G_ON(); }
    if (b) { RGB_B_ON(); }
}

void RGB_BlinkBlue(uint32_t delay_ms_val)
{
    extern volatile uint32_t SysTick_tick;
    uint32_t now = SysTick_tick;

    if ((now - rgb_blink_last_tick) >= delay_ms_val)
    {
        rgb_blink_last_tick = now;
        rgb_blink_state ^= 1U;

        if (rgb_blink_state)
        {
            RGB_ALL_OFF();
            RGB_B_ON();
        }
        else
        {
            RGB_ALL_OFF();
        }
    }
}
