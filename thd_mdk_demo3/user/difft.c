#include "difft.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#ifndef SAFE_LOG
#define SAFE_LOG(x) logf((x) > 1e-12f ? (x) : 1e-12f)
#endif


#ifndef MATH_PI
#define MATH_PI 3.14159265358979323846f
#endif

#ifndef MATH_2PI
#define MATH_2PI 6.28318530717958647692f
#endif

/* 只预分配工作缓冲区 */
static float difft_buf[DIFFT_MAX_SIZE]; // 时域
static float difft_out[DIFFT_MAX_SIZE]; // 频域
static float WORK_BUF3[DIFFT_MAX_SIZE]; // 5

/*========================================================
  1. Internal helpers
========================================================*/

// 判断 N 是否为 2 的幂（FFT要求）
static int is_power_of_two(uint32_t n)
{
    return (n & (n - 1)) == 0;
}

// 使用最小二乘法（Least Squares, LS）估计一个给定基频（f0）的正弦波在信号中的幅度
static float difft_measure_fundamental_amp_ls(const float *x, uint32_t N, float Fs, float f0)
{
    float w = 2.0f * MATH_PI * f0 / Fs;

    float Scc = 0, Sss = 0, Scs = 0;
    float Sc = 0, Ss = 0;
    float Sx = 0, Sxc = 0, Sxs = 0;
    for (uint32_t n = 0; n < N; n++)
    {
        float c = cosf(w * n);
        float s = sinf(w * n);
        float xn = x[n];

        Scc += c * c;
        Sss += s * s;
        Scs += c * s;
        Sc += c;
        Ss += s;
        Sx += xn;

        Sxc += xn * c;
        Sxs += xn * s;
    }

    float D =
        Scc * (Sss * N - Ss * Ss) - Scs * (Scs * N - Ss * Sc) + Sc * (Scs * Ss - Sss * Sc);

    if (fabsf(D) < 1e-12f)
        return 0;

    float Da =
        Sxc * (Sss * N - Ss * Ss) - Scs * (Sxs * N - Ss * Sx) + Sc * (Sxs * Ss - Sss * Sx);

    float Db =
        Scc * (Sxs * N - Ss * Sx) - Sxc * (Scs * N - Ss * Sc) + Sc * (Scs * Sx - Sxc * Ss);

    float a = Da / D;
    float b = Db / D;

    return sqrtf(a * a + b * b);
}

// 计算给定频率在采样率为 Fs 的离散系统中会折叠（混叠）到哪个频率位置。
static float difft_fold_alias(float f, float Fs)
{
    float x = fmodf(f, Fs);
    if (x < 0.0f)
        x += Fs;

    if (x > 0.5f * Fs)
        x = Fs - x;

    return x;
}

// 求解线性方程组的高斯消元法实现
static int difft_solve_linear(float *A, float *b, float *x, int n)
{
    for (int i = 0; i < n; i++)
    {
        int pivot = i;
        float maxv = fabsf(A[i * n + i]);

        for (int r = i + 1; r < n; r++)
        {
            float v = fabsf(A[r * n + i]);

            if (v > maxv)
            {
                maxv = v;
                pivot = r;
            }
        }

        if (maxv < 1e-12f)
            return -1;

        if (pivot != i)
        {
            for (int c = 0; c < n; c++)
            {
                float tmp = A[i * n + c];
                A[i * n + c] = A[pivot * n + c];
                A[pivot * n + c] = tmp;
            }

            float tb = b[i];
            b[i] = b[pivot];
            b[pivot] = tb;
        }

        float diag = A[i * n + i];
        for (int c = i; c < n; c++)
            A[i * n + c] /= diag;
        b[i] /= diag;

        for (int r = i + 1; r < n; r++)
        {
            float factor = A[r * n + i];
            if (fabsf(factor) < 1e-20f)
                continue;

            for (int c = i; c < n; c++)
                A[r * n + c] -= factor * A[i * n + c];

            b[r] -= factor * b[i];
        }
    }

    for (int i = n - 1; i >= 0; i--)
    {
        float sum = b[i];
        for (int c = i + 1; c < n; c++)
            sum -= A[i * n + c] * x[c];
        x[i] = sum;
    }

    return 0;
}

