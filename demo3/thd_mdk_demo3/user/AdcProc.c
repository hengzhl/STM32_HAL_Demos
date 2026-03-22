#include "AdcProc.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h> //包含与C语言中各种整数类型的极限值相关的宏定义,如INT_MAX
#include <math.h>   // 用于 floor 和 ceil 函数

#ifndef ABS
#define ABS(x) ((x) >= 0 ? (x) : -(x))
#endif

/**
 * @brief 计算采样数据的平均值（直流偏置）
 *
 * @param data 原始ADC采样数据数组
 * @param count 采样点数
 * @return float 平均值
 */
float calculate_average(const uint16_t *data, int count)
{
    long long sum = 0;
    for (int i = 0; i < count; ++i)
    {
        sum += data[i];
    }
    return (float)sum / count;
}

/**
 * @brief 将整数数组的元素从原始范围等比例缩放到一个新的范围
 *
 * @param arr 要进行缩放的整数数组
 * @param size 数组的大小
 * @param new_min 新范围的最小值
 * @param new_max 新范围的最大值
 */
void scale_array(uint16_t arr[], uint16_t size, uint16_t new_min, uint16_t new_max)
{
    if (size <= 1)
    {
        // 如果数组大小小于等于1，无法确定原始范围，无需缩放
        if (size == 1)
        {
            arr[0] = new_min; // 或者可以设置为新范围的中间值
        }
        return;
    }

    // 1. 找到数组的原始最大值和最小值
    int original_min = INT_MAX;
    int original_max = INT_MIN;
    for (int i = 0; i < size; i++)
    {
        if (arr[i] < original_min)
        {
            original_min = arr[i];
        }
        if (arr[i] > original_max)
        {
            original_max = arr[i];
        }
    }

    // 如果原始范围为0（所有元素都相同），则将所有元素设置为新范围的最小值
    if (original_min == original_max)
    {
        for (int i = 0; i < size; i++)
        {
            arr[i] = new_min;
        }
        return;
    }

    // 2. 遍历数组，应用缩放公式
    for (int i = 0; i < size; i++)
    {
        // 使用 long long 进行中间计算以防止溢出
        // 公式：new_value = new_min + (original_value - original_min) * (new_max - new_min) / (original_max - original_min)
        // 为了防止整数除法导致的精度损失，先进行乘法运算
        long long scaled_value = (long long)(arr[i] - original_min) * (new_max - new_min);
        scaled_value /= (original_max - original_min);
        scaled_value += new_min;
        arr[i] = (int)scaled_value;
    }
}

