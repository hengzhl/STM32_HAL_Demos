## 串口接收不定长数据

`uart_rx_idle_demo`

### STM32CubeMX

USART3--Asynchronous--DMA（Normal）--NVIC

### Keil

```c
#define usartRx100         //接受缓冲区大小
void ProcessData(uint8_t *data, uint16_t size);
uint8_t RxBuffer[usartRx];    //接收缓冲区
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
