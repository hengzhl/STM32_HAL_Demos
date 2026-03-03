## 单片机驱动SI5351

`stm32_si5351_demo`

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
