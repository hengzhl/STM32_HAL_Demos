## 串口接收不定长数据

### Demo介绍

`uart_rx_idle_demo`是一个串口接收不定长数据的demo，适合作为项目中与外界交互的模块。

### STM32CubeMX

`USART3`--`Asynchronous`--`DMA（USART3_RX/Normal）`--`NVIC`

### Keil

```c
#define usartRx 100         //接受缓冲区大小
uint8_t RxBuffer[usartRx];    //接收缓冲区
void ReceiveData(uint8_t *data, uint16_t size);
```

```c
HAL_UARTEx_ReceiveToIdle_DMA(&huart3, RxBuffer, usartRx); // 启动DMA接收，接收完成后触发空闲中断
__HAL_DMA_DISABLE_IT(huart3.hdmarx, DMA_IT_HT);          // 关闭DMA半传输中断
```

```c
//重写UART接收完成回调函数
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance != USART3)
        return;

    ReceiveData(RxBuffer, Size);

    HAL_UARTEx_ReceiveToIdle_DMA(huart, RxBuffer, usartRx);
    __HAL_DMA_DISABLE_IT(huart->hdmarx, DMA_IT_HT);
}

//串口发送，用于观察RxBuffer
void ReceiveData(uint8_t *data, uint16_t size)
{
    //将接受的数据原样返回，用于串口调试助手显示
    HAL_UART_Transmit(&huart3, (uint8_t *)&size, sizeof(size), HAL_MAX_DELAY);
    HAL_UART_Transmit(&huart3, data, size, HAL_MAX_DELAY);
}
```

需要注意的是，需要根据硬件配置，修改串口号，这里为`huart3`
