#include "SI5351.h"

// 粗略延时函数
void Delay_1us(uint16_t n) // 约1us,1100k
{
    unsigned int x = 5, i = 0;
    for (i = 0; i < n; i++)
    {
        while (x--)
            ;
        x = 5;
    }
}
void Delay_ms(uint8_t n) // 约0.5ms，2k
{
    unsigned int x = 5000, i = 0;
    for (i = 0; i < n; i++)
    {
        while (x--)
            ;
        x = 5000;
    }
}
// IIC启动函数 //
void I2C_Start(void)
{
    SDA_H;
    SCL_H;
    Delay_1us(1);
    if (!SDA_read)
        return; // SDA线为低电平则总线忙,退出
    SDA_L;
    Delay_1us(1);
    if (SDA_read)
        return; // SDA线为高电平则总线出错,退出
    SDA_L;
    Delay_1us(1);
    SCL_L;
}
//**************************************
// IIC停止信号
//**************************************
void I2C_Stop(void)
{
    SDA_L;
    SCL_L;
    Delay_1us(1);
    SCL_H;
    SDA_H;
    Delay_1us(1); // 延时
}
//**************************************
// IIC发送应答信号
// 入口参数:ack (0:ACK 1:NAK)
//**************************************
void I2C_SendACK(uint8_t i)
{
    if (1 == i)
        SDA_H; // 写应答信号
    else
        SDA_L;
    SCL_H;        // 拉高时钟线
    Delay_1us(1); // 延时
    SCL_L;        // 拉低时钟线
    Delay_1us(1);
}
//**************************************
// IIC等待应答
// 返回值：ack (1:ACK 0:NAK)
//**************************************
bool I2C_WaitAck(void) // 返回为:=1有ACK,=0无ACK
{
    unsigned int i;
    SDA_H;
    Delay_1us(1);
    SCL_H;
    Delay_1us(1);
    while (SDA_read)
    {
        i++;
        if (i == 5000)
            break;
    }
    if (SDA_read)
    {
        SCL_L;
        Delay_1us(1);
        return FALSE;
    }
    SCL_L;
    Delay_1us(1);
    return TRUE;
}

//**************************************
// 向IIC总线发送一个字节数据
//**************************************
void I2C_SendByte(uint8_t dat)
{
    unsigned int i;
    SCL_L;
    for (i = 0; i < 8; i++) // 8位计数器
    {
        if (dat & 0x80)
        {
            SDA_H;
        } // 送数据口
        else
            SDA_L;
        SCL_H;        // 拉高时钟线
        Delay_1us(1); // 延时
        SCL_L;        // 拉低时钟线
        Delay_1us(1); // 延时
        dat <<= 1;    // 移出数据的最高位
    }
}

//**************************************
// 从IIC总线接收一个字节数据
//**************************************
uint8_t I2C_RecvByte()
{
    uint8_t i;
    uint8_t dat = 0;
    SDA_H;                  // 使能内部上拉,准备读取数据,
    for (i = 0; i < 8; i++) // 8位计数器
    {
        dat <<= 1;
        SCL_H;        // 拉高时钟线
        Delay_1us(1); // 延时
        if (SDA_read) // 读数据
        {
            dat |= 0x01;
        }
        SCL_L; // 拉低时钟线
        Delay_1us(1);
    }
    return dat;
}
//**************************************
// 向IIC设备写入一个字节数据
//**************************************
bool Single_WriteI2C(uint8_t Slave_Address, uint8_t REG_Address, uint8_t REG_data)
{
    I2C_Start();                 // 起始信号
    I2C_SendByte(Slave_Address); // 发送设备地址+写信号
    if (!I2C_WaitAck())
    {
        I2C_Stop();
        return FALSE;
    }
    I2C_SendByte(REG_Address); // 内部寄存器地址，
    if (!I2C_WaitAck())
    {
        I2C_Stop();
        return FALSE;
    }
    I2C_SendByte(REG_data); // 内部寄存器数据，
    if (!I2C_WaitAck())
    {
        I2C_Stop();
        return FALSE;
    }
    I2C_Stop(); // 发送停止信号
    return TRUE;
}

