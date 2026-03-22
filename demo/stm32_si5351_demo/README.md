## 单片机驱动SI5351模块

## Demo介绍

`stm32_si5351_demo`是关于外部时钟源SI5351模块的单片机配置说明。

在STMCubeMX中为PG6和PG8添加上标签后，在main.h文件中存在，

```c
#define SI5351_SCL_Pin GPIO_PIN_6
#define SI5351_SCL_GPIO_Port GPIOG
#define SI5351_SDA_Pin GPIO_PIN_8
#define SI5351_SDA_GPIO_Port GPIOG
```

因此，在SI5351.h中，IIC总线引脚得以成功配置，

```c
// IIC总线引脚配置
#define SCL_H HAL_GPIO_WritePin(SI5351_SCL_GPIO_Port, SI5351_SCL_Pin, GPIO_PIN_SET)
#define SCL_L HAL_GPIO_WritePin(SI5351_SCL_GPIO_Port, SI5351_SCL_Pin, GPIO_PIN_RESET)

#define SDA_H HAL_GPIO_WritePin(SI5351_SDA_GPIO_Port, SI5351_SDA_Pin, GPIO_PIN_SET)
#define SDA_L HAL_GPIO_WritePin(SI5351_SDA_GPIO_Port, SI5351_SDA_Pin, GPIO_PIN_RESET)

#define SCL_read HAL_GPIO_ReadPin(SI5351_SCL_GPIO_Port, SI5351_SCL_Pin)
#define SDA_read HAL_GPIO_ReadPin(SI5351_SDA_GPIO_Port, SI5351_SDA_Pin)
```

### STM32CubeMX

`STM32H743IIT6`

`PG6`--`User Label(SI5351_SCL)`--`mode(Output Push Pull)`--`output level(High)`--`Pull-up`

`PG8`--`User Label(SI5351_SDA)`--`mode(Output Push Pull)`--`output level(High)`--`Pull-up`

### Keil

```c
#include "SI5351.h"
```

```c
SI5351_Init();
SI5351_SetFrequency(SI_CLK0, 100000, SI_PLLA); // CLK0 输出 100KHz
```

> 驱动文件在user文件夹中。

### 结果与修正

输出的方波信号频率可能存在1Hz以内的信号偏差，请在SI5351.h中修改宏定义，实验测试修正晶体频率即可。我的模块误差需要修正`2905`

```c
#define SI_XTAL_FREQ            (25000000 + 2905)
```
