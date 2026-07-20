/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : USB HID numpad + matrix keyboard + USART1 printf
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "usbd_hid.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define MATRIX_ROW_COUNT          5U
#define MATRIX_COL_COUNT          4U
#define MATRIX_SCAN_PERIOD_MS     1U
#define MATRIX_DEBOUNCE_COUNT     5U

/* 标准 USB HID Keyboard/Keypad Usage ID。 */
#define HID_KEY_NUM_LOCK          0x53U
#define HID_KEY_KP_DIVIDE         0x54U
#define HID_KEY_KP_MULTIPLY       0x55U
#define HID_KEY_KP_MINUS          0x56U
#define HID_KEY_KP_PLUS           0x57U
#define HID_KEY_KP_ENTER          0x58U
#define HID_KEY_KP_1              0x59U
#define HID_KEY_KP_2              0x5AU
#define HID_KEY_KP_3              0x5BU
#define HID_KEY_KP_4              0x5CU
#define HID_KEY_KP_5              0x5DU
#define HID_KEY_KP_6              0x5EU
#define HID_KEY_KP_7              0x5FU
#define HID_KEY_KP_8              0x60U
#define HID_KEY_KP_9              0x61U
#define HID_KEY_KP_0              0x62U
#define HID_KEY_KP_DOT            0x63U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */

/*
 * 原理图连接：
 *
 * ROW1 -> PA1
 * ROW2 -> PA2
 * ROW3 -> PA3
 * ROW4 -> PA4
 * ROW5 -> PA5
 *
 * COL1 -> PB15
 * COL2 -> PB14
 * COL3 -> PB13
 * COL4 -> PB12
 */
static const uint16_t g_row_pins[MATRIX_ROW_COUNT] =
{
  GPIO_PIN_1,
  GPIO_PIN_2,
  GPIO_PIN_3,
  GPIO_PIN_4,
  GPIO_PIN_5
};

static const uint16_t g_col_pins[MATRIX_COL_COUNT] =
{
  GPIO_PIN_15,
  GPIO_PIN_14,
  GPIO_PIN_13,
  GPIO_PIN_12
};

/*
 * 标准 17 键数字小键盘布局。
 *
 * 第 3 行第 4 列、第 4 行第 4 列、第 5 行第 2 列没有独立开关，
 * 因为 +、Enter、0 通常是大键帽，占据多个格子，但只有一个轴。
 *
 *   Num    /      *      -
 *   7      8      9      +
 *   4      5      6      空
 *   1      2      3      空
 *   0      空     .      Enter
 */
static const uint8_t g_keymap[MATRIX_ROW_COUNT][MATRIX_COL_COUNT] =
{
  {HID_KEY_NUM_LOCK, HID_KEY_KP_DIVIDE, HID_KEY_KP_MULTIPLY, HID_KEY_KP_MINUS},
  {HID_KEY_KP_7,     HID_KEY_KP_8,      HID_KEY_KP_9,        HID_KEY_KP_PLUS},
  {HID_KEY_KP_4,     HID_KEY_KP_5,      HID_KEY_KP_6,        0x00U},
  {HID_KEY_KP_1,     HID_KEY_KP_2,      HID_KEY_KP_3,        0x00U},
  {HID_KEY_KP_0,     0x00U,             HID_KEY_KP_DOT,      HID_KEY_KP_ENTER}
};

/* 每个按键的积分式消抖计数器。 */
static uint8_t g_debounce[MATRIX_ROW_COUNT][MATRIX_COL_COUNT];

/* 经过消抖后的稳定状态：0=释放，1=按下。 */
static uint8_t g_key_state[MATRIX_ROW_COUNT][MATRIX_COL_COUNT];

/* 标准 Boot Keyboard 8 字节输入报告。 */
static uint8_t g_hid_report[8];

/* 有新状态需要发送时置 1。 */
static uint8_t g_report_pending = 1U;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);