uint16_t *find_one_cycle(const uint16_t *adc_data, uint16_t data_len, uint16_t *cycle_len_ptr)
{
    if (adc_data == NULL || data_len < 8 || cycle_len_ptr == NULL)
    {
        return NULL;
    }

    *cycle_len_ptr = 0;

    /* 1) 扫描 min/max，优先用中值线而不是平均值 */
    uint16_t min_val = adc_data[0];
    uint16_t max_val = adc_data[0];

    for (uint16_t i = 1; i < data_len; ++i)
    {
        if (adc_data[i] < min_val)
            min_val = adc_data[i];
        if (adc_data[i] > max_val)
            max_val = adc_data[i];
    }

    uint16_t span = max_val - min_val;

    /* 幅度太小，基本没法可靠找周期 */
    if (span < 8)
    {
        return NULL;
    }

    /* 2) 中线 + 迟滞 */
    uint16_t mid = (uint16_t)(((uint32_t)min_val + (uint32_t)max_val) / 2U);

    /* 迟滞取幅度的 5%，并设置最小值，避免噪声抖动 */
    uint16_t hys = span / 20U;
    if (hys < 4U)
        hys = 4U;

    uint16_t low = (mid > hys) ? (mid - hys) : 0U;
    uint16_t high = mid + hys;

    /* 3) 查找多个“带迟滞的向上穿越”点
       条件：先回到 low 以下，再向上穿过 high */
    uint16_t crossings[32];
    uint16_t cross_count = 0;
    uint8_t armed = 0;

    /* 先等待信号掉到 low 以下，进入 armed 状态 */
    for (uint16_t i = 0; i < data_len; ++i)
    {
        if (adc_data[i] <= low)
        {
            armed = 1;
            break;
        }
    }

    for (uint16_t i = 1; i < data_len; ++i)
    {
        if (adc_data[i] <= low)
        {
            armed = 1;
        }

        if (armed && adc_data[i - 1] < high && adc_data[i] >= high)
        {
            if (cross_count < (uint16_t)(sizeof(crossings) / sizeof(crossings[0])))
            {
                crossings[cross_count++] = i;
            }
            armed = 0;
        }
    }

    /* 至少要有两个同向穿越点，才能形成一个周期 */
    if (cross_count < 2)
    {
        return NULL;
    }

    /* 4) 计算相邻穿越点间隔，过滤过短假周期 */
    uint16_t diffs[31];
    uint16_t diff_count = 0;

    for (uint16_t i = 1; i < cross_count; ++i)
    {
        uint16_t d = (uint16_t)(crossings[i] - crossings[i - 1]);

        /* 过短大多是噪声误触发，这里给一个下限 */
        if (d >= 8U)
        {
            diffs[diff_count++] = d;
        }
    }

    if (diff_count == 0)
    {
        return NULL;
    }

    /* 5) 对 diffs 排序，取中位数作为稳定周期估计 */
    for (uint16_t i = 0; i < diff_count; ++i)
    {
        for (uint16_t j = i + 1; j < diff_count; ++j)
        {
            if (diffs[j] < diffs[i])
            {
                uint16_t tmp = diffs[i];
                diffs[i] = diffs[j];
                diffs[j] = tmp;
            }
        }
    }

    uint16_t period = diffs[diff_count / 2U];

    if (period < 8U)
    {
        return NULL;
    }

    /* 6) 从 crossings 中选一个最接近 period 的区间
          尽量取中间位置，避免边缘周期不完整 */
    int best_pair = -1;
    uint16_t best_err = 0xFFFF;

    for (uint16_t i = 0; i + 1 < cross_count; ++i)
    {
        uint16_t d = (uint16_t)(crossings[i + 1] - crossings[i]);
        uint16_t err = (uint16_t)ABS((int)d - (int)period);

        /* 允许 25% 误差 */
        if (err <= period / 4U)
        {
            /* 更倾向选中间的 pair */
            uint16_t center_bias = (uint16_t)ABS((int)i - (int)(cross_count / 2U));
            uint16_t score = (uint16_t)(err + center_bias);

            if (best_pair < 0 || score < best_err)
            {
                best_pair = (int)i;
                best_err = score;
            }
        }
    }

    /* 如果没有找到满足误差范围的，用最接近 period 的一段兜底 */
    if (best_pair < 0)
    {
        for (uint16_t i = 0; i + 1 < cross_count; ++i)
        {
            uint16_t d = (uint16_t)(crossings[i + 1] - crossings[i]);
            uint16_t err = (uint16_t)ABS((int)d - (int)period);

            if (best_pair < 0 || err < best_err)
            {
                best_pair = (int)i;
                best_err = err;
            }
        }
    }

    if (best_pair < 0)
    {
        return NULL;
    }

    uint16_t start_index = crossings[best_pair];
    uint16_t end_index = crossings[best_pair + 1];

    if (end_index <= start_index)
    {
        return NULL;
    }

    *cycle_len_ptr = (uint16_t)(end_index - start_index);

    if (*cycle_len_ptr < 8U || *cycle_len_ptr > data_len)
    {
        *cycle_len_ptr = 0;
        return NULL;
    }

    /* 7) 分配内存并复制 */
    uint16_t *cycle_data = (uint16_t *)malloc((size_t)(*cycle_len_ptr) * sizeof(uint16_t));
    if (cycle_data == NULL)
    {
        *cycle_len_ptr = 0;
        return NULL;
    }

    for (uint16_t i = 0; i < *cycle_len_ptr; ++i)
    {
        cycle_data[i] = adc_data[start_index + i];
    }

    return cycle_data;
}


void resample_u16(const uint16_t *original_array, uint16_t original_size,
                  uint16_t *new_array, uint16_t new_size)
{
    if (original_array == NULL || new_array == NULL ||
        original_size == 0 || new_size == 0)
    {
        return;
    }

    /* 原数组只有一个点：全部填成同一个值 */
    if (original_size == 1)
    {
        for (uint16_t i = 0; i < new_size; ++i)
        {
            new_array[i] = original_array[0];
        }
        return;
    }

    /* 新数组只有一个点：直接取原数组第一个点
       也可以改成取中点/平均值，但这里保持最简单稳定 */
    if (new_size == 1)
    {
        new_array[0] = original_array[0];
        return;
    }

    /* Q16 定点步长：
       pos_q16 = i * step_q16
       index   = pos_q16 >> 16
       frac    = pos_q16 & 0xFFFF
    */
    uint32_t step_q16 = (((uint32_t)(original_size - 1)) << 16) / (uint32_t)(new_size - 1);

    for (uint16_t i = 0; i < new_size; ++i)
    {
        uint32_t pos_q16 = (uint32_t)i * step_q16;
        uint16_t index = (uint16_t)(pos_q16 >> 16);
        uint16_t frac = (uint16_t)(pos_q16 & 0xFFFF);

        if (index >= (uint16_t)(original_size - 1))
        {
            new_array[i] = original_array[original_size - 1];
        }
        else
        {
            uint32_t y0 = original_array[index];
            uint32_t y1 = original_array[index + 1];

            /* 线性插值：
               y = y0*(1-frac) + y1*frac
               frac 的范围是 [0, 65535]
            */
            uint32_t y =
                (y0 * (65536U - frac) + y1 * frac) >> 16;

            new_array[i] = (uint16_t)y;
        }
    }
}