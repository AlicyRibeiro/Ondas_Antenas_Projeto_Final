#ifndef RC522_H
#define RC522_H

#include <stm32f1xx.h>
#include <stdint.h>

#define RC522_CS_PIN    4U
#define RC522_RST_PIN   0U

#define RC522_CS_LOW()  (GPIOA->BRR  = (1U << RC522_CS_PIN))
#define RC522_CS_HIGH() (GPIOA->BSRR = (1U << RC522_CS_PIN))

#define RC522_RST_LOW()  (GPIOB->BRR  = (1U << RC522_RST_PIN))
#define RC522_RST_HIGH() (GPIOB->BSRR = (1U << RC522_RST_PIN))

#define MFRC522_REG_COMMAND         0x01U
#define MFRC522_REG_COM_I_EN        0x02U
#define MFRC522_REG_DIV_I_EN        0x03U
#define MFRC522_REG_COM_IRQ         0x04U
#define MFRC522_REG_DIV_IRQ         0x05U
#define MFRC522_REG_ERROR           0x06U
#define MFRC522_REG_STATUS1         0x07U
#define MFRC522_REG_STATUS2         0x08U
#define MFRC522_REG_FIFO_DATA       0x09U
#define MFRC522_REG_FIFO_LEVEL      0x0AU
#define MFRC522_REG_WATER_LEVEL     0x0BU
#define MFRC522_REG_CONTROL         0x0CU
#define MFRC522_REG_BIT_FRAMING     0x0DU
#define MFRC522_REG_COLL            0x0EU

#define MFRC522_REG_MODE            0x11U
#define MFRC522_REG_TX_MODE         0x12U
#define MFRC522_REG_RX_MODE         0x13U
#define MFRC522_REG_TX_CONTROL      0x14U
#define MFRC522_REG_TX_ASK          0x15U
#define MFRC522_REG_TX_SEL          0x16U
#define MFRC522_REG_RX_SEL          0x17U
#define MFRC522_REG_RX_THRESHOLD    0x18U
#define MFRC522_REG_DEMOD           0x19U
#define MFRC522_REG_MF_TX           0x1CU
#define MFRC522_REG_MF_RX           0x1DU
#define MFRC522_REG_SERIAL_SPEED    0x1FU

#define MFRC522_REG_CRC_RESULT_M    0x21U
#define MFRC522_REG_CRC_RESULT_L    0x22U
#define MFRC522_REG_MOD_WIDTH       0x24U
#define MFRC522_REG_RF_CFG          0x26U
#define MFRC522_REG_GS_N            0x27U
#define MFRC522_REG_CW_GS_P         0x28U
#define MFRC522_REG_MOD_GS_P        0x29U
#define MFRC522_REG_T_MODE          0x2AU
#define MFRC522_REG_T_PRESCALER     0x2BU
#define MFRC522_REG_T_RELOAD_H      0x2CU
#define MFRC522_REG_T_RELOAD_L      0x2DU
#define MFRC522_REG_T_COUNTER_VAL_H 0x2EU
#define MFRC522_REG_T_COUNTER_VAL_L 0x2FU

#define MFRC522_REG_TEST_SEL1       0x31U
#define MFRC522_REG_TEST_SEL2       0x32U
#define MFRC522_REG_TEST_PIN_EN     0x33U
#define MFRC522_REG_TEST_PIN_VALUE  0x34U
#define MFRC522_REG_TEST_BUS        0x35U
#define MFRC522_REG_AUTO_TEST       0x36U
#define MFRC522_REG_VERSION         0x37U
#define MFRC522_REG_ANALOG_TEST     0x38U
#define MFRC522_REG_TEST_ADC1       0x39U
#define MFRC522_REG_TEST_ADC2       0x3AU
#define MFRC522_REG_TEST_ADC0       0x3BU

#define MFRC522_CMD_IDLE            0x00U
#define MFRC522_CMD_MEM             0x01U
#define MFRC522_CMD_GEN_RANDOM_ID   0x02U
#define MFRC522_CMD_CALC_CRC        0x03U
#define MFRC522_CMD_TRANSMIT        0x04U
#define MFRC522_CMD_NO_CMD_CHANGE   0x07U
#define MFRC522_CMD_RECEIVE         0x08U
#define MFRC522_CMD_TRANSCEIVE      0x0CU
#define MFRC522_CMD_MF_AUTHENT      0x0EU
#define MFRC522_CMD_SOFT_RESET      0x0FU

#define PICC_CMD_REQA               0x26U
#define PICC_CMD_WUPA               0x52U
#define PICC_CMD_CT                 0x88U
#define PICC_CMD_SEL_CL1            0x93U
#define PICC_CMD_SEL_CL2            0x95U
#define PICC_CMD_SEL_CL3            0x97U
#define PICC_CMD_HLTA               0x50U
#define PICC_CMD_MF_AUTH_KEY_A      0x60U
#define PICC_CMD_MF_AUTH_KEY_B      0x61U
#define PICC_CMD_MF_READ            0x30U
#define PICC_CMD_MF_WRITE           0xA0U

#define MFRC522_OK                  0U
#define MFRC522_ERR                 1U
#define MFRC522_TIMEOUT             2U
#define MFRC522_NO_TAG              3U
#define MFRC522_WRONG_UID           4U

void MFRC522_Init(void);
uint8_t MFRC522_Request(uint8_t req_mode, uint8_t *tag_type);
uint8_t MFRC522_Anticoll(uint8_t *ser_num);
uint8_t MFRC522_CompareUID(const uint8_t *uid, const uint8_t *master_uid);
uint8_t MFRC522_Halt(void);

void MFRC522_WriteReg(uint8_t reg, uint8_t value);
uint8_t MFRC522_ReadReg(uint8_t reg);
void MFRC522_SetRegBits(uint8_t reg, uint8_t mask);
void MFRC522_ClearRegBits(uint8_t reg, uint8_t mask);

#endif /* RC522_H */
