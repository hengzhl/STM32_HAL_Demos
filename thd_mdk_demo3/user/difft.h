#ifndef DIFFT_H
#define DIFFT_H

#include "arm_math.h"
#include <stdint.h>

#define DIFFT_MAX_SIZE 2048
#define DIFFT_MAX_HARMONICS 10 // 最大提取谐波次数
#define DIFFT_SIGMA 0.25f      // 高斯窗标准差
#define DIFFT_ADC_MAX 65535.0f
#define Vref 3.3f
#define DIFFT_ADC_MID (DIFFT_ADC_MAX / 2.0f)
#define DIFFT_MAX_HARMONICS  10     // 最大提取谐波次数
#define DIFFT_SIGMA          0.25f  // 高斯窗标准差


typedef struct
{
    float base; // 基波归一化幅值（=1）
    float h2;   // 二次谐波归一化幅值
    float h3;   // 三次谐波归一化幅值
    float h4;   // 四次谐波归一化幅值
    float h5;   // 五次谐波归一化幅值
    float thd;  // 信号失真度
} HarmonicNorm;

typedef struct
{
    float f; // frequency (Hz)
    float k0;

    float Amp[DIFFT_MAX_HARMONICS];
    float phi[DIFFT_MAX_HARMONICS];

    float dc;
    uint32_t harmonics;

    float vpp_mv;
    float vrms_mv;
    float duty; // duty cycle %
    uint8_t wavetype; // 0-unknown, 1-sine, 2-triangle, 3-square
    HarmonicNorm harmonicNorm;

} DIFFT_Result;



/**
 * @brief 信号分析外置接口
 * @param adc        ADC采集数, 0--DIFFT_ADC_MAX
 * @param N          采样点数
 * @param Fs         采样频率, 单位 Hz
 * @param sigma      高斯窗参数
 * @param result     输出结果, DIFFT_Result结构体的频域信息
 */
uint8_t difft_run_adc(uint16_t *adc, uint32_t N, float Fs, float sigma, DIFFT_Result *result);
uint8_t difft_run_adc2(uint16_t *adc, uint32_t N, float Fs, float sigma, DIFFT_Result *result);

// difft模块
uint8_t difft_run(float *x, uint32_t N, float Fs, float sigma, DIFFT_Result *result);
uint8_t difft_run2(float *x, uint32_t N, float Fs, float sigma, DIFFT_Result *result);

// 谐波分析模块
void difft_compute_harmonic_norm(const DIFFT_Result *res, HarmonicNorm *out); // 计算归一化谐波信息
float compute_thd(HarmonicNorm *h); // 根据归一化谐波信息，计算前五次THD
uint8_t difft_detect_wave(DIFFT_Result *res); // 根据归一化谐波信息，分辨信号波形(1-sine, 2-triangle, 3-square,0-unknown)

// 峰峰值测量模块
float difft_measure_vpp(float A1, uint8_t wavetype); 
void difft_compute_vpp(DIFFT_Result *res); 

/**
 * @brief order次谐波专项欠采样测量接口 
 * @param f0           测量得到的信号基频
 * @param Fs_h5_out    要修改的采样频率
 * @param A5_out       要修改的谐波幅值
 */
uint8_t difft_measure_harmonic_alias(uint16_t *adc, uint32_t N, float f0, float *Fs_out, uint8_t order, float *Amp_out);

uint8_t difft_extract_result(uint16_t *raw_adc, uint32_t N, float32_t Fs_hz, DIFFT_Result *result);

#endif