uint8_t Single_ReadI2C(uint8_t Slave_Address, uint8_t REG_Address)
{
    uint8_t REG_data;
    I2C_Start();                 // 起始信号
    I2C_SendByte(Slave_Address); // 发送设备地址+写信号
    if (!I2C_WaitAck())
    {
        I2C_Stop();
        return FALSE;
    }
    I2C_SendByte(REG_Address); // 发送存储单元地址，从0开始
    if (!I2C_WaitAck())
    {
        I2C_Stop();
        return FALSE;
    }
    I2C_Start();                     // 起始信号
    I2C_SendByte(Slave_Address + 1); // 发送设备地址+读信号
    if (!I2C_WaitAck())
    {
        I2C_Stop();
        return FALSE;
    }
    REG_data = I2C_RecvByte(); // 读出寄存器数据
    I2C_SendACK(1);            // 发送停止传输信号
    I2C_Stop();                // 停止信号
    return REG_data;
}

uint8_t i2cSendRegister(uint8_t reg, uint8_t data)
{
    I2C_Start();        // 起始信号
    I2C_SendByte(0xC0); // 发送设备地址+写信号
    if (!I2C_WaitAck())
    {
        I2C_Stop();
        return FALSE;
    }
    I2C_SendByte(reg); // 内部寄存器地址，
    if (!I2C_WaitAck())
    {
        I2C_Stop();
        return FALSE;
    }
    I2C_SendByte(data); // 内部寄存器数据，
    if (!I2C_WaitAck())
    {
        I2C_Stop();
        return FALSE;
    }
    I2C_Stop();
    return 0;
}

void setupMultisynth(uint8_t synth, uint32_t divider, uint8_t rDiv)
{
    uint32_t P1; // 多合成器配置寄存器P1
    uint32_t P2; // 多合成器配置寄存器P2
    uint32_t P3; // 多合成器配置寄存器P3

    // 计算P1的值，这是基于分频比的配置，128是固定小数点运算的一部分，512用于调整
    P1 = 128 * divider - 512;
    P2 = 0; // 将P2设为0和P3设为1，强制使分频器的值为整数
    P3 = 1;

    // 通过I2C发送命令设置多合成器的配置寄存器
    // 发送P3的高8位
    i2cSendRegister(synth + 0, (P3 & 0x0000FF00) >> 8);
    // 发送P3的低8位
    i2cSendRegister(synth + 1, (P3 & 0x000000FF));
    // 发送P1的最高两位和rDiv。rDiv用于进一步分频，控制输出频率的范围
    i2cSendRegister(synth + 2, ((P1 & 0x00030000) >> 16) | rDiv);
    // 发送P1的中间8位
    i2cSendRegister(synth + 3, (P1 & 0x0000FF00) >> 8);
    // 发送P1的低8位
    i2cSendRegister(synth + 4, (P1 & 0x000000FF));
    // 结合P3和P2的部分高位，虽然这里P2始终为0
    i2cSendRegister(synth + 5, ((P3 & 0x000F0000) >> 12) | ((P2 & 0x000F0000) >> 16));
    // 发送P2的中间8位，这里始终为0
    i2cSendRegister(synth + 6, (P2 & 0x0000FF00) >> 8);
    // 发送P2的低8位，这里始终为0
    i2cSendRegister(synth + 7, (P2 & 0x000000FF));
}


