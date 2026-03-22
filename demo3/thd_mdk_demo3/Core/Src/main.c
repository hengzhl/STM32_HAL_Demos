/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <stdlib.h>
#include "SI5351.h"
#include "difft.h"
#include "tjc_usart_hmi.h"
#include "AdcProc.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
DIFFT_Result difftResult; // difft结果，结构体定义在difft.h
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define usartRx 100    // 串口接受缓冲区大小
#define sampleLEN 2048 // 采样点数
#define showPoints 120 // 设定发送点数，通过平均降采样实现平滑
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
__IO uint8_t AdcConvEnd = 0; // ADC转换完成标志
__IO uint8_t difftEnd = -1;  // 0：difft完成，-1：difft失败
__IO uint8_t SystemFlag = 0; // 系统工作标志位，0-空转，1-工作

uint8_t RxBuffer[usartRx];     // 接收缓冲区
uint16_t ADCBuffer[sampleLEN]; // ADC数据缓冲区

float Fs_h5 = 0.0f;      // 五次谐波专用采样率
float A5_special = 0.0f; // 五次谐波修正幅值

uint32_t SampleRate = 1300000; // 初始采样率

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
/* USER CODE BEGIN PFP */
void SetSampleRate(uint32_t sampleRate);
void toShow(uint16_t *ADCBuffer, uint16_t LEN);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_TIM4_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  // 串口不定长数据接收
  HAL_UARTEx_ReceiveToIdle_DMA(&huart1, RxBuffer, usartRx); // 启动DMA接收，接收完成后触发空闲中断
  __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);           // 关闭DMA半传输中断

  // 定时器外部时钟源配置
  SI5351_Init();             // SI5351初始化
  SetSampleRate(SampleRate); // 设置采样率,单位Hz
  HAL_Delay(10);             // 等待SI5351锁定,不加也行

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    if (SystemFlag == 1)
    {
      AdcConvEnd = 0;             // 刷新DMA结束标志位
      HAL_TIM_Base_Start(&htim4); // 开启定时器4

      // 开启ADC采集，结束后触发终端回调函数HAL_ADC_ConvCpltCallback
      HAL_ADC_Start_DMA(&hadc1, (uint32_t *)ADCBuffer, sampleLEN);

      while (!AdcConvEnd) // 等待转换完毕
        ;

      difftEnd = difft_run_adc(ADCBuffer, sampleLEN, SampleRate, 0.4f, &difftResult);

      /*低频下，采样率策略*/
      if (difftResult.f < 5000)
      {
        SampleRate = 100000;
        SetSampleRate(SampleRate);
      }
      else if (difftResult.f > 6000)
      {
        SampleRate = 1300000;
        SetSampleRate(SampleRate); // 高频信号调整采样率
      }

			// 向串口屏和蓝牙发送波形
			toShow(ADCBuffer, sampleLEN);

      /*高频下，采样率策略*/
      if (difftResult.f > 20000)
      {
        float Fs_h5 = roundf(4.2f * difftResult.f);

        if (Fs_h5 < 200000)
          Fs_h5 = 200000;
        if (Fs_h5 > 1000000)
          Fs_h5 = 1000000;

        SetSampleRate(Fs_h5);

        AdcConvEnd = 0;
        HAL_TIM_Base_Start(&htim4);
        HAL_ADC_Start_DMA(&hadc1, (uint32_t *)ADCBuffer, sampleLEN);
        while (!AdcConvEnd)
          ;

        float A5;
        if (difft_measure_harmonic_alias(ADCBuffer, sampleLEN, difftResult.f, &Fs_h5, 5, &A5) == 0)
        {
          difftResult.Amp[4] = A5;
          difftResult.harmonicNorm.h5 = A5 / difftResult.Amp[0];
          compute_thd(&difftResult.harmonicNorm);
        }
        SampleRate = 1300000;
        SetSampleRate(SampleRate);
      }

      /*发送串口屏 */
      TJCPrintf(&huart1, "b4.txt=\"%d Hz\"", (uint16_t)difftResult.f); // 信号基频
      TJCPrintf(&huart1, "b5.txt=\"%d mV\"", (uint16_t)(difftResult.vpp_mv*1.19)); // 基波峰峰值

      TJCPrintf(&huart1, "t7.txt=\"%.4f\"", 1);
      TJCPrintf(&huart1, "t8.txt=\"%.4f\"", difftResult.harmonicNorm.h2);
      TJCPrintf(&huart1, "t9.txt=\"%.4f\"", difftResult.harmonicNorm.h3);
      TJCPrintf(&huart1, "t10.txt=\"%.4f\"", difftResult.harmonicNorm.h4);
      TJCPrintf(&huart1, "t11.txt=\"%.4f\"", difftResult.harmonicNorm.h5);
      TJCPrintf(&huart1, "t12.txt=\"%.4f\"", difftResult.harmonicNorm.thd);

      if (difftResult.wavetype == 0)
      {
        TJCPrintf(&huart1, "b6.txt=\"周期波\"");
      }
      else if (difftResult.wavetype == 1)
      {
        TJCPrintf(&huart1, "b6.txt=\"正弦波\"");
      }
      else if (difftResult.wavetype == 2)
      {
        TJCPrintf(&huart1, "b6.txt=\"三角波\"");
      }
      else if (difftResult.wavetype == 3)
      {
        TJCPrintf(&huart1, "b6.txt=\"方波\"");
      }

      /*发送蓝牙 */
      TJCPrintf(&huart2, "t0.txt=\"%d\"", (uint16_t)difftResult.f);      // 信号基频,hz
      TJCPrintf(&huart2, "t1.txt=\"%d\"", (uint16_t)(difftResult.vpp_mv*0.595)); // 基波幅值,mv

      TJCPrintf(&huart2, "t2.txt=\"%.2f\"", difftResult.harmonicNorm.h2*100);// 单位%
      TJCPrintf(&huart2, "t3.txt=\"%.2f\"", difftResult.harmonicNorm.h3*100);
      TJCPrintf(&huart2, "t4.txt=\"%.2f\"", difftResult.harmonicNorm.h4*100);
      TJCPrintf(&huart2, "t5.txt=\"%.2f\"", difftResult.harmonicNorm.h5*100);
      TJCPrintf(&huart2, "t6.txt=\"%.2f\"", difftResult.harmonicNorm.thd*100);
    }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Macro to configure the PLL clock source
  */
  __HAL_RCC_PLL_PLLSOURCE_CONFIG(RCC_PLLSOURCE_HSI);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = 64;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

