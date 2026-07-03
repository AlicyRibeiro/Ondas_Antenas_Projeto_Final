/**
 * @file    rc522.c
 * @brief   Implementação do driver Bare-Metal para MFRC522 via SPI1 no STM32F103C8T6
 */

#include "rc522.h"

extern void delay_ms(uint32_t ms);

static uint8_t SPI1_TransmitReceive(uint8_t data)
{
    uint32_t timeout = 10000U;
    
    while (!(SPI1->SR & SPI_SR_TXE))
    {
        if (--timeout == 0U) { return 0xFFU; }
    }

    *(__IO uint8_t *)&SPI1->DR = data;

    timeout = 10000U;
    while (!(SPI1->SR & SPI_SR_RXNE))
    {
        if (--timeout == 0U) { return 0xFFU; }
    }

    return (uint8_t)(SPI1->DR);
}

void MFRC522_WriteReg(uint8_t reg, uint8_t value)
{
    uint8_t addr_byte = (uint8_t)((reg << 1U) & 0x7EU);

    RC522_CS_LOW();
    SPI1_TransmitReceive(addr_byte);
    SPI1_TransmitReceive(value);
    RC522_CS_HIGH();
}

uint8_t MFRC522_ReadReg(uint8_t reg)
{
    uint8_t addr_byte = (uint8_t)(((reg << 1U) & 0x7EU) | 0x80U);
    uint8_t value;

    RC522_CS_LOW();
    SPI1_TransmitReceive(addr_byte);
    value = SPI1_TransmitReceive(0x00U);
    RC522_CS_HIGH();

    return value;
}

void MFRC522_SetRegBits(uint8_t reg, uint8_t mask)
{
    MFRC522_WriteReg(reg, MFRC522_ReadReg(reg) | mask);
}

void MFRC522_ClearRegBits(uint8_t reg, uint8_t mask)
{
    MFRC522_WriteReg(reg, MFRC522_ReadReg(reg) & (~mask));
}

static uint8_t MFRC522_ToCard(uint8_t cmd,
                               uint8_t *send_data, uint8_t send_len,
                               uint8_t *back_data, uint16_t *back_len)
{
    uint8_t  status    = MFRC522_ERR;
    uint8_t  irq_en    = 0x00U;
    uint8_t  wait_irq  = 0x00U;
    uint8_t  last_bits;
    uint8_t  n;
    uint16_t i;

    if (cmd == MFRC522_CMD_MF_AUTHENT)
    {
        irq_en   = 0x12U;
        wait_irq = 0x10U;
    }
    if (cmd == MFRC522_CMD_TRANSCEIVE)
    {
        irq_en   = 0x77U;
        wait_irq = 0x30U;
    }

    MFRC522_WriteReg(MFRC522_REG_COM_I_EN, irq_en | 0x80U);
    MFRC522_ClearRegBits(MFRC522_REG_COM_IRQ, 0x80U);
    MFRC522_SetRegBits(MFRC522_REG_FIFO_LEVEL, 0x80U);
    MFRC522_WriteReg(MFRC522_REG_COMMAND, MFRC522_CMD_IDLE);

    for (i = 0U; i < send_len; i++)
    {
        MFRC522_WriteReg(MFRC522_REG_FIFO_DATA, send_data[i]);
    }

    MFRC522_WriteReg(MFRC522_REG_COMMAND, cmd);

    if (cmd == MFRC522_CMD_TRANSCEIVE)
    {
        MFRC522_SetRegBits(MFRC522_REG_BIT_FRAMING, 0x80U);
    }

    i = 2000U;
    do
    {
        n = MFRC522_ReadReg(MFRC522_REG_COM_IRQ);
        i--;
    } while ((i != 0U) && !(n & 0x01U) && !(n & wait_irq));

    MFRC522_ClearRegBits(MFRC522_REG_BIT_FRAMING, 0x80U);

    if (i == 0U)
    {
        return MFRC522_TIMEOUT;
    }

    if (MFRC522_ReadReg(MFRC522_REG_ERROR) & 0x1BU)
    {
        return MFRC522_ERR;
    }

    status = MFRC522_OK;

    if (n & irq_en & 0x01U)
    {
        status = MFRC522_NO_TAG;
    }

    if (cmd == MFRC522_CMD_TRANSCEIVE)
    {
        n         = MFRC522_ReadReg(MFRC522_REG_FIFO_LEVEL);
        last_bits = MFRC522_ReadReg(MFRC522_REG_CONTROL) & 0x07U;

        if (last_bits)
        {
            *back_len = (uint16_t)((n - 1U) * 8U + last_bits);
        }
        else
        {
            *back_len = (uint16_t)(n * 8U);
        }

        if (n == 0U) { n = 1U; }
        if (n > 16U) { n = 16U; }

        for (i = 0U; i < n; i++)
        {
            back_data[i] = MFRC522_ReadReg(MFRC522_REG_FIFO_DATA);
        }
    }

    return status;
}

