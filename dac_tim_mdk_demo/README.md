## TIM接外部时钟源驱动DAC输出

### 功能

`dac_tim_mdk_demo`

`Demo`基于`stm32_si5351_demo`，根据配置信息，调整外部时钟源的信号频率，调整波形输出函数，完成DAC输出不同波形。

### STM32CubeMX

`PA9`--`User Label(SI5351_SDA)`--`mode(Output Push Pull)`--`output level(High)`--`Pull-up`

`PA10`--`User Label(SI5351_SCL)`--`mode(Output Push Pull)`--`output level(High)`--`Pull-up`

`TIM2`--`Clock Source(ETR2)`--`PSC(0)`--`ARR(1)`--`auto-reload preload(enable)`--`Trigger Event Selection TRGO(Update Event)`

`TIM4`--`Clock Source(ETR2)`--`PSC(0)`--`ARR(1)`--`auto-reload preload(enable)`--`Trigger Event Selection TRGO(Update Event)`

`DAC1`--`OUT1`--`only external pin`--`Timer 2 Trigger Out event`--`DMA(circular/Word)`

`DAC1`--`OUT2`--`only external pin`--`Timer 4 Trigger Out event`--`DMA(circular/Word)`

### Keil

```C
#include "SI5351.h"
#include "signal.h"
```

```C
typedef struct
{
    uint16_t frequency;   // 频率值,单位Hz 
    uint16_t vpp;         // 峰峰值,单位mV
    uint16_t offset;      // 直流偏置，单位mV
    uint8_t  wave_type;   // 1-正弦波,2-三角波,3-方波
    uint16_t  duty;       // 占空比,0-100,方波用
    uint16_t wave_table[NPoints];  // 波表，NPoints定义在signal.h中
    __IO uint8_t IsRunning;    // 硬件是否工作(0-否,1-是)
    __IO uint8_t IsChanging;   // 结构体信息是否发生改变(0-否,1-是)
} gen;

gen signal = {
    .IsRunning = 0 ,
    .IsChanging = 0
};
```

```C
void signal_dac_start(gen* p );
void gen_signal_cfg(gen* p);

void gen_signal_cfg(gen* p)
{
  p->wave_type=3;
  p->frequency=10000;
  p->vpp=2000;
  p->duty=5;

  p->IsChanging=1;
}

void signal_dac_start(gen* p )
{
  if(p->IsRunning==1){
    HAL_DAC_Stop_DMA(&hdac1, DAC1_CHANNEL_2);
    HAL_TIM_Base_Stop(&htim4);
  }

  if(p->wave_type == 1){
    SineTableGen(p->vpp, p->offset, p->wave_table);
  }else if(p->wave_type == 2){
    TriangleTableGen(p->vpp, p->offset, p->wave_table);
  }else if(p->wave_type == 3){
    SquareTableGen(p->vpp, p->offset, p->duty, p->wave_table);
  }

  uint32_t CLK=2*NPoints*p->frequency;  //定时器二分频，计算Si5351外部时钟源频率

  SI5351_SetFrequency(SI_CLK0, CLK, SI_PLLA); // 设置通道0提供TIM4外部时钟源
  HAL_TIM_Base_Start(&htim4);     //开启定时器4
  HAL_DAC_Start_DMA(&hdac1, DAC1_CHANNEL_2, (uint32_t *)p->wave_table, NPoints, DAC_ALIGN_12B_R);
  p->IsRunning=1;
}
```

```c
  SI5351_Init();
  gen_signal_cfg(&signal);

  while (1)
  {
    if(signal.IsChanging==1){
      signal_dac_start(&signal);
      signal.IsChanging=0;
    }
  }
```