// 重写UART接收完成回调函数
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
  if (huart->Instance != USART1)
    return;

  if (RxBuffer[0] == 0x01 && RxBuffer[1] == 0x11)
  {
    SystemFlag = 1;
  }
  else if (RxBuffer[0] == 0x01 && RxBuffer[1] == 0x12)
  {
    SystemFlag = 0;
  }

  HAL_UARTEx_ReceiveToIdle_DMA(huart, RxBuffer, usartRx);
  __HAL_DMA_DISABLE_IT(huart->hdmarx, DMA_IT_HT);
}

// ADC转换完成回调
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
  if (hadc->Instance == ADC1) // 检查触发中断的ADC身份
  {
    AdcConvEnd = 1;            // 设置DMA结束标志位
    HAL_TIM_Base_Stop(&htim4); // 关闭定时器
  }
}

void toShow(uint16_t *ADCBuffer, uint16_t LEN)
{
  uint16_t cycleLen = 0;

  // 1. 提取一个周期的采样点
  // 注意：此函数内部 malloc 分配内存，必须手动 free
  uint16_t *oneCycleData = find_one_cycle(ADCBuffer, LEN, &cycleLen);

  if (oneCycleData == NULL)
  {
    // printf("未找到完整周期\n");
    return;
  }

  // 2. 将电压值等比例缩放到 0 - 200 范围
  // 这样做可以确保无论原始采样精度如何，最终输出都在 0-170 之间
  scale_array(oneCycleData, cycleLen, 0, 170);

  // 3. 自适应重采样：无论原始周期有多少点，统一变换为 200 点输出


  uint16_t sendBuffer[showPoints]; // 最终发送缓冲区，大小为 showPoints

  resample_u16(oneCycleData, cycleLen, sendBuffer, showPoints);

  // 4. 发送数据
  for (int i = 0; i < showPoints; i++)
  {
    TJCPrintf(&huart1,"add s0.id,0,%d", sendBuffer[i]);
    TJCPrintf(&huart2,"add s0.id,0,%d", sendBuffer[i]);
    HAL_Delay(5); // 适当延时，确保数据发送稳定
  }

  // 5. 必须释放 find_one_cycle 申请的堆空间
  free(oneCycleData);
}

void SetSampleRate(uint32_t sampleRate)
{
  // 因为定时器TIM4重装载值为1，因此2*sampleRate的外部时钟源产生sampleRate的采样率
  SI5351_SetFrequency(SI_CLK0, 2 * sampleRate, SI_PLLA);
}

// 串口重定向，USART1
int fputc(int ch, FILE *f)
{
  uint8_t temp = (uint8_t)ch;

  // 处理换行符 \n -> \r\n
  if (ch == '\n')
  {
    HAL_UART_Transmit(&huart1, (uint8_t *)"\r", 1, 100);
  }

  HAL_UART_Transmit(&huart1, &temp, 1, 100);

  return ch;
}
/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