void si5351aSetFrequency(uint32_t frequency)
{
    // 局部变量定义
    uint32_t pllFreq;              // PLL频率
    uint32_t xtalFreq = XTAL_FREQ; // 晶体频率，由XTAL_FREQ宏定义指定
    uint32_t l;
    float f;
    uint8_t mult;     // PLL频率乘数
    uint32_t num;     // 分数乘数的分子
    uint32_t denom;   // 分数乘数的分母
    uint32_t divider; // 分频比

    // 计算分频比。900,000,000是内部PLL的最大频率：900MHz
    divider = 900000000 / frequency;
    // 如果divider是奇数，减一使其成为偶数，确保是整数分频比
    if (divider % 2)
        divider--;

    // 计算PLL频率：分频比 * 目标输出频率
    pllFreq = divider * frequency;

    // 确定乘数以达到所需的PLL频率
    mult = pllFreq / xtalFreq;
    // 计算乘数的整数部分和小数部分
    l = pllFreq % xtalFreq;
    f = l;
    // 小数部分的处理：乘以1048575，然后除以晶体频率
    f *= 1048575;
    f /= xtalFreq;
    num = f;
    denom = 1048575; // 简化处理，将分母设为最大值1048575

    // 使用计算出的乘数、分子、分母设置PLL A
    setupPLL(SI_SYNTH_PLL_A, mult, num, denom);

    // 使用计算出的分频比设置MultiSynth除数0。最终R分频阶段可以将频率除以1到128之间的2的幂次
    // 由SI_R_DIV1到SI_R_DIV128常量表示（见si5351a.h头文件）
    // 如果你想输出低于1MHz的频率，必须使用最终的R分频阶段
    setupMultisynth(SI_SYNTH_MS_0, divider, SI_R_DIV_1);

    // 重置PLL。这会导致输出短暂的不稳定。对参数的小改动不需要重置PLL，也就不会有不稳定现象
    i2cSendRegister(SI_PLL_RESET, 0xA0);

    // 最后，打开CLK0输出（0x4F）并设置MultiSynth0的输入为PLL A
    i2cSendRegister(SI_CLK0_CONTROL, 0x4F | SI_CLK_SRC_PLL_A);
}

void si5351aSetFrequency_Low(uint32_t frequency)
{
    // 局部变量定义
    uint32_t pllFreq;              // PLL频率
    uint32_t xtalFreq = XTAL_FREQ; // 晶体频率，由XTAL_FREQ宏定义指定
    uint32_t l;
    float f;
    uint8_t mult;     // PLL频率乘数
    uint32_t num;     // 分数乘数的分子
    uint32_t denom;   // 分数乘数的分母
    uint32_t divider; // 分频比
		frequency = frequency*128;//因为分频128倍，所以先放大128倍。
    // 计算分频比。900,000,000是内部PLL的最大频率：900MHz
    divider = 900000000 / frequency;
    // 如果divider是奇数，减一使其成为偶数，确保是整数分频比
    if (divider % 2)
        divider--;

    // 计算PLL频率：分频比 * 目标输出频率
    pllFreq = divider * frequency;

    // 确定乘数以达到所需的PLL频率
    mult = pllFreq / xtalFreq;
    // 计算乘数的整数部分和小数部分
    l = pllFreq % xtalFreq;
    f = l;
    // 小数部分的处理：乘以1048575，然后除以晶体频率
    f *= 1048575;
    f /= xtalFreq;
    num = f;
    denom = 1048575; // 简化处理，将分母设为最大值1048575

    // 使用计算出的乘数、分子、分母设置PLL A
    setupPLL(SI_SYNTH_PLL_A, mult, num, denom);

    // 使用计算出的分频比设置MultiSynth除数0。最终R分频阶段可以将频率除以1到128之间的2的幂次
    // 由SI_R_DIV1到SI_R_DIV128常量表示（见si5351a.h头文件）
    // 如果你想输出低于1MHz的频率，必须使用最终的R分频阶段

    setupMultisynth(SI_SYNTH_MS_0, divider, SI_R_DIV_128);

    // 重置PLL。这会导致输出短暂的不稳定。对参数的小改动不需要重置PLL，也就不会有不稳定现象
    i2cSendRegister(SI_PLL_RESET, 0xA0);

    // 最后，打开CLK0输出（0x4F）并设置MultiSynth0的输入为PLL A
    i2cSendRegister(SI_CLK0_CONTROL, 0x4F | SI_CLK_SRC_PLL_A);
}