/*========================================================
  2. FFT based spectrum analysis
========================================================*/
/**
 * @brief 参考两阶段 difft_extract 思路，直接输出到 DIFFT_Result
 * @param raw_adc   ADC原始采样，范围 0 ~ DIFFT_ADC_MAX
 * @param N         采样点数，必须为2的幂，且 <= DIFFT_MAX_SIZE
 * @param Fs_hz     采样频率
 * @param result    输出结果
 * @return 0 成功，-1 失败
 */
uint8_t difft_extract_result(uint16_t *raw_adc, uint32_t N, float32_t Fs_hz, DIFFT_Result *result)
{
    float32_t tmp_f;
    uint32_t idx;

    /* 1. DC */
    float32_t dc_mean = 0.0f;
    for (uint32_t i = 0; i < N; i++)
    {
        dc_mean += raw_adc[i];
    }
    dc_mean /= N;
    result->dc = dc_mean;

    /* 2. 去直流 + 加窗 */
    float32_t N_2 = N / 2.0f;
    float32_t denom = DIFFT_SIGMA * N_2;
    for (uint32_t i = 0; i < N; i++)
    {
        float32_t val = (i - N_2) / denom;
        difft_buf[i] = ((float32_t)raw_adc[i] - dc_mean) * expf(-0.5f * val * val);
    }

    /* 3. FFT + 幅值 */
    arm_rfft_fast_instance_f32 rfft_inst;
    if (arm_rfft_fast_init_f32(&rfft_inst, N) != ARM_MATH_SUCCESS)
        return -1;

    arm_rfft_fast_init_f32(&rfft_inst, N);
    arm_rfft_fast_f32(&rfft_inst, difft_buf, difft_out, 0);
    difft_buf[0] = fabsf(difft_out[0]);
    difft_buf[N / 2] = fabsf(difft_out[1]);
    arm_cmplx_mag_f32(&difft_out[2], &difft_buf[1], (N / 2) - 1);

    /* 4. 找基波 */
    arm_max_f32(&difft_buf[2], (N / 2) - 2, &tmp_f, &idx);
    idx += 2;

    if (tmp_f < 1e-3f || idx < 2)
    {
        result->f = 0.0f;
        return (uint8_t)-1;
    }

    /* 越界保护 */
    if (idx <= 1 || idx >= (N / 2 - 1))
    {
        result->f = 0.0f;
        return (uint8_t)-1;
    }

    /* 5. 高斯插值 */
    {
        float32_t ln_p = SAFE_LOG(difft_buf[idx + 1]);
        float32_t ln_m = SAFE_LOG(difft_buf[idx - 1]);
        float32_t ln_k = SAFE_LOG(difft_buf[idx]);

        tmp_f = 2.0f * ln_k - ln_p - ln_m;
        if (tmp_f < 1e-6f)
            tmp_f = 1e-6f;

        result->f = (idx + (ln_p - ln_m) / (2.0f * tmp_f)) * (Fs_hz / N);
        result->k0 = (float)idx;
    }

    /* 6. SRC 重采样 */
    {
        idx = (uint32_t)(N * (result->f) / Fs_hz);

        if (idx == 0) {
            result->f = 0.0f;
            return (uint8_t)-1;
        }

        tmp_f = (Fs_hz * idx) / (N * (result->f));

        float32_t pos = 0.0f;

        for (uint32_t i = 0; i < N; i++)
        {

            uint32_t src_idx = (uint32_t)pos;

            if (src_idx >= N - 1)
            {
                difft_buf[i] = (float32_t)raw_adc[N - 1] - dc_mean;
            }
            else
            {
                float32_t y1 = (float32_t)raw_adc[src_idx] - dc_mean;
                float32_t y2 = (float32_t)raw_adc[src_idx + 1] - dc_mean;
                difft_buf[i] = y1 + (pos - src_idx) * (y2 - y1);
            }

            pos += tmp_f;
        }
    }

    /* 7. 第二次FFT （不需要加窗）*/
    arm_rfft_fast_f32(&rfft_inst, difft_buf, difft_out, 0);

    // 用于存储基波绝对相位，作为全域相对相位的参考系
    float32_t fund_phi_abs = 0.0f;

    result->harmonics = DIFFT_MAX_HARMONICS;

    for (uint32_t h = 1; h <= DIFFT_MAX_HARMONICS; h++) {

        uint32_t bin = h * idx;

        if (bin <= N / 2)
        {

            float32_t re = (bin == N / 2) ? difft_out[1] : difft_out[2 * bin];
            float32_t im = (bin == N / 2) ? 0.0f : difft_out[2 * bin + 1];

            // 提取幅度
            arm_sqrt_f32(re * re + im * im, &tmp_f);
            float32_t raw_amp = 2.0f * tmp_f / N;

            /* sinc^2 补偿 */
            float32_t target_freq = (float32_t)h * (result->f);
            float32_t ratio = target_freq / Fs_hz;

            if (ratio > 0.001f && ratio < 0.5f) {
                float32_t pi_x = MATH_PI * ratio;
                float32_t sinc;
                if (fabsf(pi_x) < 1e-6f)
                    sinc = 1.0f;
                else
                    sinc = sinf(pi_x) / pi_x;
                raw_amp = raw_amp / (sinc * sinc);
            }
            result->Amp[h - 1] = raw_amp;

            // 提取并补偿相对相位
            float32_t current_phi_abs;
            arm_atan2_f32(im, re, &current_phi_abs);

            if (h == 1) {
                // 基波
                fund_phi_abs = current_phi_abs;
                result->phi[h - 1] = 0.0f; // 基波相对自身相移永远定义为 0
            } else {
                // 谐波 (Harmonics): 相对相位 = 绝对相位 - 次数 * 基波绝对相位
                float32_t rel_phi = current_phi_abs - (float32_t)h * fund_phi_abs;

                rel_phi -= (float32_t)(h - 1) * (MATH_PI / 2.0f);

                // 将角度限制到 [-π, π] 的标准主值区间内
                rel_phi = fmodf(rel_phi, MATH_2PI);
                if (rel_phi > MATH_PI) {
                    rel_phi -= MATH_2PI;
                } else if (rel_phi < -MATH_PI) {
                    rel_phi += MATH_2PI;
                }

                result->phi[h - 1] = rel_phi;
            }
        }
        else
        {
            // 超出奈奎斯特频率部分强制归零
            result->Amp[h - 1] = 0.0f;
            result->phi[h - 1] = 0.0f;
        }
    }

    return 0;
}


