/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32l4xx_hal.h"

#include "stm32l4xx_ll_iwdg.h"
#include "stm32l4xx_ll_lpuart.h"
#include "stm32l4xx_ll_rcc.h"
#include "stm32l4xx_ll_system.h"
#include "stm32l4xx_ll_gpio.h"
#include "stm32l4xx_ll_exti.h"
#include "stm32l4xx_ll_bus.h"
#include "stm32l4xx_ll_cortex.h"
#include "stm32l4xx_ll_utils.h"
#include "stm32l4xx_ll_pwr.h"
#include "stm32l4xx_ll_dma.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */
extern char print_buf[];
/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LED_Pin LL_GPIO_PIN_4
#define LED_GPIO_Port GPIOB
#define BOOT_Pin LL_GPIO_PIN_9
#define BOOT_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */
#define _RAM1 __attribute__((section(".RAM1")))
#define _RAM2 __attribute__((section(".RAM2")))

#if 1
#define uart_printf(fmt, args...)                       \
  do {                                                  \
    sprintf((char *)print_buf, fmt, ##args);            \
    for (uint16_t i = 0; i < strlen(print_buf); ++i) {  \
      while (LL_LPUART_IsActiveFlag_TC(LPUART1) != 1) { \
        ;                                               \
      }                                                 \
      LL_LPUART_TransmitData8(LPUART1, print_buf[i]);   \
    }                                                   \
    while (LL_LPUART_IsActiveFlag_TC(LPUART1) != 1) {   \
      ;                                                 \
    }                                                   \
  } while (0)
#else
#define uart_printf(fmt, args...)
#endif
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
