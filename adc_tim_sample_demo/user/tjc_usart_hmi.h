#ifndef __TJCUSARTHMI_H_
#define __TJCUSARTHMI_H_

#include "main.h"

#define RINGBUFF_LEN (500)
#define usize		 getRingBuffLenght()
#define code_c() 	 initRingBuff()
#define udelete(x) 	 deleteRingBuff(x)
#define u(x) 		 read1BFromRingBuff(x)
#define STR_LENGTH 	 (100)

extern UART_HandleTypeDef *my_huart;
extern uint8_t RxBuff[1];

void TJCPrintf(const char *cmd, ...);
void initRingBuff(void);
void writeRingBuff(uint8_t data);
void deleteRingBuff(uint16_t size);
void uart_send_uint32t(uint32_t num);
void uart_send_char(uint8_t ch);
uint16_t getRingBuffLenght(void);
uint8_t read1BFromRingBuff(uint16_t position);

#endif
