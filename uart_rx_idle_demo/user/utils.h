#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief             4位小端数据帧解析函数 
 * @param buffer      接收缓冲区 (包含帧头、数据、帧尾的原始字节数组)
 * @param bufferLen   当前帧的总字节长度
 * @param header      期望的帧头字节数组指针
 * @param hLen        帧头数组的长度 (字节)
 * @param footer      期望的帧尾字节数组指针
 * @param fLen        帧尾数组的长度 (字节)
 * @param dataOut     用于存储解析出的 uint32_t 结果的数组指针
 *  @return int        返回成功解析出的数字个数；若校验失败或长度非法则返回 -1
 **/
int processDataUniversal(uint8_t *buffer, uint16_t bufferLen, 
                         uint8_t *header, uint8_t hLen, 
                         uint8_t *footer, uint8_t fLen, 
                         uint32_t *dataOut);

#endif /* UTILS_H */