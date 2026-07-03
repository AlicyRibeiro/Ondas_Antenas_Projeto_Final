#ifndef RGB_H
#define RGB_H

#include <stm32f1xx.h>
#include <stdint.h>

#define RGB_R_PIN    0U
#define RGB_G_PIN    1U
#define RGB_B_PIN    2U

#define RGB_ALL_MASK ((1U << RGB_R_PIN) | (1U << RGB_G_PIN) | (1U << RGB_B_PIN))

#define RGB_R_ON()   (GPIOA->BRR  = (1U << RGB_R_PIN))
#define RGB_R_OFF()  (GPIOA->BSRR = (1U << RGB_R_PIN))

#define RGB_G_ON()   (GPIOA->BRR  = (1U << RGB_G_PIN))
#define RGB_G_OFF()  (GPIOA->BSRR = (1U << RGB_G_PIN))

#define RGB_B_ON()   (GPIOA->BRR  = (1U << RGB_B_PIN))
#define RGB_B_OFF()  (GPIOA->BSRR = (1U << RGB_B_PIN))

#define RGB_ALL_OFF() (GPIOA->BSRR = RGB_ALL_MASK)

#define RGB_SET_RED()     do { RGB_ALL_OFF(); RGB_R_ON(); } while(0)
#define RGB_SET_GREEN()   do { RGB_ALL_OFF(); RGB_G_ON(); } while(0)
#define RGB_SET_BLUE()    do { RGB_ALL_OFF(); RGB_B_ON(); } while(0)
#define RGB_SET_YELLOW()  do { RGB_ALL_OFF(); RGB_R_ON(); RGB_G_ON(); } while(0)
#define RGB_SET_CYAN()    do { RGB_ALL_OFF(); RGB_G_ON(); RGB_B_ON(); } while(0)
#define RGB_SET_MAGENTA() do { RGB_ALL_OFF(); RGB_R_ON(); RGB_B_ON(); } while(0)
#define RGB_SET_WHITE()   do { RGB_ALL_OFF(); RGB_R_ON(); RGB_G_ON(); RGB_B_ON(); } while(0)

void RGB_Init(void);
void RGB_SetColor(uint8_t r, uint8_t g, uint8_t b);
void RGB_BlinkBlue(uint32_t delay_ms);

#endif /* RGB_H */
