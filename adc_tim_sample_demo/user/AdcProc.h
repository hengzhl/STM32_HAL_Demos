/**
 * @file AdcProc.h
 * @brief 用于处理ADC采样数据的函数库头文件
 * @author 24029100337
 * @version 1.0
 * @date 2025-10-3
 *
 * @copyright Copyright (c) 2025
 *
 * @note
 * 这个文件声明了一系列用于分析和处理ADC波形数据的函数，
 * 包括计算平均值、查找单个周期以及对数据进行降采样。
 * 适用于资源受限的单片机环境。
 */

#ifndef ADCPROC_H
#define ADCPROC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h> // 包含了uint16_t等标准整数类型的定义

//==============================================================================
// 宏定义 (MACROS)
//==============================================================================

#define ADC_SAMPLE_COUNT 4096       // 假设ADC采样值为12位，范围在0-4095之间

//==============================================================================
// 函数原型声明 (FUNCTION PROTOTYPES)
//==============================================================================

/**
 * @brief 计算采样数据的平均值（直流偏置）
 * 
 * @param data 原始ADC采样数据数组
 * @param count 采样点数
 * @return float 平均值
 */
float calculate_average(const uint16_t* data, int count);

/**
 * @brief 从ADC数据中查找一个周期的采样点
 *
 * @param adc_data      输入的ADC原始数据数组
 * @param data_len      输入数据的长度
 * @param cycle_len_ptr 用于返回找到的周期长度的指针。函数成功时，这里会存储一个周期包含的采样点数。
 * @return uint16_t*    返回一个在堆上动态分配的数组，包含一个周期的数据。
 *                      如果找不到完整的周期或发生错误，返回NULL。
 * @warning             使用者需要在使用后手动调用 free() 来释放返回的数组所占用的内存，以避免内存泄漏。
 */
uint16_t* find_one_cycle(const uint16_t* adc_data, uint16_t data_len, uint16_t* cycle_len_ptr);

void resample_u16(const uint16_t* original_array, uint16_t original_size, uint16_t* new_array, uint16_t new_size);
/**
 * @brief 将整数数组的元素从原始范围等比例缩放到一个新的范围
 *
 * @param arr 要进行缩放的整数数组
 * @param size 数组的大小
 * @param new_min 新范围的最小值
 * @param new_max 新范围的最大值
 */
void scale_array(uint16_t arr[], uint16_t size, uint16_t new_min, uint16_t new_max); 


#ifdef __cplusplus
}
#endif

#endif // ADCPROC_H