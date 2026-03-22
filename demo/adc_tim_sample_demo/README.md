## TIM驱动ADC采样并串口发送

### Demo介绍

`adc_tim_sample_demo`

定时器产生驱动事件，控制ADC采样电信号，处理信号，将处理后的信号以稳定方式发送到串口屏。

![波形](https://raw.githubusercontent.com/hengzhl/img-bed/main/img/QQ20260303-185133.jpg)

数据处理 ADC 缓冲区，提取一个周期，缩放至 0-200 并重采样为固定点数发送。

### STM32CubeMX

`APB1 Timer Clocks (240MHz)`

`TIM3`--`Internal Clock`--`PSC(23)`--`ARR(199)`--`auto-reload preload(enable)`--`Trigger Event Selection TRGO(Update Event)`

`ADC1`--`IN14 Single-ended（PA2）`--`DMA(normal)`--`Conversion Data Management Mode(DMA One Shot Mode)`--`External Trigger Conversion Source(Timer 3 Trigger event)`

`USART3`--`Asynchronous`

### Keil

```c
#include "AdcProc.h"
#include "tjc_usart_hmi.h"
#include <stdlib.h>
#include <stdio.h>

#define sampleLEN 2048
#define showPoints 90  // 设定发送点数，即每周期点数

__IO uint8_t AdcConvEnd = 0;
uint16_t ADCBuffer[sampleLEN];

void toShow(uint16_t* ADCBuffer, uint16_t LEN);
```

```c
  while (1)
  {
    AdcConvEnd = 0;//刷新DMA结束标志位

    HAL_TIM_Base_Start(&htim3);     //开启定时器3

    //开启ADC采集，结束后触发终端回调函数HAL_ADC_ConvCpltCallback
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *)ADCBuffer, sampleLEN); 

    while (!AdcConvEnd)     //等待转换完毕
         ;

    toShow(ADCBuffer, sampleLEN); //处理ADC数据并发送

  }
```

```c
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1) //检查触发中断的ADC身份
    {
        AdcConvEnd = 1; //设置DMA结束标志位
        HAL_TIM_Base_Stop(&htim3); //关闭定时器
    }
}

/**
 * @brief 处理 ADC 缓冲区，提取一个周期，缩放至 0-200 并重采样为固定点数发送
 */
void toShow(uint16_t* ADCBuffer, uint16_t LEN) {
    uint16_t cycleLen = 0;

    // 1. 提取一个周期的采样点
    // 注意：此函数内部 malloc 分配内存，必须手动 free
    uint16_t* oneCycleData = find_one_cycle(ADCBuffer, LEN, &cycleLen); 

    if (oneCycleData == NULL) {
        //printf("未找到完整周期\n");
        return;
    }

    // 2. 将电压值等比例缩放到 0 - 200 范围
    // 这样做可以确保无论原始采样精度如何，最终输出都在 0-200 之间
    scale_array(oneCycleData, cycleLen, 0, 200);

    // 3. 自适应重采样：无论原始周期有多少点，统一变换为 200 点输出
    // 480个点实现五个周期，平均每个周期96点，重采样为90点可以平滑显示
    // 因此宏定义 showPoints 设为90  

    uint16_t sendBuffer[showPoints]; // 最终发送缓冲区，大小为 showPoints

    resample_u16(oneCycleData, cycleLen, sendBuffer,  showPoints);

    // 4. 发送数据 
    for (int i = 0; i < showPoints; i++) {
        TJCPrintf("add s0.id,0,%d", sendBuffer[i]);
        HAL_Delay(10); // 适当延时，确保数据发送稳定
    }

    // 5. 必须释放 find_one_cycle 申请的堆空间
    free(oneCycleData);
}
```