/**
 *函数功能：通过FFT计算信号的频率和幅值
 *@param x       输入归一化信号,[-1, 1]范围
 *@param N       采样点数（必须为2的幂）
 *@param Fs      采样频率
 *@param sigma   高斯窗参数
 *@result   输出结果结构体, 包含【基波频率】、【基波幅值】
 *@return ：0  成功；-1 失败（N不是2的幂）
 */
uint8_t difft_run(float *x, uint32_t N, float Fs, float sigma, DIFFT_Result *result)
{
    if (!is_power_of_two(N))
        return -1;

    float mean = 0.0f;

    for (uint32_t i = 0; i < N; i++)
        mean += x[i];

    mean /= N; // 计算输入信号的平均值

    // 去除直流分量并将结果复制到FFT输入缓冲区
    for (uint32_t i = 0; i < N; i++)
        difft_buf[i] = x[i] - mean;

    // 构建特制的高斯窗 并施加到输入，用于控制频谱泄露
    for (uint32_t i = 0; i < N; i++)
    {
        float v = (i - N * 0.5f) / (sigma * N * 0.5f);
        float w = expf(-0.5f * v * v);
        difft_buf[i] *= w;
    } // 实时计算，省内存

    arm_rfft_fast_instance_f32 fft;
    arm_rfft_fast_init_f32(&fft, N);
    arm_rfft_fast_f32(&fft, difft_buf, difft_buf, 0);

    for (uint32_t i = 0; i < N / 2; i++)
    {
        float re = difft_buf[2 * i];
        float im = difft_buf[2 * i + 1];

        difft_out[i] = sqrtf(re * re + im * im);
    }

    // 查找最大峰值作为基波
    uint32_t peak_index = 1;
    float peak_val = difft_out[1];

    for (uint32_t i = 2; i < N / 2 - 1; i++)
    {
        if (difft_out[i] > peak_val)
        {
            peak_val = difft_out[i];
            peak_index = i;
        }
    }

    float bin_size = Fs / N;

    /*使用三个相邻频点（k-1、k、k+1）进行对数插值，以提升频率估值精度*/
    float eps = 1e-12f;
    float m1 = difft_out[peak_index - 1] + eps;
    float m2 = difft_out[peak_index] + eps;
    float m3 = difft_out[peak_index + 1] + eps;

    float log_ratio = logf(m3 / m1);

    float log_term = logf((m2 * m2) / (m1 * m3));

    if (fabsf(log_term) < 1e-12f)
        log_term = (log_term >= 0.0f) ? 1e-12f : -1e-12f;

    float f =
        (peak_index + log_ratio / log_term * 0.5f) * bin_size;

    // 计算得到基波的索引和频率
    result->k0 = peak_index + log_ratio / log_term * 0.5f;
    result->f = f;

    // 在这里计算基波幅值
    float A1 = difft_measure_fundamental_amp_ls(x, N, Fs, result->f);
    result->Amp[0] = A1;

    return 0;
}

