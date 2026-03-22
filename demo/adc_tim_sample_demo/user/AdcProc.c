#include "AdcProc.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h> //包含与C语言中各种整数类型的极限值相关的宏定义,如INT_MAX
#include <math.h> // 用于 floor 和 ceil 函数


/**
 * @brief 计算采样数据的平均值（直流偏置）
 * 
 * @param data 原始ADC采样数据数组
 * @param count 采样点数
 * @return float 平均值
 */
float calculate_average(const uint16_t* data, int count) {
    long long sum = 0;
    for (int i = 0; i < count; ++i) {
        sum += data[i];
    }
    return (float)sum / count;
}

/**
 * @brief 从ADC数据中查找一个周期的采样点
 *
 * @param adc_data      输入的ADC原始数据数组
 * @param data_len      输入数据的长度
 * @param cycle_len_ptr 用于返回找到的周期长度的指针
 * @return uint16_t*    返回一个动态分配的数组，包含一个周期的数据。如果找不到，返回NULL。
 *                      使用者需要在使用后手动free释放内存。
 */
uint16_t* find_one_cycle(const uint16_t* adc_data, uint16_t data_len, uint16_t* cycle_len_ptr) {
    if (adc_data == NULL || data_len < 2 || cycle_len_ptr == NULL) {
        return NULL;
    }

    // 1. 计算平均值
    float average = calculate_average(adc_data, data_len);

    int start_index = -1;
    int end_index = -1;

    // 2. 寻找第一个向上穿过平均值的点（周期起点）
    for (int i = 0; i < data_len - 1; ++i) {
        if (adc_data[i] < average && adc_data[i + 1] >= average) {
            start_index = i + 1;
            break;
        }
    }

    // 如果没有找到起始点，说明可能没有完整的周期
    if (start_index == -1) {
        return NULL;
    }

    // 3. 从起点之后，寻找第二个向上穿过平均值的点（周期终点）
    for (int i = start_index; i < data_len - 1; ++i) {
        if (adc_data[i] < average && adc_data[i + 1] >= average) {
            end_index = i + 1;
            break;
        }
    }

    // 如果没有找到结束点，说明数据中不足一个完整周期
    if (end_index == -1) {
        return NULL;
    }

    // 4. 计算周期长度
    *cycle_len_ptr = end_index - start_index;
    
    // 周期长度过短可能是一个噪声点，可以加入一个阈值判断
    if (*cycle_len_ptr <= 2) {
        return NULL;
    }

    // 5. 分配内存并复制一个周期的数据
    uint16_t* cycle_data = (uint16_t*)malloc(*cycle_len_ptr * sizeof(uint16_t));
    if (cycle_data == NULL) {
        // 内存分配失败
        return NULL;
    }

    for (int i = 0; i < *cycle_len_ptr; ++i) {
        cycle_data[i] = adc_data[start_index + i];
    }

    return cycle_data;
}


/**
 * @brief 自适应重采样：无论原始数组比新数组大还是小，都能平滑缩放。
 * * 当原始数据多时，它类似于降采样；当原始数据少时，它通过线性插值补充缺失点。
 */
void resample_u16(const uint16_t* original_array, uint16_t original_size, uint16_t* new_array, uint16_t new_size) {
    if (original_array == NULL || new_array == NULL || original_size == 0 || new_size == 0) {
        return;
    }

    if (original_size == 1) {
        for (int i = 0; i < new_size; i++) new_array[i] = original_array[0];
        return;
    }

    // 计算步长：将原始数据的间隙均匀分布到新数组中
    double step = (double)(original_size - 1) / (new_size - 1);

    for (int i = 0; i < new_size; ++i) {
        double pos = i * step;          // 当前点在原数组中的浮点位置
        int index = (int)pos;           // 左侧点索引
        double fraction = pos - index;  // 两个原始点之间的比例偏移

        if (index >= original_size - 1) {
            new_array[i] = original_array[original_size - 1];
        } else {
            // 线性插值公式：y = y0 + (y1 - y0) * fraction
            // 这样即使原始点很少，输出的波形也是平滑连线的
            uint16_t y0 = original_array[index];
            uint16_t y1 = original_array[index + 1];
            new_array[i] = (uint16_t)(y0 + (double)(y1 - y0) * fraction);
        }
    }
}



/**
 * @brief 将整数数组的元素从原始范围等比例缩放到一个新的范围
 *
 * @param arr 要进行缩放的整数数组
 * @param size 数组的大小
 * @param new_min 新范围的最小值
 * @param new_max 新范围的最大值
 */
void scale_array(uint16_t arr[], uint16_t size, uint16_t new_min, uint16_t new_max) {
    if (size <= 1) {
        // 如果数组大小小于等于1，无法确定原始范围，无需缩放
        if (size == 1) {
            arr[0] = new_min; // 或者可以设置为新范围的中间值
        }
        return;
    }

    // 1. 找到数组的原始最大值和最小值
    int original_min = INT_MAX;
    int original_max = INT_MIN;
    for (int i = 0; i < size; i++) {
        if (arr[i] < original_min) {
            original_min = arr[i];
        }
        if (arr[i] > original_max) {
            original_max = arr[i];
        }
    }

    // 如果原始范围为0（所有元素都相同），则将所有元素设置为新范围的最小值
    if (original_min == original_max) {
        for (int i = 0; i < size; i++) {
            arr[i] = new_min;
        }
        return;
    }

    // 2. 遍历数组，应用缩放公式
    for (int i = 0; i < size; i++) {
        // 使用 long long 进行中间计算以防止溢出
        // 公式：new_value = new_min + (original_value - original_min) * (new_max - new_min) / (original_max - original_min)
        // 为了防止整数除法导致的精度损失，先进行乘法运算
        long long scaled_value = (long long)(arr[i] - original_min) * (new_max - new_min);
        scaled_value /= (original_max - original_min);
        scaled_value += new_min;
        arr[i] = (int)scaled_value;
    }
}