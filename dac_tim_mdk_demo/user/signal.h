/**
 * @file signal.h
 * @brief 快速整数波形生成模块接口
 * @note 所有电压参数均以 mV 为单位，使用 uint16_t 类型。
 * 峰峰值 (Vpp) 与 偏置 (Offset) 的组合不应超过硬件参考电压 (3300mV)。
 */

#ifndef __SIGNAL_H__
#define __SIGNAL_H__

/* 兼容 C++ 编译环境 */
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* --- 硬件与波形配置常量 --- */

/** @brief 每个周期的采样点数 */
#define NPoints     128

/** @brief 系统参考电压 (mV) */
#define V_REF_MV    3300

/** @brief 12位DAC最大量化值 */
#define DAC_MAX_VAL 4095


/* --- 导出函数声明 --- */

/**
 * @brief 快速整数正弦波产生函数
 * @param Vpp_mv     信号峰峰值 (mV)
 * @param Offset_mv  信号直流偏置 (mV)
 * @param Table      指向长度至少为 NPoints 的 uint16_t 数组
 */
void SineTableGen(uint16_t Vpp_mv, uint16_t Offset_mv, uint16_t *Table);

/**
 * @brief 快速整数三角波产生函数
 * @param Vpp_mv     信号峰峰值 (mV)
 * @param Offset_mv  信号直流偏置/中心电压 (mV)
 * @param Table      指向长度至少为 NPoints 的 uint16_t 数组
 */
void TriangleTableGen(uint16_t Vpp_mv, uint16_t Offset_mv, uint16_t *Table);

/**
 * @brief 快速整数方波产生函数
 * @param Vpp_mv     信号峰峰值 (mV)
 * @param Offset_mv  信号直流偏置 (mV)
 * @param Duty       占空比 (取值范围: 0 - 100)
 * @param Table      指向长度至少为 NPoints 的 uint16_t 数组
 */
void SquareTableGen(uint16_t Vpp_mv, uint16_t Offset_mv, uint16_t Duty, uint16_t *Table);

#ifdef __cplusplus
}
#endif

#endif /* __SIGNAL_H__ */