// 计算谐波幅值
uint8_t difft_run2(float *x, uint32_t N, float Fs, float sigma, DIFFT_Result *result)
{
    (void)sigma; // 本版本不使用高斯窗参数，保留接口兼容性

    if (!is_power_of_two(N))
        return -1;

    if (N > DIFFT_MAX_SIZE)
        return -1;

    if (result == NULL)
        return -1;

    // 去直流
    float mean = 0.0f;
    for (uint32_t i = 0; i < N; i++)
        mean += x[i];
    mean /= (float)N;

    for (uint32_t i = 0; i < N; i++)
        difft_buf[i] = x[i] - mean; // time domain

    // Hann窗
    for (uint32_t i = 0; i < N; i++)
    {
        float w = 0.5f * (1.0f - cosf(2.0f * MATH_PI * i / (N - 1)));
        difft_buf[i] *= w;
    } // 实时计算，省内存

    // RFFT
    arm_rfft_fast_instance_f32 fft;
    if (arm_rfft_fast_init_f32(&fft, N) != ARM_MATH_SUCCESS)
        return -1;
    arm_rfft_fast_f32(&fft, difft_buf, difft_buf, 0); // complex spectrum

    /* -------------------------------
       4) 计算单边幅度谱
       arm_rfft_fast_f32输出格式：
       bin0:      WORK_BUF1[0] = Re{X[0]}
       binN/2:    WORK_BUF1[1] = Re{X[N/2]}
       其余:
       bin k -> WORK_BUF1[2k], WORK_BUF1[2k+1]
    --------------------------------*/

    for (uint32_t k = 1; k < N / 2; k++)
    {
        float re = difft_buf[2 * k];
        float im = difft_buf[2 * k + 1];
        difft_out[k] = sqrtf(re * re + im * im); // magnitude
    }

    /* 去掉直流和极低频干扰 */
    difft_out[0] = 0.0f;
    if (N / 2 > 1)
        difft_out[1] = 0.0f;

    /* -------------------------------
       5) 查找基波峰值
    --------------------------------*/
    float global_max = 0.0f;
    for (uint32_t k = 2; k < N / 2 - 1; k++)
    {
        if (difft_out[k] > global_max)
            global_max = difft_out[k];
    }

    float threshold = global_max * 0.15f; // 可调

    uint32_t peak_index = 0;
    for (uint32_t k = 2; k < N / 2 - 1; k++)
    {
        if (difft_out[k] > threshold &&
            difft_out[k] > difft_out[k - 1] &&
            difft_out[k] >= difft_out[k + 1])
        {
            peak_index = k;
            break;
        }
    }

    if (peak_index == 0)
        return -1;

    /* -------------------------------
       6) 能量重心矫正
       kc = sum(|X[k]|^2 * k) / sum(|X[k]|^2)
    --------------------------------*/
    const int span = 5; // 对应你之前代码里的 n=5
    float bin_size = Fs / (float)N;

    int k_start = (int)peak_index - span;
    int k_end = (int)peak_index + span;

    if (k_start < 1)
        k_start = 1;
    if (k_end > (int)(N / 2 - 1))
        k_end = (int)(N / 2 - 1);

    float energy_sum = 0.0f;
    float energy_idx_sum = 0.0f;

    for (int k = k_start; k <= k_end; k++)
    {
        float e = difft_out[k] * difft_out[k];
        energy_sum += e;
        energy_idx_sum += e * (float)k;
    }

    if (energy_sum < 1e-20f)
        return -1;

    float f = result->f; // 直接用run1计算好的频率

    /* -------------------------------
       7) 计算可测谐波数
       最高只测到5次，同时不能超过奈奎斯特
    --------------------------------*/

    uint32_t n_harmonics = (uint32_t)(Fs / (2.0f * f));
    if (n_harmonics > 5)
        n_harmonics = 5;
    if (n_harmonics > DIFFT_MAX_HARMONICS)
        n_harmonics = DIFFT_MAX_HARMONICS;
    if (n_harmonics < 1)
        n_harmonics = 1;

    result->harmonics = n_harmonics;

    /* -------------------------------
       8) 基于“谐波附近频谱能量”恢复幅值
       沿用你之前那套思路：
       Amp ≈ sqrt(E * 8 / 3) / N * 2
       这里 E = sum_{k0-span}^{k0+span} |X[k]|^2
    --------------------------------*/
    for (uint32_t h = 1; h <= n_harmonics; h++)
    {
        float target_f = h * f;
        int target_bin = (int)lroundf(target_f / bin_size);

        if (target_bin < 1)
            target_bin = 1;
        if (target_bin > (int)(N / 2 - 1))
            target_bin = (int)(N / 2 - 1);

        /* 在目标谐波附近再局部找峰，避免直接整数倍落点不准 */
        int local_start = target_bin - 1;
        int local_end = target_bin + 1;

        if (local_start < 1)
            local_start = 1;
        if (local_end > (int)(N / 2 - 1))
            local_end = (int)(N / 2 - 1);

        int local_peak = local_start;
        float local_peak_val = difft_out[local_start];

        for (int k = local_start + 1; k <= local_end; k++)
        {
            if (difft_out[k] > local_peak_val)
            {
                local_peak_val = difft_out[k];
                local_peak = k;
            }
        }

        int hs = local_peak - span;
        int he = local_peak + span;

        if (hs < 1)
            hs = 1;
        if (he > (int)(N / 2 - 1))
            he = (int)(N / 2 - 1);

        float harm_energy = 0.0f;
        float harm_energy_idx_sum = 0.0f;

        for (int k = hs; k <= he; k++)
        {
            float e = difft_out[k] * difft_out[k];
            harm_energy += e;
            harm_energy_idx_sum += e * (float)k;
        }

        if (harm_energy < 1e-20f)
        {
            result->Amp[h - 1] = 0.0f;
            result->phi[h - 1] = 0.0f;
            continue;
        }

        /* 幅值恢复：与前面你的THD代码口径一致 */
        result->Amp[h - 1] = sqrtf(harm_energy * 8.0f / 3.0f) / (float)N * 2.0f;

        /* 相位：取重心中心附近bin的复数相位 */
        float harm_center = harm_energy_idx_sum / harm_energy;
        int phase_bin = (int)lroundf(harm_center);

        if (phase_bin < 1)
            phase_bin = 1;
        if (phase_bin > (int)(N / 2 - 1))
            phase_bin = (int)(N / 2 - 1);

        float re = difft_buf[2 * phase_bin]; // 这里用频谱数据
        float im = difft_buf[2 * phase_bin + 1];
        result->phi[h - 1] = atan2f(im, re);
    }

    return 0;
}

