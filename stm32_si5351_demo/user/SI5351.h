/*
 * SI5351A 软件IIC驱动，输出频率8k-160M
 */
#ifndef SI5351_h
#define SI5351_h

#include "main.h"

#define SCL_H HAL_GPIO_WritePin(SI5351_SCL_GPIO_Port, SI5351_SCL_Pin, GPIO_PIN_SET)
#define SCL_L HAL_GPIO_WritePin(SI5351_SCL_GPIO_Port, SI5351_SCL_Pin, GPIO_PIN_RESET)
#define SDA_H HAL_GPIO_WritePin(SI5351_SDA_GPIO_Port, SI5351_SDA_Pin, GPIO_PIN_SET)
#define SDA_L HAL_GPIO_WritePin(SI5351_SDA_GPIO_Port, SI5351_SDA_Pin, GPIO_PIN_RESET)
#define SDA_read HAL_GPIO_ReadPin(SI5351_SDA_GPIO_Port, SI5351_SDA_Pin)

// Si5351A 寄存器
#define SI_I2C_ADDR             0xC0
#define SI_XTAL_FREQ            (25000000 + 2265)

#define SI_REG_OUTPUT_EN        3     // 输出使能控制
#define SI_REG_CLK0_CTRL        16    // CLK0 控制
#define SI_REG_CLK1_CTRL        17    // CLK1 控制
#define SI_REG_CLK2_CTRL        18    // CLK2 控制
#define SI_REG_PLL_A            26    // PLLA 参数起始z
#define SI_REG_PLL_B            34    // PLLB 参数起始
#define SI_REG_MS0              42    // MS0 参数起始
#define SI_REG_MS1              50    // MS1 参数起始
#define SI_REG_MS2              58    // MS2 参数起始
#define SI_REG_PLL_RESET        177   // PLL 复位
#define SI_REG_XTAL_LOAD        183   // 负载电容设置

// 参数枚举
typedef enum { SI_PLLA = 0, SI_PLLB = 1 } si_pll_t;
typedef enum { SI_CLK0 = 0, SI_CLK1 = 1, SI_CLK2 = 2 } si_clk_t;

// 函数接口
void SI5351_Init(void);
void SI5351_SetFrequency(si_clk_t clk, uint32_t freq_hz, si_pll_t pll);
void SI5351_DisableOutput(si_clk_t clk);

#endif