void setupPLL(uint8_t pll, uint8_t mult, uint32_t num, uint32_t denom)
{
    uint32_t P1; // PLL配置寄存器P1
    uint32_t P2; // PLL配置寄存器P2
    uint32_t P3; // PLL配置寄存器P3

    // 计算P1值，首先将分数乘数转换为浮点数进行运算，然后将结果乘以128
    P1 = (uint32_t)(128 * ((float)num / (float)denom));
    // 然后加上乘数mult乘以128，最后减去512
    P1 = (uint32_t)(128 * (uint32_t)(mult) + P1 - 512);
    // 计算P2值，过程同P1，但是计算方式略有不同
    P2 = (uint32_t)(128 * ((float)num / (float)denom));
    // 使用num乘以128减去denom乘以前面计算的P2值
    P2 = (uint32_t)(128 * num - denom * P2);
    // P3简单地等于分母denom
    P3 = denom;

    // 使用I2C发送寄存器值来配置PLL
    // 发送P3的第二个字节
    i2cSendRegister(pll + 0, (P3 & 0x0000FF00) >> 8);
    // 发送P3的第一个字节
    i2cSendRegister(pll + 1, (P3 & 0x000000FF));
    // 发送P1的最高两位
    i2cSendRegister(pll + 2, (P1 & 0x00030000) >> 16);
    // 发送P1的中间八位
    i2cSendRegister(pll + 3, (P1 & 0x0000FF00) >> 8);
    // 发送P1的最低八位
    i2cSendRegister(pll + 4, (P1 & 0x000000FF));
    // 发送P3和P2的最高四位
    i2cSendRegister(pll + 5, ((P3 & 0x000F0000) >> 12) | ((P2 & 0x000F0000) >> 16));
    // 发送P2的中间八位
    i2cSendRegister(pll + 6, (P2 & 0x0000FF00) >> 8);
    // 发送P2的最低八位
    i2cSendRegister(pll + 7, (P2 & 0x000000FF));
}


/**
 * @brief  为SI5351的指定通道设置输出频率
 * @param  channel    要配置的输出通道 (0, 1, 或 2)
 * @param  frequency  目标输出频率，单位 Hz
 * @param  pll_source 使用的PLL (SI_SYNTH_PLL_A 或 SI_SYNTH_PLL_B)
 * @param  r_div      R分频值 (例如 SI_R_DIV_1, SI_R_DIV_8, SI_R_DIV_128)
 */

 /**
 * @brief  R分频选择指南 (r_div 参数参考)
 * -------------------------------------------------------------------
 * 原则：确保内部 VCO 频率 (Frequency * Divider * R_Div) 在 600MHz~900MHz 之间。
 * * 目标输出频率 (f)          | 推荐 R 分频常量      | 实际倍数
 * -------------------------|---------------------|---------
 * f > 1.2 MHz              | SI_R_DIV_1          | 1
 * 150 kHz < f <= 1.2 MHz   | SI_R_DIV_8          | 8
 * 20 kHz < f <= 150 kHz    | SI_R_DIV_64         | 64
 * 2.3 kHz < f <= 20 kHz    | SI_R_DIV_128        | 128
 * -------------------------------------------------------------------
 * 注意：如果不确定，1MHz 以下统统选 SI_R_DIV_128 也是安全的。
 */