/*========================================================
  4. Harmonic processing
========================================================*/

// 谐波分析,函数计算并归一化各次谐波的相对幅度
void difft_compute_harmonic_norm(const DIFFT_Result *res, HarmonicNorm *out)
{
    float U1 = res->Amp[0];

    if (U1 <= 1e-12f)
    {
        out->base = 0.0f;
        out->h2 = 0.0f;
        out->h3 = 0.0f;
        out->h4 = 0.0f;
        out->h5 = 0.0f;
        return;
    }

    out->base = 1.0f;

    out->h2 = (res->harmonics >= 2) ? res->Amp[1] / U1 : 0.0f;
    out->h3 = (res->harmonics >= 3) ? res->Amp[2] / U1 : 0.0f;
    out->h4 = (res->harmonics >= 4) ? res->Amp[3] / U1 : 0.0f;
    out->h5 = (res->harmonics >= 5) ? res->Amp[4] / U1 : 0.0f;
}

// THD计算，利用归一化后的谐波信息计算前五次谐波的THD
float compute_thd(HarmonicNorm *h)
{
    float sum = 0.0f;

    sum += h->h2 * h->h2;
    sum += h->h3 * h->h3;
    sum += h->h4 * h->h4;
    sum += h->h5 * h->h5;

    float thd = sqrtf(sum);

    h->thd = thd;

    return thd;
}

