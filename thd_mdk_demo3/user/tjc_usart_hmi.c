/**
使用注意事项:
	1.将tjc_usart_hmi.c和tjc_usart_hmi.h 分别导入工程
	2.在需要使用的函数所在的头文件中添加 #include "tjc_usart_hmi.h"
	3.使用前请将 HAL_UART_Transmit() 这个函数改为你的单片机的串口发送单字节函数
	3.TJCPrintf和printf用法一样

*/

#include "tjc_usart_hmi.h"
#include "usart.h" 

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>

UART_HandleTypeDef *my_huart = &huart1; // 唯一要改动的地方

typedef struct
{
	uint16_t Head;
	uint16_t Tail;
	uint16_t Lenght;
	uint8_t Ring_data[RINGBUFF_LEN];
} RingBuff_t;

RingBuff_t ringBuff; // 创建一个ringBuff的缓冲区
uint8_t RxBuff[1];

/********************************************************
函数名：  	uart_send_char
日期：    	2024.09.18
功能：    	串口发送单个字符
输入参数：		要发送的单个字符
返回值： 		无
修改记录：
**********************************************************/

void uart_send_char(uint8_t ch)
{

	uint8_t ch2 = (uint8_t)ch;
	// 发送单个字符
	HAL_UART_Transmit(my_huart, &ch2, 1, 0xffff); // 这个函数改为你的单片机的串口发送单字节函数
}

void uart_send_uint32t(uint32_t num)
{
	uint32_t num2 = (uint32_t)num;
	uint8_t ch;
	for (int i = 0; i < 4; i++)
	{
		ch = (uint8_t)(num2 >> ((3 - i) * 8));
		// 发送单个字符
		HAL_UART_Transmit(my_huart, &ch, 1, 0xffff); // 这个函数改为你的单片机的串口发送单字节函数
	}

	// 当串口0忙的时候等待，不忙的时候再发送传进来的字符
	// while(__HAL_UART_GET_FLAG(&TJC_UART, UART_FLAG_TXE) == RESET);	//等待发送完毕
	return;
}

/********************************************************
函数名：  	TJCPrintf
作者：    	wwd
日期：    	2022.10.08
功能：    	向串口打印数据,使用前请将USART_SCREEN_write这个函数改为你的单片机的串口发送单字节函数
输入参数：		参考printf
返回值： 		打印到串口的数量
修改记录：
**********************************************************/

// 临时修改，增加串口选择接口，比如(&huart1,&huart2)之类的
void TJCPrintf(UART_HandleTypeDef *huart, const char *str, ...)
{

	uint8_t end = 0xff;
	char buffer[STR_LENGTH + 1]; // 数据长度
	uint8_t i = 0;

	va_list arg_ptr;
	va_start(arg_ptr, str);
	vsnprintf(buffer, STR_LENGTH + 1, str, arg_ptr);
	va_end(arg_ptr);

	while ((i < STR_LENGTH) && (i < strlen(buffer)))
	{
		HAL_UART_Transmit(huart, (uint8_t *)(buffer) + i++, 1, 0xffff);
	}
	// 发送结束符
	HAL_UART_Transmit(huart, &end, 1, 0xffff);
	HAL_UART_Transmit(huart, &end, 1, 0xffff);
	HAL_UART_Transmit(huart, &end, 1, 0xffff);
}

/********************************************************
函数名：  	HAL_UART_RxCpltCallback
作者：
日期：    	2022.10.08
功能：    	串口接收中断,将接收到的数据写入环形缓冲区
输入参数：
返回值： 		void
修改记录：
*********************************************************
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	// 判断是由哪个串口触发的中断
	if (huart->Instance == my_huart->Instance) 
	{
		writeRingBuff(RxBuff[0]);
		HAL_UART_Receive_IT(my_huart, RxBuff, 1); 
	}
}
*/
/********************************************************
函数名：  	initRingBuff
作者：
日期：    	2022.10.08
功能：    	初始化环形缓冲区
输入参数：
返回值： 		void
修改记录：
**********************************************************/
void initRingBuff(void)
{
	// 初始化相关信息
	ringBuff.Head = 0;
	ringBuff.Tail = 0;
	ringBuff.Lenght = 0;
}

/********************************************************
函数名：  	writeRingBuff
作者：
日期：    	2022.10.08
功能：    	往环形缓冲区写入数据
输入参数：
返回值： 		void
修改记录：
**********************************************************/
void writeRingBuff(uint8_t data)
{
	if (ringBuff.Lenght >= RINGBUFF_LEN) // 判断缓冲区是否已满
	{
		return;
	}
	ringBuff.Ring_data[ringBuff.Tail] = data;
	ringBuff.Tail = (ringBuff.Tail + 1) % RINGBUFF_LEN; // 防止越界非法访问
	ringBuff.Lenght++;
}

/********************************************************
函数名：  	deleteRingBuff
作者：
日期：    	2022.10.08
功能：    	删除串口缓冲区中相应长度的数据
输入参数：		要删除的长度
返回值： 		void
修改记录：
**********************************************************/
void deleteRingBuff(uint16_t size)
{
	if (size >= ringBuff.Lenght)
	{
		initRingBuff();
		return;
	}
	for (int i = 0; i < size; i++)
	{

		if (ringBuff.Lenght == 0) // 判断非空
		{
			initRingBuff();
			return;
		}
		ringBuff.Head = (ringBuff.Head + 1) % RINGBUFF_LEN; // 防止越界非法访问
		ringBuff.Lenght--;
	}
}

/********************************************************
函数名：  	read1BFromRingBuff
作者：
日期：    	2022.10.08
功能：    	从串口缓冲区读取1字节数据
输入参数：		position:读取的位置
返回值： 		所在位置的数据(1字节)
修改记录：
**********************************************************/
uint8_t read1BFromRingBuff(uint16_t position)
{
	uint16_t realPosition = (ringBuff.Head + position) % RINGBUFF_LEN;

	return ringBuff.Ring_data[realPosition];
}

/********************************************************
函数名：  	getRingBuffLenght
作者：
日期：    	2022.10.08
功能：    	获取串口缓冲区的数据数量
输入参数：
返回值： 		串口缓冲区的数据数量
修改记录：
**********************************************************/
uint16_t getRingBuffLenght()
{
	return ringBuff.Lenght;
}

/********************************************************
函数名：  	isRingBuffOverflow
作者：
日期：    	2022.10.08
功能：    	判断环形缓冲区是否已满
输入参数：
返回值： 		1:环形缓冲区已满 , 2:环形缓冲区未满
修改记录：
**********************************************************/
uint8_t isRingBuffOverflow()
{
	return ringBuff.Lenght == RINGBUFF_LEN;
}