void si5351_SetOutputFreq(uint8_t channel, uint32_t frequency, uint8_t pll_source, uint8_t r_div)
{
    uint32_t pllFreq;
    uint32_t xtalFreq = XTAL_FREQ; // 假设 XTAL_FREQ 已定义，例如 #define XTAL_FREQ 25000000
    uint32_t l;
    float f;
    uint8_t mult;
    uint32_t num;
    uint32_t denom = 1048575; // 2^20 - 1
    uint32_t divider;
    
    uint8_t multisynth_base_addr;
    uint8_t clk_control_reg;

    // 根据通道号选择对应的寄存器地址
    switch(channel)
    {
        case 0:
            multisynth_base_addr = SI_SYNTH_MS_0;
            clk_control_reg = SI_CLK0_CONTROL;
            break;
        case 1:
            multisynth_base_addr = SI_SYNTH_MS_1;
            clk_control_reg = SI_CLK1_CONTROL;
            break;
        case 2:
            multisynth_base_addr = SI_SYNTH_MS_2;
            clk_control_reg = SI_CLK2_CONTROL;
            break;
        default:
            return; // 无效通道
    }

    // --- 计算和配置 ---
    
    // 1. 计算分频比 (Divider)
    // 为了提高精度，先将频率乘以R分频的倍数
    // R分频值定义为 (0bxxx << 4)，所以先右移4位获取实际的指数
    uint32_t r_div_val = 1 << (r_div >> 4);
    if (frequency == 0) { // 如果频率为0，则关闭输出
        i2cSendRegister(clk_control_reg, 0x80); // 设置上电位为1，关闭输出
        return;
    }
    
    // 计算所需的PLL频率
    // PLL频率必须在600MHz到900MHz之间
    // F_out = F_pll / (divider * R_div_val)
    // F_pll = F_out * divider * R_div_val
    // 我们选择一个尽可能大的divider来获得最佳精度，但要确保PLL频率不超过900MHz
    divider = 900000000 / (frequency * r_div_val);
    
    // Multisynth分频器(divider)必须是4到2048之间的偶数 (除了特殊情况)
    if (divider < 4) divider = 4;
    if (divider > 2048) divider = 2048; // 数据手册中提到最大为2048
    if (divider % 2) divider--; // 确保是偶数

    // 2. 计算并设置PLL频率
    pllFreq = divider * r_div_val * frequency;

    mult = pllFreq / xtalFreq;
    l = pllFreq % xtalFreq;
    f = l;
    f *= 1048575;
    f /= xtalFreq;
    num = f;

    // 设置PLL (PLLA 或 PLLB)
    setupPLL(pll_source, mult, num, denom);

    // 3. 设置Multisynth分频器
    setupMultisynth(multisynth_base_addr, divider, r_div);

    // 4. 重置PLL (当PLL参数有大的改变时需要)
    // 如果PLLA和PLLB都改变了，则为0xA0, 只改变PLLA为0x20, 只改变PLLB为0x80
    if(pll_source == SI_SYNTH_PLL_A)
    {
        i2cSendRegister(SI_PLL_RESET, 0x20);
    }
    else
    {
        i2cSendRegister(SI_PLL_RESET, 0x80);
    }

    // 5. 启用时钟输出并选择PLL源
    // 0x0F: 8mA驱动, 整数模式, Multisynth N作为源, 上电
    // 0x4F: 8mA驱动, 整数模式, Multisynth N作为源, 上电 (推荐使用)
    uint8_t clk_src_sel = (pll_source == SI_SYNTH_PLL_A) ? SI_CLK_SRC_PLL_A : SI_CLK_SRC_PLL_B;
    i2cSendRegister(clk_control_reg, 0x4F | clk_src_sel);
}

/**
 * @brief  关闭指定的时钟输出通道
 * @param  channel 要关闭的通道 (0, 1, 或 2)
 */
void si5351_DisableOutput(uint8_t channel)
{
    uint8_t clk_control_reg;
    switch(channel)
    {
        case 0: clk_control_reg = SI_CLK0_CONTROL; break;
        case 1: clk_control_reg = SI_CLK1_CONTROL; break;
        case 2: clk_control_reg = SI_CLK2_CONTROL; break;
        default: return;
    }
    // 将时钟控制寄存器的最高位(Clock Enable)设为1来关闭输出
    i2cSendRegister(clk_control_reg, 0x80);
}

/******************* (C) COPYRIGHT 2011 STMicroelectronics *****END OF FILE****/