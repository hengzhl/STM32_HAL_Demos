#ifndef SI5351_h
#define SI5351_h

#include "main.h"
#include "gpio.h"

// 寄存器定义
#define SI_CLK0_CONTROL 16  // 时钟0控制寄存器地址
#define SI_CLK1_CONTROL 17  // 时钟1控制寄存器地址
#define SI_CLK2_CONTROL 18  // 时钟2控制寄存器地址
#define SI_SYNTH_PLL_A  26  // PLL A频率合成器地址
#define SI_SYNTH_PLL_B  34  // PLL B频率合成器地址
#define SI_SYNTH_MS_0   42  // 频率合成器参数0地址
#define SI_SYNTH_MS_1   50  // 频率合成器参数1地址
#define SI_SYNTH_MS_2   58  // 频率合成器参数2地址
#define SI_PLL_RESET    177 // PLL复位寄存器地址

// R-分频比定义
#define SI_R_DIV_1      0x00 // 分频比为1
#define SI_R_DIV_2      0x10 // 分频比为2
#define SI_R_DIV_4      0x20 // 分频比为4
#define SI_R_DIV_8      0x30 // 分频比为8
#define SI_R_DIV_16     0x40 // 分频比为16
#define SI_R_DIV_32     0x50 // 分频比为32
#define SI_R_DIV_64     0x60 // 分频比为64
#define SI_R_DIV_128    0x70 // 分频比为128

// 时钟源选择
#define SI_CLK_SRC_PLL_A      0x00 // 选择PLL A作为时钟源
#define SI_CLK_SRC_PLL_B      0x20 // 选择PLL B作为时钟源

// 晶体频率
#define XTAL_FREQ (25000000 + 2265)

// typedef unsigned short int uint;
typedef enum
{
    FALSE = 0,
    TRUE = !FALSE
} bool;

// IIC总线引脚配置
#define SCL_H HAL_GPIO_WritePin(SI5351_SCL_GPIO_Port, SI5351_SCL_Pin, GPIO_PIN_SET)
#define SCL_L HAL_GPIO_WritePin(SI5351_SCL_GPIO_Port, SI5351_SCL_Pin, GPIO_PIN_RESET)

#define SDA_H HAL_GPIO_WritePin(SI5351_SDA_GPIO_Port, SI5351_SDA_Pin, GPIO_PIN_SET)
#define SDA_L HAL_GPIO_WritePin(SI5351_SDA_GPIO_Port, SI5351_SDA_Pin, GPIO_PIN_RESET)

#define SCL_read HAL_GPIO_ReadPin(SI5351_SCL_GPIO_Port, SI5351_SCL_Pin)
#define SDA_read HAL_GPIO_ReadPin(SI5351_SDA_GPIO_Port, SI5351_SDA_Pin)

void I2C_SendByte(uint8_t dat);
uint8_t i2cSendRegister(uint8_t reg, uint8_t data);
void setupPLL(uint8_t pll, uint8_t mult, uint32_t num, uint32_t denom);
void setupMultisynth(uint8_t synth, uint32_t divider, uint8_t rDiv);
void si5351aSetFrequency(uint32_t frequency);//设置SI5351的输出频率，频率范围为1MHz-150MHz
void si5351aSetFrequency_Low(uint32_t frequency);//设置SI5351的输出频率，频率范围为7813Hz-1MHz

void si5351_SetOutputFreq(uint8_t channel, uint32_t frequency, uint8_t pll_source, uint8_t r_div);
void si5351_DisableOutput(uint8_t channel);

#endif