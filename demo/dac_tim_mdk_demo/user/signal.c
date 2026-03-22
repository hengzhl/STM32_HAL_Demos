/**
 * @file signal.c
 * @brief 规范化波形数据表生成模块
 * @author heng_zhl
 * @date 2026.03.07
 */

#include "signal.h"

/* --- 预生成的归一化正弦表 (128点, 0 -> 4095 对应 0 -> 2*PI) --- */
/* 此表代表：Value = (sin(angle) + 1) * 2047.5 */
static const uint16_t LUT_Sine[NPoints] = {
    2048, 2148, 2248, 2348, 2447, 2545, 2642, 2737, 2831, 2923, 3013, 3100, 3185, 3267, 3346, 3422,
    3495, 3564, 3630, 3692, 3750, 3804, 3853, 3898, 3939, 3975, 4007, 4034, 4056, 4073, 4085, 4093,
    4095, 4093, 4085, 4073, 4056, 4034, 4007, 3975, 3939, 3898, 3853, 3804, 3750, 3692, 3630, 3564,
    3495, 3422, 3346, 3267, 3185, 3100, 3013, 2923, 2831, 2737, 2642, 2545, 2447, 2348, 2248, 2148,
    2048, 1947, 1847, 1747, 1648, 1550, 1453, 1358, 1264, 1172, 1082,  995,  910,  828,  749,  673,
     600,  531,  465,  403,  345,  291,  242,  197,  156,  120,   88,   61,   39,   22,   10,    2,
       0,    2,   10,   22,   39,   61,   88,  120,  156,  197,  242,  291,  345,  403,  465,  531,
     600,  673,  749,  828,  910,  995, 1082, 1172, 1264, 1358, 1453, 1550, 1648, 1747, 1847, 1947
};

/**
 * @brief 快速整数正弦波生成
 * @param Vpp_mv     峰峰值 (mV)
 * @param Offset_mv  直流偏置 (mV)
 */
void SineTableGen(uint16_t Vpp_mv, uint16_t Offset_mv, uint16_t *Table)
{
    // 预计算 DAC 对应的 Vpp 增益和 Offset 偏移
    uint32_t dac_vpp = (uint32_t)Vpp_mv * DAC_MAX_VAL / V_REF_MV;
    uint32_t dac_offset = (uint32_t)Offset_mv * DAC_MAX_VAL / V_REF_MV;

    for (int i = 0; i < NPoints; i++)
    {
        // 归一化处理：LUT_Sine[i] - 2048 得到 -2048 到 2047 的相对偏移
        int32_t rel_val = (int32_t)LUT_Sine[i] - 2048;
        
        // 计算公式：(相对偏移 * 增益 / 4096) + 偏移量
        // >> 12 对应 1/4096 的缩放
        int32_t val = ((rel_val * (int32_t)dac_vpp) >> 12) + (int32_t)dac_offset;

        // 边界钳位保护
        if (val > DAC_MAX_VAL) val = DAC_MAX_VAL;
        else if (val < 0)      val = 0;

        Table[i] = (uint16_t)val;
    }
}

/**
 * @brief 快速整数三角波生成
 * @param Vpp_mv     峰峰值 (mV)
 * @param Offset_mv  直流偏置/中心电压 (mV)
 */
void TriangleTableGen(uint16_t Vpp_mv, uint16_t Offset_mv, uint16_t *Table)
{
    // 计算高低电平端点
    int32_t high_dac = ((int32_t)Offset_mv + (Vpp_mv >> 1)) * DAC_MAX_VAL / V_REF_MV;
    int32_t low_dac  = ((int32_t)Offset_mv - (Vpp_mv >> 1)) * DAC_MAX_VAL / V_REF_MV;

    // 硬件边界检查
    if (high_dac > DAC_MAX_VAL) high_dac = DAC_MAX_VAL;
    if (low_dac < 0)            low_dac = 0;

    uint32_t span = (uint32_t)(high_dac - low_dac);
    uint32_t half = NPoints >> 1;

    for (uint32_t i = 0; i < NPoints; i++)
    {
        if (i < half) {
            // 上升沿线性插值
            Table[i] = (uint16_t)(low_dac + (span * i / half));
        } else {
            // 下降沿线性插值
            Table[i] = (uint16_t)(high_dac - (span * (i - half) / (NPoints - half)));
        }
    }
}

/**
 * @brief 快速整数方波生成
 * @param Vpp_mv     峰峰值 (mV),默认低电平0V
 * @param Offset_mv  直流偏置 (mV)
 * @param Duty       占空比 (0-100)
 */
void SquareTableGen(uint16_t Vpp_mv, uint16_t Offset_mv, uint16_t Duty, uint16_t *Table)
{
    // 计算高低电平对应的 DAC 数值
    int32_t high_dac = ((int32_t)Offset_mv + (Vpp_mv >> 1)) * DAC_MAX_VAL / V_REF_MV;
    int32_t low_dac  = ((int32_t)Offset_mv - (Vpp_mv >> 1)) * DAC_MAX_VAL / V_REF_MV;

    // 边界检查
    if (high_dac > DAC_MAX_VAL) high_dac = DAC_MAX_VAL;
    if (low_dac < 0)            low_dac = 0;

    // 根据占空比计算切换点
    uint32_t threshold = ((uint32_t)Duty * NPoints) / 100;

    for (uint32_t i = 0; i < NPoints; i++)
    {
        Table[i] = (i < threshold) ? (uint16_t)high_dac : (uint16_t)low_dac;
    }
}