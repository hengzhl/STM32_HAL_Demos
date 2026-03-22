#include "SI5351.h"

// ================== 第一部分：最优化底层驱动 ==================

static void I2C_Delay(void) {
    uint8_t i = 10; while(i--); // 约 1-2us
}

void I2C_Start(void) {
    SDA_H; SCL_H; I2C_Delay(); SDA_L; I2C_Delay(); SCL_L;
}

void I2C_Stop(void) {
    SDA_L; SCL_L; I2C_Delay(); SCL_H; I2C_Delay(); SDA_H;
}

uint8_t I2C_WaitAck(void) {
    uint16_t timer = 0;
    SDA_H; I2C_Delay(); SCL_H; I2C_Delay();
    while (SDA_read) { if (++timer > 2000) break; }
    SCL_L; I2C_Delay();
    return (timer < 2000);
}

void I2C_SendByte(uint8_t dat) {
    for (uint8_t i = 0; i < 8; i++) {
        if (dat & 0x80) SDA_H; else SDA_L;
        SCL_H; I2C_Delay(); SCL_L; I2C_Delay();
        dat <<= 1;
    }
}

// 核心写操作：支持批量写入（用于一次性推入 8 字节参数）
void SI5351_WriteBulk(uint8_t reg, uint8_t *data, uint8_t len) {
    I2C_Start();
    I2C_SendByte(SI_I2C_ADDR); I2C_WaitAck();
    I2C_SendByte(reg);         I2C_WaitAck();
    for (uint8_t i = 0; i < len; i++) {
        I2C_SendByte(data[i]); I2C_WaitAck();
    }
    I2C_Stop();
}

void SI5351_WriteReg(uint8_t reg, uint8_t data) {
    SI5351_WriteBulk(reg, &data, 1);
}

// ================== 第二部分：Si5351A 参数逻辑 ==================

/**
 * @brief Si5351 通用 8 字节 P-参数打包器
 * 自动处理 den(20bit), P1(18bit), P2(20bit) 到寄存器位的复杂映射
 */
static void _SI5351_Pack8(uint8_t *buf, uint32_t ratio_int, uint32_t num, uint32_t den, uint8_t r_div) {
    uint32_t P1 = 128 * ratio_int + (128 * num / den) - 512;
    uint32_t P2 = 128 * num - den * (128 * num / den);

    buf[0] = (uint8_t)(den >> 8);
    buf[1] = (uint8_t)den;
    buf[2] = (uint8_t)((P1 >> 16) & 0x03) | r_div;
    buf[3] = (uint8_t)(P1 >> 8);
    buf[4] = (uint8_t)P1;
    buf[5] = (uint8_t)(((den >> 12) & 0xF0) | ((P2 >> 16) & 0x0F));
    buf[6] = (uint8_t)(P2 >> 8);
    buf[7] = (uint8_t)P2;
}

void SI5351_Init(void) {
    SI5351_WriteReg(SI_REG_XTAL_LOAD, 0xD2); // 10pF 负载电容
    SI5351_WriteReg(SI_REG_OUTPUT_EN, 0xFF); // 初始全关
}

/**
 * @brief 合并高低频、支持手动选择 PLL
 */
void SI5351_SetFrequency(si_clk_t clk, uint32_t freq_hz, si_pll_t pll) {
    uint8_t  params[8];
    uint32_t ms_div, vco_freq;
    uint8_t  r_div_bits = 0;
    uint32_t calc_freq = freq_hz;

    // 1. 低频自动衔接：频率 < 1MHz 时使用 R 分频器(128)
    if (calc_freq < 1000000) {
        r_div_bits = 0x70; // SI_R_DIV_128
        calc_freq *= 128;
    }

    // 2. 计算 Multisynth 分频比 (为了相噪，尽量用偶数整数分频)
    ms_div = 900000000 / calc_freq; // 目标 VCO 约 900MHz
    if (ms_div % 2) ms_div--;
    if (ms_div < 8) ms_div = 8;     // 硬件最小限值

    vco_freq = ms_div * calc_freq;

    // 3. 计算并配置 PLL (Fractional 模式)
    uint32_t mult = vco_freq / SI_XTAL_FREQ;
    uint32_t num  = (uint32_t)(((uint64_t)(vco_freq % SI_XTAL_FREQ) * 1048575) / SI_XTAL_FREQ);
    _SI5351_Pack8(params, mult, num, 1048575, 0);
    SI5351_WriteBulk((pll == SI_PLLA) ? SI_REG_PLL_A : SI_REG_PLL_B, params, 8);

    // 4. 配置通道 Multisynth (Integer 模式)
    _SI5351_Pack8(params, ms_div, 0, 1, r_div_bits);
    SI5351_WriteBulk(SI_REG_MS0 + (clk * 8), params, 8);

    // 5. 应用配置：复位 PLL 并开启输出
    SI5351_WriteReg(SI_REG_PLL_RESET, (pll == SI_PLLA) ? 0x20 : 0x80);

    // 控制位：8mA, 开启, 整数模式, 指定 PLL
    uint8_t ctrl = 0x4F | (pll << 5);
    if (freq_hz > 112500000) ctrl |= 0x40; // 高频强制整数位
    SI5351_WriteReg(SI_REG_CLK0_CTRL + clk, ctrl);

    // 解禁该通道输出
    static uint8_t oe_status = 0xFF;
    oe_status &= ~(1 << clk);
    SI5351_WriteReg(SI_REG_OUTPUT_EN, oe_status);
}

void SI5351_DisableOutput(si_clk_t clk) {
    SI5351_WriteReg(SI_REG_CLK0_CTRL + clk, 0x80);
}