// 波形判断函数，利用归一化后的谐波信息判断波形
uint8_t difft_detect_wave(DIFFT_Result *res)
{

    float A1 = res->Amp[0];

    if (A1 < 1e-6f)
        return 0;

    float H3 = res->harmonicNorm.h3;
    float H5 = res->harmonicNorm.h5;

    if (H3 < 0.02f)
    {
        res->wavetype = 1;
        return 1;
    }

    float R53 = H5 / (H3 + 1e-6f);

    /* triangle */
    // 理想H3=0.111,R53=0.36
    if (H3 > 0.08f && H3 < 0.14f && R53 > 0.25f && R53 < 0.47f)
    {
        res->wavetype = 2;
        return 2;
    }

    /* square */
    // 理想H3=0.333,R53=0.6
    if (H3 > 0.25f && H3 < 0.40f && R53 > 0.45f && R53 < 0.75f)
    {
        res->wavetype = 3;
        return 3;
    }

    return 0;
}

/**
 * @brief 五次谐波专项欠采样测量
 * 核心思路：
 * 1) ADC → float + 去直流
 * 2) 低通滤波
 * 3) FFT
 * 4) 五次谐波分析
 */
uint8_t difft_measure_harmonic_alias(uint16_t *adc,
                                     uint32_t N,
                                     float f0,
                                     float *Fs_out,
                                     uint8_t order,
                                     float *Amp_out)
{
    if (adc == NULL || Fs_out == NULL || Amp_out == NULL)
        return -1;

    if (N == 0 || N > DIFFT_MAX_SIZE || f0 <= 1.0f)
        return -1;

    if ((N & (N - 1)) != 0)
        return -1;

    float Fs = *Fs_out;

    /* ADC → float + 去直流 */

    float mean = 0.0f;

    for (uint32_t i = 0; i < N; i++)
    {
        WORK_BUF3[i] =
            ((float)adc[i] - DIFFT_ADC_MID) / DIFFT_ADC_MID;

        mean += WORK_BUF3[i];
    }

    mean /= (float)N;

    for (uint32_t i = 0; i < N; i++)
        WORK_BUF3[i] -= mean;

    /* Hann窗 */

    for (uint32_t i = 0; i < N; i++)
    {
        float w =
            0.5f * (1.0f - cosf(2.0f * MATH_PI * i / (N - 1)));

        WORK_BUF3[i] *= w;
    }

    /* FFT */
    arm_rfft_fast_instance_f32 fft;
    arm_rfft_fast_init_f32(&fft, N);
    arm_rfft_fast_f32(&fft, WORK_BUF3, WORK_BUF3, 0);

    /* 幅度谱 */

    for (uint32_t k = 1; k < N / 2; k++)
    {
        float re = WORK_BUF3[2 * k];
        float im = WORK_BUF3[2 * k + 1];

        difft_out[k] = sqrtf(re * re + im * im);
    }

    difft_out[0] = 0.0f;
    if (N / 2 > 1)
        difft_out[1] = 0.0f;

    /* alias 位置 */

    float fh = (float)order * f0;

    float f_alias =
        difft_fold_alias(fh, Fs);

    float bin_size = Fs / (float)N;

    int k0 =
        (int)lroundf(f_alias / bin_size);

    if (k0 < 2)
        k0 = 2;

    if (k0 > (int)(N / 2 - 2))
        k0 = (int)(N / 2 - 2);

    /* 局部峰值 */

    int peak = k0;
    float peak_val = difft_out[k0];

    for (int k = k0 - 1; k <= k0 + 1; k++)
    {
        if (difft_out[k] > peak_val)
        {
            peak_val = difft_out[k];
            peak = k;
        }
    }

    /* 能量积分 */

    const int span = 5;

    int ks = peak - span;
    int ke = peak + span;

    if (ks < 1)
        ks = 1;

    if (ke > (int)(N / 2 - 1))
        ke = (int)(N / 2 - 1);

    float energy_sum = 0.0f;

    for (int k = ks; k <= ke; k++)
    {
        float e = difft_out[k] * difft_out[k];
        energy_sum += e;
    }

    if (energy_sum < 1e-20f)
    {
        *Amp_out = 0.0f;
        return -1;
    }

    /* Hann窗幅值恢复 */

    float A =
        sqrtf(energy_sum * 8.0f / 3.0f) /
        (float)N * 2.0f;

    *Amp_out = A;

    return 0;
}