void MFRC522_Init(void)
{
    GPIOA->CRL &= ~(0xFFFF0000U);
    GPIOA->CRL |= (0x2U << 16U)  
               |  (0xBU << 20U)  
               |  (0x4U << 24U)  
               |  (0xBU << 28U); 

    RC522_CS_HIGH();

    GPIOB->CRL &= ~(0x0000000FU);
    GPIOB->CRL |=  (0x2U << 0U);

    RCC->APB2RSTR |=  RCC_APB2RSTR_SPI1RST;
    RCC->APB2RSTR &= ~RCC_APB2RSTR_SPI1RST;

    SPI1->CR1 = 0U;
    SPI1->CR1 = SPI_CR1_SSM      
              | SPI_CR1_SSI      
              | SPI_CR1_MSTR     
              | (0x2U << SPI_CR1_BR_Pos); 

    SPI1->CR2 = 0U;
    SPI1->CR1 |= SPI_CR1_SPE;

    RC522_RST_LOW();
    delay_ms(10U);    
    RC522_RST_HIGH();
    delay_ms(50U);    

    MFRC522_WriteReg(MFRC522_REG_COMMAND, MFRC522_CMD_SOFT_RESET);
    delay_ms(50U);

    MFRC522_WriteReg(MFRC522_REG_T_MODE,       0x8DU); 
    MFRC522_WriteReg(MFRC522_REG_T_PRESCALER,  0x3EU); 
    MFRC522_WriteReg(MFRC522_REG_T_RELOAD_H,   0x00U); 
    MFRC522_WriteReg(MFRC522_REG_T_RELOAD_L,   0x1EU); 

    MFRC522_WriteReg(MFRC522_REG_TX_ASK,  0x40U);
    MFRC522_WriteReg(MFRC522_REG_MODE,    0x3DU);

    MFRC522_SetRegBits(MFRC522_REG_TX_CONTROL, 0x03U);
}

uint8_t MFRC522_Request(uint8_t req_mode, uint8_t *tag_type)
{
    uint8_t  status;
    uint16_t back_bits;

    MFRC522_WriteReg(MFRC522_REG_BIT_FRAMING, 0x07U);

    tag_type[0] = req_mode;

    status = MFRC522_ToCard(MFRC522_CMD_TRANSCEIVE,
                            tag_type, 1U,
                            tag_type, &back_bits);

    if ((status != MFRC522_OK) || (back_bits != 0x10U))
    {
        status = MFRC522_ERR;
    }

    return status;
}

uint8_t MFRC522_Anticoll(uint8_t *ser_num)
{
    uint8_t  status;
    uint8_t  i;
    uint8_t  ser_num_check = 0U;
    uint16_t unLen;

    MFRC522_WriteReg(MFRC522_REG_BIT_FRAMING, 0x00U);

    ser_num[0] = PICC_CMD_SEL_CL1; 
    ser_num[1] = 0x20U;             

    status = MFRC522_ToCard(MFRC522_CMD_TRANSCEIVE,
                            ser_num, 2U,
                            ser_num, &unLen);

    if (status == MFRC522_OK)
    {
        ser_num_check = 0U;
        for (i = 0U; i < 4U; i++)
        {
            ser_num_check ^= ser_num[i];
        }

        if (ser_num_check != ser_num[4])
        {
            status = MFRC522_ERR;
        }
    }

    return status;
}

uint8_t MFRC522_CompareUID(const uint8_t *uid, const uint8_t *master_uid)
{
    uint8_t i;

    for (i = 0U; i < 4U; i++)
    {
        if (uid[i] != master_uid[i])
        {
            return MFRC522_WRONG_UID;
        }
    }

    return MFRC522_OK;
}

uint8_t MFRC522_Halt(void)
{
    uint8_t  buf[4U];
    uint16_t unLen;

    buf[0] = PICC_CMD_HLTA;
    buf[1] = 0U;

    MFRC522_WriteReg(MFRC522_REG_COMMAND, MFRC522_CMD_IDLE);
    MFRC522_SetRegBits(MFRC522_REG_FIFO_LEVEL, 0x80U); 
    MFRC522_WriteReg(MFRC522_REG_FIFO_DATA, buf[0]);
    MFRC522_WriteReg(MFRC522_REG_FIFO_DATA, buf[1]);
    MFRC522_WriteReg(MFRC522_REG_COMMAND, MFRC522_CMD_CALC_CRC);

    uint32_t timeout = 5000U;
    uint8_t  n;
    do
    {
        n = MFRC522_ReadReg(MFRC522_REG_DIV_IRQ);
        timeout--;
    } while ((timeout != 0U) && !(n & 0x04U)); 

    buf[2] = MFRC522_ReadReg(MFRC522_REG_CRC_RESULT_L);
    buf[3] = MFRC522_ReadReg(MFRC522_REG_CRC_RESULT_M);

    MFRC522_ToCard(MFRC522_CMD_TRANSCEIVE, buf, 4U, buf, &unLen);

    return MFRC522_OK;
}
