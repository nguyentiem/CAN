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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "log.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define CAN_REQUEST_ID  0x200U
#define CAN_RESPONSE_ID 0x201U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
CAN_HandleTypeDef hcan;

/* USER CODE BEGIN PV */
static uint32_t last_recovery_ms;
static uint8_t bus_off_reported;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_CAN_Init(void);
/* USER CODE BEGIN PFP */
static void CAN_ConfigureAndStart(void);
static void CAN_Service(void);
static void CAN_LogFrame(const char *direction, uint32_t id, uint32_t dlc,
                         const uint8_t *data);

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

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  log_init();
  log_println("[BOOT] STM32 CAN demo");

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_CAN_Init();
  /* USER CODE BEGIN 2 */
  CAN_ConfigureAndStart();
  log_println("[CAN] started 125 kbit/s, RX=0x200 TX=0x201");

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    CAN_Service();
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

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief CAN Initialization Function
  * @param None
  * @retval None
  */
static void MX_CAN_Init(void)
{

  /* USER CODE BEGIN CAN_Init 0 */

  /* USER CODE END CAN_Init 0 */

  /* USER CODE BEGIN CAN_Init 1 */

  /* USER CODE END CAN_Init 1 */
  hcan.Instance = CAN1;
  /* 8 MHz APB1 / (4 * (1 + 13 + 2)) = 125 kbit/s. */
  hcan.Init.Prescaler = 4;
  hcan.Init.Mode = CAN_MODE_NORMAL;
  hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan.Init.TimeSeg1 = CAN_BS1_13TQ;
  hcan.Init.TimeSeg2 = CAN_BS2_2TQ;
  hcan.Init.TimeTriggeredMode = DISABLE;
  hcan.Init.AutoBusOff = ENABLE;
  hcan.Init.AutoWakeUp = DISABLE;
  hcan.Init.AutoRetransmission = ENABLE;
  hcan.Init.ReceiveFifoLocked = DISABLE;
  hcan.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN_Init 2 */

  /* USER CODE END CAN_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
static void CAN_ConfigureAndStart(void)
{
  CAN_FilterTypeDef filter = {0};

  /* 32-bit mask filter: standard data frame with ID 0x200 only. */
  filter.FilterBank = 0;
  filter.FilterMode = CAN_FILTERMODE_IDMASK;
  filter.FilterScale = CAN_FILTERSCALE_32BIT;
  filter.FilterIdHigh = (uint16_t)(CAN_REQUEST_ID << 5);
  filter.FilterIdLow = 0;
  filter.FilterMaskIdHigh = (uint16_t)(0x7FFU << 5);
  filter.FilterMaskIdLow = 0x0006U; /* match IDE and RTR */
  filter.FilterFIFOAssignment = CAN_FILTER_FIFO0;
  filter.FilterActivation = ENABLE;
  if (HAL_CAN_ConfigFilter(&hcan, &filter) != HAL_OK)
  {
    log_print("[CAN] filter failed, HAL error=");
    log_print_u32_hex(HAL_CAN_GetError(&hcan));
    log_println("");
    Error_Handler();
  }
  log_print("[CAN] before start: PB8=");
  log_print_u32_dec(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_8));
  log_print(" MSR=");
  log_print_u32_hex(CAN1->MSR);
  log_print(" CAN_REMAP=");
  log_print_u32_hex(AFIO->MAPR & AFIO_MAPR_CAN_REMAP);
  log_print(" BTR=");
  log_print_u32_hex(CAN1->BTR);
  log_println("");
  if (HAL_CAN_Start(&hcan) != HAL_OK)
  {
    log_print("[CAN] start failed, HAL error=");
    log_print_u32_hex(HAL_CAN_GetError(&hcan));
    log_print(" PB8=");
    log_print_u32_dec(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_8));
    log_print(" MCR=");
    log_print_u32_hex(CAN1->MCR);
    log_print(" MSR=");
    log_print_u32_hex(CAN1->MSR);
    log_print(" ESR=");
    log_print_u32_hex(CAN1->ESR);
    log_println("");
    Error_Handler();
  }
}