/*========================================================
  Signal Peak-to-Peak Voltage Measurement
========================================================*/
float difft_measure_vpp(float A1, uint8_t wavetype)
{
    float Vpp_norm = 0.0f;

    switch (wavetype)
    {
    case 1: // sine
        Vpp_norm = 2.0f * A1;
        break;

    case 2: // triangle
        Vpp_norm = 2.0f * A1 * (MATH_PI * MATH_PI / 8.0f);
        break;

    case 3: // square
        Vpp_norm = 2.0f * A1 * (MATH_PI / 4.0f);
        break;

    default: // unknown
        Vpp_norm = 2.0f * A1;
        break;
    }

    return Vpp_norm;
}

void difft_compute_vpp(DIFFT_Result *res)
{
    float A1 = res->Amp[0];

    float Vpp_norm = difft_measure_vpp(A1, res->wavetype);

    float vpp = Vpp_norm * 1.65f;

    res->vpp_mv = vpp * 1000.0f;
}

/*========================================================
  5. External API
========================================================*/

/**
 * @brief 通过ADC采集的数据计算信号特征
 * @param adc ADC采集数据，0--DIFFT_ADC_MAX
 */
uint8_t difft_run_adc(uint16_t *adc, uint32_t N, float Fs, float sigma, DIFFT_Result *result)
{
    static float x[DIFFT_MAX_SIZE];

    if (N > DIFFT_MAX_SIZE)
        return (uint8_t)-1;

    // 将ADC采集数据[0-65535]转换为[-1, 1]范围的浮点数
    for (uint32_t i = 0; i < N; i++)
    {
        x[i] = ((float)adc[i] - DIFFT_ADC_MID) / DIFFT_ADC_MID;
    }

    uint8_t re = difft_run(x, N, Fs, sigma, result); // 计算频率和Vpp（基波）

    difft_run2(x, N, Fs, sigma, result); // 计算谐波信息

    difft_compute_harmonic_norm(result, &result->harmonicNorm); // 计算归一化谐波信息

    difft_detect_wave(result); // 根据res里的归一化谐波信息，分辨信号波形

    difft_compute_vpp(result);

    compute_thd(&result->harmonicNorm); // 根据归一化谐波信息，计算前五次THD

    return re;
}

uint8_t difft_run_adc2(uint16_t *adc, uint32_t N, float Fs, float sigma, DIFFT_Result *result)
{
    (void)sigma; // 当前实现未使用

    if (adc == NULL || result == NULL)
        return (uint8_t)-1;

    if (N == 0 || N > DIFFT_MAX_SIZE)
        return (uint8_t)-1;

    // 使用 difft_extract_result 作为核心处理链路
    uint8_t re = difft_extract_result(adc, N, Fs, result);
    if (re != 0)
        return re;

    // 后处理：归一化谐波、波形识别、Vpp 和 THD
    difft_compute_harmonic_norm(result, &result->harmonicNorm);
    difft_detect_wave(result);
    difft_compute_vpp(result);
    compute_thd(&result->harmonicNorm);

    return 0;
}

