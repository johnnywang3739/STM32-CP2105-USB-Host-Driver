/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
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
#include "cmsis_os.h"
#include "usb_host.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "usbh_core.h"
#include "usbh_cdc.h"
#include "usbh_cp2105.h"
#include "FreeRTOS.h"
#include "task.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

UART_HandleTypeDef huart3;

osThreadId USBProcessTaskHandle;
osThreadId TransmitTaskHandle;
osThreadId ReceiveTaskHandle;
/* USER CODE BEGIN PV */

char Uart_Buf[100];
extern USBH_HandleTypeDef hUsbHostFS;
extern ApplicationTypeDef Appli_state;

#define RX_BUFFER_SIZE 32
CP2105_StateTypeDef CP2105_State = CP2105_IDLE_STATE;

static uint8_t rx_buffer[RX_BUFFER_SIZE];
static uint8_t tx_buffer[] = "Hello World\r\n";
static uint8_t transmitting = 0;
static uint8_t receiving = 0;
static uint8_t setBaudRateFlag = 1;
uint32_t new_baud_rate = 9600; // Default baud rate

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART3_UART_Init(void);
void StartUSBProcessTask(void const * argument);
void StartTransmitTask(void const * argument);
void StartReceiveTask(void const * argument);

/* USER CODE BEGIN PFP */
const char* getAppliStateName(ApplicationTypeDef state);
void print_application_state(ApplicationTypeDef state);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void UART_Printf(const char *fmt, ...) {
    char buffer[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    HAL_UART_Transmit(&huart3, (uint8_t *)buffer, strlen(buffer), HAL_MAX_DELAY);
    HAL_UART_Transmit(&huart3, (uint8_t *)"\r\n", 2, HAL_MAX_DELAY);
}



const char* getAppliStateName(ApplicationTypeDef state) {
    switch (state) {
        case APPLICATION_IDLE: return "APPLICATION_IDLE";
        case APPLICATION_START: return "APPLICATION_START";
        case APPLICATION_READY: return "APPLICATION_READY";
        case APPLICATION_DISCONNECT: return "APPLICATION_DISCONNECT";
        default: return "UNKNOWN_STATE";
    }
}
void print_application_state(ApplicationTypeDef state) {
    // Create the main application state message
    sprintf(Uart_Buf, "Application State: %s\n", getAppliStateName(state));
    int len = strlen(Uart_Buf);
    HAL_UART_Transmit(&huart3, (uint8_t *)Uart_Buf, len, 1000);

    // Add an extra new line for separation between subsequent calls
    const char newline[] = "\r\n";
    HAL_UART_Transmit(&huart3, (uint8_t *)newline, sizeof(newline) - 1, 100);
}

void process_received_data(uint8_t* data, uint32_t length) {
    data[length] = '\0';  // Null-terminate the received data

    sprintf(Uart_Buf, "Received data: %s", data);
    int len = strlen(Uart_Buf);
    HAL_UART_Transmit(&huart3, (uint8_t *)Uart_Buf, len, 1000);

    const char newline[] = "\r\n";
    HAL_UART_Transmit(&huart3, (uint8_t *)newline, sizeof(newline) - 1, 100);
}
osMutexId usbMutexHandle;

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

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
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */
  HAL_GPIO_WritePin(GPIOB, LD1_Pin, GPIO_PIN_RESET); //
  HAL_GPIO_WritePin(GPIOB, LD3_Pin, GPIO_PIN_RESET); //
  HAL_GPIO_WritePin(GPIOB, LD2_Pin, GPIO_PIN_RESET); //
  /* USER CODE END 2 */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of USBProcessTask */
  /* USB Process Task - Highest Priority */
  osThreadDef(USBProcessTask, StartUSBProcessTask, osPriorityRealtime, 0, 512);
  USBProcessTaskHandle = osThreadCreate(osThread(USBProcessTask), NULL);

  /* Receive Task - High Priority */
  osThreadDef(ReceiveTask, StartReceiveTask, osPriorityHigh, 0, 512);
  ReceiveTaskHandle = osThreadCreate(osThread(ReceiveTask), NULL);

  /* Transmit Task - Normal Priority */
  osThreadDef(TransmitTask, StartTransmitTask, osPriorityNormal, 0, 512);
  TransmitTaskHandle = osThreadCreate(osThread(TransmitTask), NULL);


  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  osMutexDef(usbMutex);
  usbMutexHandle = osMutexCreate(osMutex(usbMutex));

  /* USER CODE END RTOS_THREADS */
  HAL_GPIO_WritePin(GPIOF, Debug_LD_Main_Pin, GPIO_PIN_SET);
  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */


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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 216;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 9;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_7) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(Debug_LD_Green_GPIO_Port, Debug_LD_Green_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LD1_Pin|LD3_Pin|GPIO_PIN_5|LD2_Pin
                          |Debug_LD_Blue_Pin|Debug_LD_Red_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(Debug_LD_Main_GPIO_Port, Debug_LD_Main_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(VBUS_GPIO_Port, VBUS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : Button_Pin */
  GPIO_InitStruct.Pin = Button_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(Button_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : Debug_LD_Green_Pin */
  GPIO_InitStruct.Pin = Debug_LD_Green_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(Debug_LD_Green_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LD1_Pin LD3_Pin PB5 LD2_Pin
                           Debug_LD_Blue_Pin Debug_LD_Red_Pin */
  GPIO_InitStruct.Pin = LD1_Pin|LD3_Pin|GPIO_PIN_5|LD2_Pin
                          |Debug_LD_Blue_Pin|Debug_LD_Red_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : Debug_LD_Main_Pin */
  GPIO_InitStruct.Pin = Debug_LD_Main_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(Debug_LD_Main_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : VBUS_Pin */
  GPIO_InitStruct.Pin = VBUS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(VBUS_GPIO_Port, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartUSBProcessTask */
/**
  * @brief  Function implementing the USBProcessTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartUSBProcessTask */
void StartUSBProcessTask(void const * argument)
{
  /* init code for USB_HOST */
  MX_USB_HOST_Init();
  /* USER CODE BEGIN 5 */
  /* Infinite loop */
  for(;;)
  {
    if (osMutexWait(usbMutexHandle, 1000) == osOK) {  // Wait for 1000 ticks (1 second)
      HAL_GPIO_WritePin(GPIOB, Debug_LD_Blue_Pin, GPIO_PIN_SET);
      USBH_Process(&hUsbHostFS);

      switch (Appli_state) {
        case APPLICATION_START:
          HAL_GPIO_WritePin(GPIOB, LD1_Pin, GPIO_PIN_SET); // Turn on LD1
          HAL_GPIO_WritePin(GPIOB, LD2_Pin, GPIO_PIN_RESET);   // Ensure LD2 is off
          HAL_GPIO_WritePin(GPIOB, LD3_Pin, GPIO_PIN_RESET);   // Ensure LD3 is off
          break;
        case APPLICATION_READY:
          HAL_GPIO_WritePin(GPIOB, LD1_Pin, GPIO_PIN_RESET);   // Ensure LD1 is off
          HAL_GPIO_WritePin(GPIOB, LD2_Pin, GPIO_PIN_SET); // Turn on LD2
          HAL_GPIO_WritePin(GPIOB, LD3_Pin, GPIO_PIN_RESET);   // Ensure LD3 is off

          uint8_t port_selected = CP2105_SCI_PORT;
          CP2105_HandleTypeDef *handle = (CP2105_HandleTypeDef *)hUsbHostFS.pActiveClass->pData;
          if (handle->data_received_flag[port_selected]) {
            process_received_data(rx_buffer, RX_BUFFER_SIZE);
//            osDelay(50);
            handle->data_received_flag[port_selected] = 0; // Reset the flag
          }

          break;
        case APPLICATION_DISCONNECT:
          HAL_GPIO_WritePin(GPIOB, LD1_Pin, GPIO_PIN_RESET);   // Ensure LD1 is off
          HAL_GPIO_WritePin(GPIOB, LD2_Pin, GPIO_PIN_RESET);   // Ensure LD2 is off
          HAL_GPIO_WritePin(GPIOB, LD3_Pin, GPIO_PIN_SET); // Turn on LD3
          break;
        default:
          HAL_GPIO_WritePin(GPIOB, LD1_Pin, GPIO_PIN_RESET);
          HAL_GPIO_WritePin(GPIOB, LD2_Pin, GPIO_PIN_RESET);
          HAL_GPIO_WritePin(GPIOB, LD3_Pin, GPIO_PIN_RESET);
          break;
      }
      HAL_GPIO_WritePin(GPIOB, Debug_LD_Blue_Pin, GPIO_PIN_RESET);

      osMutexRelease(usbMutexHandle);
    }

    osDelay(1);
  }
  /* USER CODE END 5 */
}

/* USER CODE BEGIN Header_StartTransmitTask */
/**
* @brief Function implementing the TransmitTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTransmitTask */
void StartTransmitTask(void const * argument)
{
  /* USER CODE BEGIN StartTransmitTask */
  /* Infinite loop */
  for(;;)
  {
    HAL_GPIO_WritePin(GPIOB, Debug_LD_Red_Pin, GPIO_PIN_SET);
    if (Appli_state == APPLICATION_READY) {
      if (osMutexWait(usbMutexHandle, 1000) == osOK) {  // Wait for 1000 ticks (1 second)
        uint8_t port_selected = CP2105_SCI_PORT;
        USBH_StatusTypeDef result = USBH_CP2105_Transmit(&hUsbHostFS, tx_buffer, strlen((char*)tx_buffer), port_selected);
        osMutexRelease(usbMutexHandle);
      } else {
        UART_Printf("StartTransmitTask: Failed to acquire mutex within timeout");
//        osDelay(1000);
      }
    }
    HAL_GPIO_WritePin(GPIOB, Debug_LD_Red_Pin, GPIO_PIN_RESET);
    osDelay(1000);
  }
  /* USER CODE END StartTransmitTask */
}

/* USER CODE BEGIN Header_StartReceiveTask */
/**
* @brief Function implementing the ReceiveTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartReceiveTask */
void StartReceiveTask(void const * argument)
{
  /* USER CODE BEGIN StartReceiveTask */
  /* Infinite loop */
  for(;;)
  {
    HAL_GPIO_WritePin(GPIOA, Debug_LD_Green_Pin, GPIO_PIN_SET);
    if (Appli_state == APPLICATION_READY) {
      if (osMutexWait(usbMutexHandle, 1000) == osOK) {  // Wait for 1000 ticks (1 second)
        uint8_t port_selected = CP2105_SCI_PORT;
        USBH_StatusTypeDef result = USBH_CP2105_Receive(&hUsbHostFS, rx_buffer, RX_BUFFER_SIZE, port_selected);
        osMutexRelease(usbMutexHandle);
      } else {
        UART_Printf("StartReceiveTask: Failed to acquire mutex within timeout");
      }
    }
    HAL_GPIO_WritePin(GPIOA, Debug_LD_Green_Pin, GPIO_PIN_RESET);
    osDelay(1);
  }
  /* USER CODE END StartReceiveTask */
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6) {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
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


#ifdef  USE_FULL_ASSERT
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