static void CAN_Service(void)
{
  CAN_RxHeaderTypeDef rx;
  CAN_TxHeaderTypeDef tx = {0};
  uint8_t request[8] = {0};
  uint8_t response[8];
  uint32_t mailbox;
  uint32_t now = HAL_GetTick();
  uint32_t i;
  uint32_t number_length;

  /* HAL error flags are sticky; ESR reports the current bus-off state. */
  if ((hcan.Instance->ESR & CAN_ESR_BOFF) != 0U)
  {
    if (bus_off_reported == 0U)
    {
      log_print("[CAN] bus off, HAL error=");
      log_print_u32_hex(HAL_CAN_GetError(&hcan));
      log_println("");
      bus_off_reported = 1U;
    }
    if ((now - last_recovery_ms) >= 1000U)
    {
      last_recovery_ms = now;
      if (HAL_CAN_Stop(&hcan) == HAL_OK)
      {
        if (HAL_CAN_Start(&hcan) == HAL_OK)
        {
          log_println("[CAN] restart requested");
        }
        else
        {
          log_println("[CAN] restart failed");
        }
      }
    }
    return;
  }
  if (bus_off_reported != 0U)
  {
    log_println("[CAN] bus active again");
    bus_off_reported = 0U;
  }

  if (HAL_CAN_GetRxFifoFillLevel(&hcan, CAN_RX_FIFO0) == 0U)
  {
    return;
  }
  if (HAL_CAN_GetRxMessage(&hcan, CAN_RX_FIFO0, &rx, request) != HAL_OK)
  {
    log_print("[CAN] RX failed, HAL error=");
    log_print_u32_hex(HAL_CAN_GetError(&hcan));
    log_println("");
    return;
  }
  CAN_LogFrame("RX", rx.IDE == CAN_ID_STD ? rx.StdId : rx.ExtId,
               rx.DLC, request);
  if (rx.IDE != CAN_ID_STD || rx.RTR != CAN_RTR_DATA ||
      rx.StdId != CAN_REQUEST_ID || rx.DLC > 8U)
  {
    log_println("[CAN] ignored unexpected frame");
    return;
  }
  if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan) == 0U)
  {
    log_println("[CAN] TX mailboxes full; response dropped");
    return;
  }

  if (rx.DLC < 7U || request[0] != 'h' || request[1] != 'e' ||
      request[2] != 'l' || request[3] != 'l' || request[4] != 'o' ||
      request[5] != ' ')
  {
    log_println("[CAN] ignored invalid request");
    return;
  }
  number_length = rx.DLC - 6U;
  if (number_length > 2U)
  {
    log_println("[CAN] ignored invalid number");
    return;
  }
  for (i = 6U; i < rx.DLC; ++i)
  {
    if (request[i] < '0' || request[i] > '9')
    {
      log_println("[CAN] ignored invalid number");
      return;
    }
  }
  response[0] = 'h';
  response[1] = 'i';
  response[2] = ' ';
  for (i = 0U; i < number_length; ++i)
  {
    response[3U + i] = request[6U + i];
  }
  tx.StdId = CAN_RESPONSE_ID;
  tx.IDE = CAN_ID_STD;
  tx.RTR = CAN_RTR_DATA;
  tx.DLC = 3U + number_length;
  if (HAL_CAN_AddTxMessage(&hcan, &tx, response, &mailbox) == HAL_OK)
  {
    CAN_LogFrame("TX queued", tx.StdId, tx.DLC, response);
  }
  else
  {
    log_print("[CAN] TX failed, HAL error=");
    log_print_u32_hex(HAL_CAN_GetError(&hcan));
    log_println("");
  }
}

static void CAN_LogFrame(const char *direction, uint32_t id, uint32_t dlc,
                         const uint8_t *data)
{
  uint32_t i;

  log_print("[CAN] ");
  log_print(direction);
  log_print(" id=");
  log_print_u32_hex(id);
  log_print(" dlc=");
  log_print_u32_dec(dlc);
  log_print(" text=\"");
  for (i = 0U; i < dlc && i < 8U; ++i)
  {
    char character[2] = {(char)data[i], '\0'};
    log_print(character);
  }
  log_print("\"");
  log_println("");
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  log_print("[ERROR] fatal, CAN HAL error=");
  log_print_u32_hex(HAL_CAN_GetError(&hcan));
  log_println("");
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