/* USER CODE BEGIN PFP */
static void Keyboard_MatrixInit(void);
static void Keyboard_MatrixScan(void);
static void Keyboard_BuildReport(void);
static void Keyboard_USBService(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

extern USBD_HandleTypeDef hUsbDeviceFS;

/**
  * @brief 将 printf() 重定向到 USART1。
  */
int _write(int file, char *ptr, int len)
{
  (void)file;

  if ((ptr == NULL) || (len <= 0))
  {
    return 0;
  }

  if (HAL_UART_Transmit(&huart1,
                        (uint8_t *)ptr,
                        (uint16_t)len,
                        HAL_MAX_DELAY) != HAL_OK)
  {
    return -1;
  }

  return len;
}

/**
  * @brief 将所有行恢复为高阻态。
  *
  * ROW 配置为开漏输出：
  *   SET   -> MOS 关闭，相当于高阻
  *   RESET -> 输出低电平，选中当前行
  */
static void Keyboard_ReleaseAllRows(void)
{
  HAL_GPIO_WritePin(GPIOA,
                    GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 |
                    GPIO_PIN_4 | GPIO_PIN_5,
                    GPIO_PIN_SET);
}

/**
  * @brief 初始化矩阵键盘状态。
  */
static void Keyboard_MatrixInit(void)
{
  memset(g_debounce, 0, sizeof(g_debounce));
  memset(g_key_state, 0, sizeof(g_key_state));
  memset(g_hid_report, 0, sizeof(g_hid_report));

  Keyboard_ReleaseAllRows();
  g_report_pending = 1U;
}

/**
  * @brief 根据稳定按键状态生成标准 8 字节 Boot Keyboard 报告。
  */
static void Keyboard_BuildReport(void)
{
  uint8_t report_index = 2U;
  uint8_t row;
  uint8_t col;

  memset(g_hid_report, 0, sizeof(g_hid_report));

  for (row = 0U; row < MATRIX_ROW_COUNT; row++)
  {
    for (col = 0U; col < MATRIX_COL_COUNT; col++)
    {
      uint8_t keycode = g_keymap[row][col];

      if ((g_key_state[row][col] != 0U) &&
          (keycode != 0x00U))
      {
        if (report_index < sizeof(g_hid_report))
        {
          g_hid_report[report_index] = keycode;
          report_index++;
        }
      }
    }
  }
}

/**
  * @brief 扫描完整的 5x4 键盘矩阵，并进行约 5 ms 消抖。
  *
  * 二极管方向：
  *   COL(PB 输入上拉) -> 按键 -> 二极管 -> ROW(PA 开漏拉低)
  */
static void Keyboard_MatrixScan(void)
{
  uint8_t state_changed = 0U;
  uint8_t row;
  uint8_t col;

  Keyboard_ReleaseAllRows();

  for (row = 0U; row < MATRIX_ROW_COUNT; row++)
  {
    /*
     * 当前行拉低，其他行保持开漏高阻。
     * 按键按下时，对应 COL 输入会被拉低。
     */
    HAL_GPIO_WritePin(GPIOA, g_row_pins[row], GPIO_PIN_RESET);

    /*
     * 等待 GPIO 电平稳定。
     * 72 MHz 下这些 NOP 足够覆盖短走线和输入建立时间。
     */
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();

    for (col = 0U; col < MATRIX_COL_COUNT; col++)
    {
      uint8_t pressed;

      pressed = (HAL_GPIO_ReadPin(GPIOB, g_col_pins[col]) == GPIO_PIN_RESET)
              ? 1U
              : 0U;

      if (pressed != 0U)
      {
        if (g_debounce[row][col] < MATRIX_DEBOUNCE_COUNT)
        {
          g_debounce[row][col]++;
        }
      }
      else
      {
        if (g_debounce[row][col] > 0U)
        {
          g_debounce[row][col]--;
        }
      }

      if ((g_debounce[row][col] >= MATRIX_DEBOUNCE_COUNT) &&
          (g_key_state[row][col] == 0U))
      {
        g_key_state[row][col] = 1U;
        state_changed = 1U;

        printf("KEY DOWN: row=%u col=%u hid=0x%02X",
               (unsigned int)(row + 1U),
               (unsigned int)(col + 1U),
               (unsigned int)g_keymap[row][col]);
      }
      else if ((g_debounce[row][col] == 0U) &&
               (g_key_state[row][col] != 0U))
      {
        g_key_state[row][col] = 0U;
        state_changed = 1U;

        printf("KEY UP  : row=%u col=%u hid=0x%02X",
               (unsigned int)(row + 1U),
               (unsigned int)(col + 1U),
               (unsigned int)g_keymap[row][col]);
      }
    }

    /* 当前行扫描完成，恢复为高阻态。 */
    HAL_GPIO_WritePin(GPIOA, g_row_pins[row], GPIO_PIN_SET);
  }

  if (state_changed != 0U)
  {
    Keyboard_BuildReport();
    g_report_pending = 1U;
  }
}

/**
  * @brief 当 USB 已枚举完成时发送最新键盘报告。
  */
static void Keyboard_USBService(void)
{
  if (g_report_pending == 0U)
  {
    return;
  }

  if (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED)
  {
    return;
  }

  if (USBD_HID_SendReport(&hUsbDeviceFS,
                          g_hid_report,
                          sizeof(g_hid_report)) == USBD_OK)
  {
    g_report_pending = 0U;
  }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
  uint32_t last_scan_tick = 0U;
  uint32_t last_log_tick = 0U;
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_USB_DEVICE_Init();

  /* USER CODE BEGIN 2 */

  Keyboard_MatrixInit();

  printf("\r\n");
  printf("========================================\r\n");
  printf("STM32F103 USB numpad start\r\n");
  printf("SystemCoreClock : %lu Hz\r\n",
         (unsigned long)SystemCoreClock);
  printf("USART1          : 115200 8N1\r\n");
  printf("Matrix rows     : PA1 PA2 PA3 PA4 PA5, open-drain output\r\n");
  printf("Matrix columns  : PB15 PB14 PB13 PB12, input pull-up\r\n");
  printf("USB report      : Boot Keyboard, 8 bytes\r\n");
  printf("========================================\r\n");

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    uint32_t now = HAL_GetTick();

    if ((uint32_t)(now - last_scan_tick) >= MATRIX_SCAN_PERIOD_MS)
    {
      last_scan_tick = now;
      Keyboard_MatrixScan();
    }

    Keyboard_USBService();

    if ((uint32_t)(now - last_log_tick) >= 1000U)
    {
      last_log_tick = now;

      printf("[%lu ms] USB=%s\r\n",
             (unsigned long)now,
             (hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED)
               ? "configured"
               : "waiting");
    }

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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /*
   * HSE    = 8 MHz
   * PLL    = 8 MHz x 9 = 72 MHz
   * SYSCLK = 72 MHz
   * USBCLK = 72 MHz / 1.5 = 48 MHz
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;

  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK |
                                RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 |
                                RCC_CLOCKTYPE_PCLK2;

  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct,
                          FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLL_DIV1_5;

  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;

  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*
   * ROW1~ROW5：PA1~PA5，开漏输出。
   * 默认写 1，表示开漏高阻态。
   */
  HAL_GPIO_WritePin(GPIOA,
                    GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 |
                    GPIO_PIN_4 | GPIO_PIN_5,
                    GPIO_PIN_SET);

  GPIO_InitStruct.Pin = GPIO_PIN_1 |
                        GPIO_PIN_2 |
                        GPIO_PIN_3 |
                        GPIO_PIN_4 |
                        GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*
   * COL1~COL4：PB15~PB12，输入上拉。
   * 未按下时读取 1，按键连接到当前拉低 ROW 时读取 0。
   */
  GPIO_InitStruct.Pin = GPIO_PIN_12 |
                        GPIO_PIN_13 |
                        GPIO_PIN_14 |
                        GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  __disable_irq();

  while (1)
  {
  }
}

#ifdef USE_FULL_ASSERT

void assert_failed(uint8_t *file, uint32_t line)
{
  printf("Assert failed: file=%s line=%lu\r\n",
         file,
         (unsigned long)line);
}

#endif /* USE_FULL_ASSERT */