#include <stdint.h>
#include <stdbool.h>
#include <string.h> 

/**
 * @brief 数据帧解析函数(小端)--processDataUniversal
 * @param buffer      接收缓冲区 (RxBuffer)
 * @param bufferLen   这一帧数据的实际总长度
 * @param header      期望的帧头数组 (如 {0x02, 0x23})
 * @param hLen        帧头数组的长度
 * @param footer      期望的帧尾数组 (如 {0xFF, 0xFF, 0xFF})
 * @param fLen        帧尾数组的长度
 * @param dataOut     存储十进制结果的数组
 * @return int        返回解析出的数字个数，校验失败返回 -1
 */
int processDataUniversal(uint8_t *buffer, uint16_t bufferLen, 
                         uint8_t *header, uint8_t hLen, 
                         uint8_t *footer, uint8_t fLen, 
                         uint32_t *dataOut) {
    
    // 1. 基本安全检查：总长度必须大于头长+尾长
    if (bufferLen < (uint16_t)(hLen + fLen)) {
        return -1; 
    }

    // 2. 校验帧头：对比 buffer 开头的内容
    if (memcmp(buffer, header, hLen) != 0) {
        return -1;
    }

    // 3. 校验帧尾：对比 buffer 末尾的内容
    // buffer + bufferLen - fLen 指向缓冲区中帧尾应该开始的位置
    if (memcmp(buffer + bufferLen - fLen, footer, fLen) != 0) {
        return -1;
    }

    // 4. 解析有效载荷 (Payload)
    int payloadBytes = bufferLen - hLen - fLen;
    
    // 确保剩余字节是 4 的整数倍（uint32_t）
    if (payloadBytes <= 0 || (payloadBytes % 4) != 0) {
        return -1;
    }

    int dataCount = payloadBytes / 4;

    for (int i = 0; i < dataCount; i++) {
        // 起始位置 = 传入的头长度 + (当前第几个数 * 4字节)
        int offset = hLen + (i * 4);

        // 小端拼接逻辑
        dataOut[i] = (uint32_t)buffer[offset] |
                     ((uint32_t)buffer[offset + 1] << 8) |
                     ((uint32_t)buffer[offset + 2] << 16) |
                     ((uint32_t)buffer[offset + 3] << 24);
    }

    return dataCount;
}