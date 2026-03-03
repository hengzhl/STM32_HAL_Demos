## 单片机驱动SI5351

## 说明

`stm32_si5351_demo`

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

`PG6`--`User Label(SI5351_SCL)`--`mode(Output Open Drain)`--`output level(High)`--`Pull-up`

`PG8`--`User Label(SI5351_SDA)`--`mode(Output Open Drain)`--`output level(High)`--`Pull-up`

### Keil

```c
#include "SI5351.h"
```

```c
si5351_SetOutputFreq(0, 100000, SI_CLK_SRC_PLL_A, SI_R_DIV_64); 
// 设置通道0输出100KHz，使用PLL A作为时钟源，R分频64
```

> 驱动文件在user文件夹中。
