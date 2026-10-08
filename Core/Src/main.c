/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : MultiPad "方向" board - STM32F103C8T6 USB HID keyboard
  *                   + MCP23008 matrix + PB6 addressable RGB chain
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
#include "usb_device.h"

/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "usbd_hid.h"
/* USER CODE END Includes */

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
extern USBD_HandleTypeDef hUsbDeviceFS;

#define KEY_SCAN_PERIOD_MS             2U
#define KEY_DEBOUNCE_COUNT             1U
#define MCP23008_POLL_PERIOD_MS     1000U

#define MCP23008_ADDR_FIRST          0x20U
#define MCP23008_ADDR_LAST           0x27U
#define MCP23008_COUNT                  5U
#define MCP23008_REG_IODIR           0x00U
#define MCP23008_REG_IPOL            0x01U
#define MCP23008_REG_GPINTEN         0x02U
#define MCP23008_REG_IOCON           0x05U
#define MCP23008_REG_GPPU            0x06U
#define MCP23008_REG_GPIO            0x09U
#define MCP23008_REG_OLAT            0x0AU
#define MCP_COL_OUT_MASK             0xF0U

#define SOFT_I2C_PORT                GPIOB
#define SOFT_I2C_SCL_PIN             GPIO_PIN_8
#define SOFT_I2C_SDA_PIN             GPIO_PIN_9

#define LED6028_COUNT               100U
#define LED6028_GPIO_PORT            GPIOB
#define LED6028_PIN                  GPIO_PIN_6
#define LED6028_BIT_CYCLES           90U
#define LED6028_T0H_CYCLES           29U
#define LED6028_T1H_CYCLES           58U
#define LED6028_RESET_US            100U
#define LED6028_BREATH_UPDATE_MS     20U
#define LED6028_BREATH_MIN            1U
#define LED6028_BREATH_RANGE         64U
#define LED6028_PRESSED_LEVEL        36U
#define LED6028_BG_R_MAX            128U
#define LED6028_BG_G_MAX             32U
#define LED6028_BG_B_MAX             50U
#define LED6028_BREATH_MAX          256U
#define LED6028_BREATH_PERIOD_MS   1200U
#define LED_NONE                    0xFFU

#define HID_KEY_A                    0x04U
#define HID_KEY_B                    0x05U
#define HID_KEY_C                    0x06U
#define HID_KEY_D                    0x07U
#define HID_KEY_E                    0x08U
#define HID_KEY_F                    0x09U
#define HID_KEY_G                    0x0AU
#define HID_KEY_H                    0x0BU
#define HID_KEY_I                    0x0CU
#define HID_KEY_J                    0x0DU
#define HID_KEY_K                    0x0EU
#define HID_KEY_L                    0x0FU
#define HID_KEY_M                    0x10U
#define HID_KEY_N                    0x11U
#define HID_KEY_O                    0x12U
#define HID_KEY_P                    0x13U
#define HID_KEY_Q                    0x14U
#define HID_KEY_R                    0x15U
#define HID_KEY_S                    0x16U
#define HID_KEY_T                    0x17U
#define HID_KEY_U                    0x18U
#define HID_KEY_V                    0x19U
#define HID_KEY_W                    0x1AU
#define HID_KEY_X                    0x1BU
#define HID_KEY_Y                    0x1CU
#define HID_KEY_Z                    0x1DU
#define HID_KEY_1                    0x1EU
#define HID_KEY_2                    0x1FU
#define HID_KEY_3                    0x20U
#define HID_KEY_4                    0x21U
#define HID_KEY_5                    0x22U
#define HID_KEY_6                    0x23U
#define HID_KEY_7                    0x24U
#define HID_KEY_8                    0x25U
#define HID_KEY_9                    0x26U
#define HID_KEY_0                    0x27U
#define HID_KEY_ENTER                0x28U
#define HID_KEY_BACKSPACE            0x2AU
#define HID_KEY_TAB                  0x2BU
#define HID_KEY_SPACE                0x2CU
#define HID_KEY_MINUS                0x2DU
#define HID_KEY_EQUAL                0x2EU
#define HID_KEY_LEFT_BRACKET         0x2FU
#define HID_KEY_RIGHT_BRACKET        0x30U
#define HID_KEY_BACKSLASH            0x31U
#define HID_KEY_SEMICOLON            0x33U
#define HID_KEY_APOSTROPHE           0x34U
#define HID_KEY_COMMA                0x36U
#define HID_KEY_DOT                  0x37U
#define HID_KEY_SLASH                0x38U
#define HID_KEY_CAPS_LOCK            0x39U
#define HID_KEY_INSERT               0x49U
#define HID_KEY_HOME                 0x4AU
#define HID_KEY_PAGE_UP              0x4BU
#define HID_KEY_DELETE               0x4CU
#define HID_KEY_END                  0x4DU
#define HID_KEY_PAGE_DOWN            0x4EU
#define HID_KEY_RIGHT                0x4FU
#define HID_KEY_LEFT                 0x50U
#define HID_KEY_DOWN                 0x51U
#define HID_KEY_UP                   0x52U

#define HID_MOD_LCTRL                0x01U
#define HID_MOD_LSHIFT               0x02U
#define HID_MOD_LALT                 0x04U
#define HID_MOD_LGUI                 0x08U
#define HID_MOD_RCTRL                0x10U
#define HID_MOD_RSHIFT               0x20U
#define HID_MOD_RALT                 0x40U
#define HID_MOD_RGUI                 0x80U

typedef struct
{
  uint8_t addr;
  uint8_t row;
  uint8_t col;
  uint8_t hid;
  uint8_t mod;
  uint8_t led;
} KeyMapEntry;

static const uint8_t g_mcp_addrs[MCP23008_COUNT] = {0x20U, 0x21U, 0x22U, 0x23U, 0x24U};

/* 0x20: direction board. */
static const KeyMapEntry g_keymap[] =
{
  {0x20U, 0U, 1U, HID_KEY_INSERT,       0U, 0U},
  {0x20U, 1U, 1U, HID_KEY_HOME,         0U, 1U},
  {0x20U, 2U, 1U, HID_KEY_PAGE_UP,      0U, 2U},
  {0x20U, 0U, 2U, HID_KEY_DELETE,       0U, 3U},
  {0x20U, 1U, 2U, HID_KEY_END,          0U, 4U},
  {0x20U, 2U, 2U, HID_KEY_PAGE_DOWN,    0U, 5U},
  {0x20U, 0U, 3U, HID_KEY_LEFT,         0U, 6U},
  {0x20U, 1U, 3U, HID_KEY_DOWN,         0U, 7U},
  {0x20U, 2U, 3U, HID_KEY_RIGHT,        0U, 8U},
  {0x20U, 3U, 3U, HID_KEY_UP,           0U, 9U},

  /* 0x23: 键盘87_左1. */
  {0x23U, 0U, 0U, HID_KEY_4,            0U, LED_NONE},
  {0x23U, 1U, 0U, HID_KEY_1,            0U, LED_NONE},
  {0x23U, 2U, 0U, HID_KEY_2,            0U, LED_NONE},
  {0x23U, 3U, 0U, HID_KEY_3,            0U, LED_NONE},
  {0x23U, 0U, 1U, HID_KEY_TAB,          0U, LED_NONE},
  {0x23U, 1U, 1U, HID_KEY_Q,            0U, LED_NONE},
  {0x23U, 2U, 1U, HID_KEY_W,            0U, LED_NONE},
  {0x23U, 3U, 1U, 0U,        HID_MOD_LCTRL, LED_NONE},
  {0x23U, 0U, 2U, HID_KEY_CAPS_LOCK,    0U, LED_NONE},
  {0x23U, 1U, 2U, HID_KEY_A,            0U, LED_NONE},
  {0x23U, 2U, 2U, HID_KEY_S,            0U, LED_NONE},
  {0x23U, 3U, 2U, 0U,         HID_MOD_LGUI, LED_NONE},
  {0x23U, 0U, 3U, 0U,       HID_MOD_LSHIFT, LED_NONE},
  {0x23U, 1U, 3U, HID_KEY_Z,            0U, LED_NONE},
  {0x23U, 2U, 3U, HID_KEY_X,            0U, LED_NONE},
  {0x23U, 3U, 3U, 0U,         HID_MOD_LALT, LED_NONE},

  /* 0x24: 键盘87_左2. */
  {0x24U, 0U, 0U, HID_KEY_4,            0U, LED_NONE},
  {0x24U, 1U, 0U, HID_KEY_5,            0U, LED_NONE},
  {0x24U, 2U, 0U, HID_KEY_6,            0U, LED_NONE},
  {0x24U, 3U, 0U, HID_KEY_7,            0U, LED_NONE},
  {0x24U, 0U, 1U, HID_KEY_E,            0U, LED_NONE},
  {0x24U, 1U, 1U, HID_KEY_R,            0U, LED_NONE},
  {0x24U, 2U, 1U, HID_KEY_T,            0U, LED_NONE},
  {0x24U, 3U, 1U, HID_KEY_Y,            0U, LED_NONE},
  {0x24U, 0U, 2U, HID_KEY_D,            0U, LED_NONE},
  {0x24U, 1U, 2U, HID_KEY_F,            0U, LED_NONE},
  {0x24U, 2U, 2U, HID_KEY_G,            0U, LED_NONE},
  {0x24U, 3U, 2U, HID_KEY_H,            0U, LED_NONE},
  {0x24U, 0U, 3U, HID_KEY_C,            0U, LED_NONE},
  {0x24U, 1U, 3U, HID_KEY_V,            0U, LED_NONE},
  {0x24U, 2U, 3U, HID_KEY_B,            0U, LED_NONE},
  {0x24U, 3U, 3U, HID_KEY_SPACE,        0U, LED_NONE},

  /* 0x22: 键盘87_左3. */
  {0x22U, 1U, 0U, HID_KEY_8,            0U, LED_NONE},
  {0x22U, 2U, 0U, HID_KEY_9,            0U, LED_NONE},
  {0x22U, 3U, 0U, HID_KEY_0,            0U, LED_NONE},
  {0x22U, 0U, 1U, HID_KEY_U,            0U, LED_NONE},
  {0x22U, 1U, 1U, HID_KEY_I,            0U, LED_NONE},
  {0x22U, 2U, 1U, HID_KEY_O,            0U, LED_NONE},
  {0x22U, 3U, 1U, HID_KEY_P,            0U, LED_NONE},
  {0x22U, 0U, 2U, HID_KEY_J,            0U, LED_NONE},
  {0x22U, 1U, 2U, HID_KEY_K,            0U, LED_NONE},
  {0x22U, 2U, 2U, HID_KEY_L,            0U, LED_NONE},
  {0x22U, 3U, 2U, 0U,         HID_MOD_RALT, LED_NONE},
  {0x22U, 0U, 3U, HID_KEY_N,            0U, LED_NONE},
  {0x22U, 1U, 3U, HID_KEY_M,            0U, LED_NONE},
  {0x22U, 2U, 3U, HID_KEY_COMMA,        0U, LED_NONE},
  {0x22U, 3U, 3U, HID_KEY_DOT,          0U, LED_NONE},

  /* 0x21: 键盘87_左4. Fn is scanned/logged but not sent to USB yet. */
  {0x21U, 1U, 0U, HID_KEY_MINUS,        0U, LED_NONE},
  {0x21U, 2U, 0U, HID_KEY_EQUAL,        0U, LED_NONE},
  {0x21U, 3U, 0U, HID_KEY_BACKSPACE,    0U, LED_NONE},
  {0x21U, 1U, 1U, HID_KEY_LEFT_BRACKET, 0U, LED_NONE},
  {0x21U, 2U, 1U, HID_KEY_RIGHT_BRACKET,0U, LED_NONE},
  {0x21U, 3U, 1U, HID_KEY_BACKSLASH,    0U, LED_NONE},
  {0x21U, 0U, 2U, HID_KEY_SEMICOLON,    0U, LED_NONE},
  {0x21U, 1U, 2U, HID_KEY_APOSTROPHE,   0U, LED_NONE},
  {0x21U, 2U, 2U, HID_KEY_ENTER,        0U, LED_NONE},
  {0x21U, 3U, 2U, 0U,        HID_MOD_RCTRL, LED_NONE},
  {0x21U, 0U, 3U, HID_KEY_SLASH,        0U, LED_NONE},
  {0x21U, 1U, 3U, 0U,       HID_MOD_RSHIFT, LED_NONE},
  {0x21U, 2U, 3U, 0U,                  0U, LED_NONE},
  {0x21U, 3U, 3U, 0U,         HID_MOD_RGUI, LED_NONE}
};

#define KEY_COUNT ((uint8_t)(sizeof(g_keymap) / sizeof(g_keymap[0])))

static uint8_t g_debounce[KEY_COUNT];
static uint8_t g_key_state[KEY_COUNT];
static uint8_t g_hid_report[8];
static uint8_t g_report_pending = 1U;
static uint8_t g_mcp_ready[MCP23008_COUNT];
static uint8_t g_mcp_found_count = 0U;
static uint8_t g_scan_raw_rbits[MCP23008_COUNT][4];
static uint8_t g_led6028[LED6028_COUNT][3];
/* USER CODE END PV */

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);

/* USER CODE BEGIN PFP */
static void CycleCounter_Init(void);
static void DelayUs(uint32_t us);
static void SoftI2C_Init(void);
static uint8_t SoftI2C_Probe(uint8_t addr7);
static uint8_t SoftI2C_WriteReg(uint8_t addr7, uint8_t reg, uint8_t value);
static uint8_t SoftI2C_ReadReg(uint8_t addr7, uint8_t reg, uint8_t *value);
static uint8_t MCP23008_InitAll(void);
static void MCP23008_PollAll(void);
static void Keyboard_Init(void);
static void Keyboard_Scan(void);
static void Keyboard_BuildReport(void);
static void Keyboard_USBService(void);
static void Keyboard_UpdateLEDs(uint8_t breath_level);
static void LED6028_Init(void);
static void LED6028_SetPixel(uint8_t index, uint8_t red, uint8_t green, uint8_t blue);
static void LED6028_SetAll(uint8_t red, uint8_t green, uint8_t blue);
static void LED6028_Show(void);
static void LED6028_BreathService(uint32_t now);
/* USER CODE END PFP */

/* USER CODE BEGIN 0 */
int _write(int file, char *ptr, int len)
{
  (void)file;
  if ((ptr == NULL) || (len <= 0)) return 0;
  if (HAL_UART_Transmit(&huart1, (uint8_t *)ptr, (uint16_t)len, HAL_MAX_DELAY) != HAL_OK) return -1;
  return len;
}

static void CycleCounter_Init(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void DelayUs(uint32_t us)
{
  uint32_t start = DWT->CYCCNT;
  uint32_t cycles = (SystemCoreClock / 1000000U) * us;
  while ((uint32_t)(DWT->CYCCNT - start) < cycles) {}
}

static void GPIOB_SetCRHNibble(uint8_t pin, uint32_t nibble)
{
  uint32_t shift = ((uint32_t)pin - 8U) * 4U;
  uint32_t crh = GPIOB->CRH;
  crh &= ~(0xFU << shift);
  crh |= ((nibble & 0xFU) << shift);
  GPIOB->CRH = crh;
}

static void SoftI2C_SCL_Low(void)
{
  GPIOB->BRR = SOFT_I2C_SCL_PIN;
  GPIOB_SetCRHNibble(8U, 0x6U);
}

static void SoftI2C_SCL_Release(void)
{
  GPIOB->BSRR = SOFT_I2C_SCL_PIN;
  GPIOB_SetCRHNibble(8U, 0x8U);
}

static void SoftI2C_SDA_Low(void)
{
  GPIOB->BRR = SOFT_I2C_SDA_PIN;
  GPIOB_SetCRHNibble(9U, 0x6U);
}

static void SoftI2C_SDA_Release(void)
{
  GPIOB->BSRR = SOFT_I2C_SDA_PIN;
  GPIOB_SetCRHNibble(9U, 0x8U);
}

static uint8_t SoftI2C_SDA_Read(void)
{
  return ((GPIOB->IDR & SOFT_I2C_SDA_PIN) != 0U) ? 1U : 0U;
}

static void SoftI2C_WaitSCLHigh(void)
{
  uint32_t timeout = 2000U;
  SoftI2C_SCL_Release();
  while (((GPIOB->IDR & SOFT_I2C_SCL_PIN) == 0U) && (timeout > 0U)) timeout--;
}

static void SoftI2C_Delay(void)
{
  DelayUs(2U);
}

static void SoftI2C_Start(void)
{
  SoftI2C_SDA_Release();
  SoftI2C_WaitSCLHigh();
  SoftI2C_Delay();
  SoftI2C_SDA_Low();
  SoftI2C_Delay();
  SoftI2C_SCL_Low();
  SoftI2C_Delay();
}

static void SoftI2C_Stop(void)
{
  SoftI2C_SDA_Low();
  SoftI2C_Delay();
  SoftI2C_WaitSCLHigh();
  SoftI2C_Delay();
  SoftI2C_SDA_Release();
  SoftI2C_Delay();
}

static uint8_t SoftI2C_WriteByte(uint8_t data)
{
  uint8_t ack;
  for (uint8_t bit = 0U; bit < 8U; bit++)
  {
    if ((data & 0x80U) != 0U) SoftI2C_SDA_Release(); else SoftI2C_SDA_Low();
    SoftI2C_Delay();
    SoftI2C_WaitSCLHigh();
    SoftI2C_Delay();
    SoftI2C_SCL_Low();
    SoftI2C_Delay();
    data <<= 1U;
  }

  SoftI2C_SDA_Release();
  SoftI2C_Delay();
  SoftI2C_WaitSCLHigh();
  SoftI2C_Delay();
  ack = (SoftI2C_SDA_Read() == 0U) ? 1U : 0U;
  SoftI2C_SCL_Low();
  SoftI2C_Delay();
  return ack;
}

static uint8_t SoftI2C_ReadByte(uint8_t ack)
{
  uint8_t data = 0U;
  SoftI2C_SDA_Release();
  for (uint8_t bit = 0U; bit < 8U; bit++)
  {
    data <<= 1U;
    SoftI2C_Delay();
    SoftI2C_WaitSCLHigh();
    SoftI2C_Delay();
    if (SoftI2C_SDA_Read() != 0U) data |= 1U;
    SoftI2C_SCL_Low();
    SoftI2C_Delay();
  }

  if (ack != 0U) SoftI2C_SDA_Low(); else SoftI2C_SDA_Release();
  SoftI2C_Delay();
  SoftI2C_WaitSCLHigh();
  SoftI2C_Delay();
  SoftI2C_SCL_Low();
  SoftI2C_Delay();
  SoftI2C_SDA_Release();
  return data;
}

static void SoftI2C_Init(void)
{
  __HAL_RCC_GPIOB_CLK_ENABLE();
  SoftI2C_SDA_Release();
  SoftI2C_SCL_Release();
  DelayUs(20U);
  for (uint8_t i = 0U; i < 9U; i++)
  {
    SoftI2C_SCL_Low();
    SoftI2C_Delay();
    SoftI2C_WaitSCLHigh();
    SoftI2C_Delay();
  }
  SoftI2C_Stop();
}

static uint8_t SoftI2C_Probe(uint8_t addr7)
{
  uint8_t ok;
  SoftI2C_Start();
  ok = SoftI2C_WriteByte((uint8_t)(addr7 << 1U));
  SoftI2C_Stop();
  return ok;
}

static uint8_t SoftI2C_WriteReg(uint8_t addr7, uint8_t reg, uint8_t value)
{
  uint8_t ok = 1U;
  SoftI2C_Start();
  ok &= SoftI2C_WriteByte((uint8_t)(addr7 << 1U));
  ok &= SoftI2C_WriteByte(reg);
  ok &= SoftI2C_WriteByte(value);
  SoftI2C_Stop();
  return ok;
}

static uint8_t SoftI2C_ReadReg(uint8_t addr7, uint8_t reg, uint8_t *value)
{
  uint8_t ok = 1U;
  if (value == NULL) return 0U;

  SoftI2C_Start();
  ok &= SoftI2C_WriteByte((uint8_t)(addr7 << 1U));
  ok &= SoftI2C_WriteByte(reg);
  if (ok == 0U)
  {
    SoftI2C_Stop();
    return 0U;
  }

  SoftI2C_Start();
  ok &= SoftI2C_WriteByte((uint8_t)((addr7 << 1U) | 1U));
  if (ok != 0U) *value = SoftI2C_ReadByte(0U);
  SoftI2C_Stop();
  return ok;
}

static uint8_t MCP23008_Config(uint8_t addr)
{
  uint8_t iodir = 0U, gppu = 0U;
  if (SoftI2C_Probe(addr) == 0U) return 0U;
  if (SoftI2C_WriteReg(addr, MCP23008_REG_IODIR, 0x0FU) == 0U) return 0U;
  if (SoftI2C_WriteReg(addr, MCP23008_REG_IPOL, 0x00U) == 0U) return 0U;
  if (SoftI2C_WriteReg(addr, MCP23008_REG_GPINTEN, 0x00U) == 0U) return 0U;
  if (SoftI2C_WriteReg(addr, MCP23008_REG_IOCON, 0x00U) == 0U) return 0U;
  if (SoftI2C_WriteReg(addr, MCP23008_REG_GPPU, 0x0FU) == 0U) return 0U;
  if (SoftI2C_WriteReg(addr, MCP23008_REG_OLAT, 0xF0U) == 0U) return 0U;
  if (SoftI2C_ReadReg(addr, MCP23008_REG_IODIR, &iodir) == 0U) return 0U;
  if (SoftI2C_ReadReg(addr, MCP23008_REG_GPPU, &gppu) == 0U) return 0U;
  return ((iodir == 0x0FU) && ((gppu & 0x0FU) == 0x0FU)) ? 1U : 0U;
}

static uint8_t MCP23008_InitAll(void)
{
  uint8_t count = 0U;
  for (uint8_t i = 0U; i < MCP23008_COUNT; i++)
  {
    uint8_t addr = g_mcp_addrs[i];
    g_mcp_ready[i] = MCP23008_Config(addr);
    if (g_mcp_ready[i] != 0U)
    {
      count++;
      printf("MCP 0x%02X ready\r\n", (unsigned int)addr);
    }
    else
    {
      printf("MCP 0x%02X missing/config fail\r\n", (unsigned int)addr);
    }
  }
  g_mcp_found_count = count;
  return count;
}

static void MCP23008_PollAll(void)
{
  printf("MCP:");
  for (uint8_t i = 0U; i < MCP23008_COUNT; i++)
  {
    uint8_t gpio = 0U;
    uint8_t addr = g_mcp_addrs[i];
    if ((g_mcp_ready[i] != 0U) && (SoftI2C_ReadReg(addr, MCP23008_REG_GPIO, &gpio) != 0U))
      printf(" %02X=%02X", (unsigned int)addr, (unsigned int)gpio);
    else
      printf(" %02X=ERR", (unsigned int)addr);
  }
  printf("\r\n");
}

static const char *Keyboard_KeyName(uint8_t hid, uint8_t mod)
{
  if (mod == HID_MOD_LCTRL) return "LCTRL";
  if (mod == HID_MOD_LSHIFT) return "LSHIFT";
  if (mod == HID_MOD_LALT) return "LALT";
  if (mod == HID_MOD_LGUI) return "LWIN";
  if (mod == HID_MOD_RCTRL) return "RCTRL";
  if (mod == HID_MOD_RSHIFT) return "RSHIFT";
  if (mod == HID_MOD_RALT) return "RALT";
  if (mod == HID_MOD_RGUI) return "RWIN";

  switch (hid)
  {
    case HID_KEY_A: return "A"; case HID_KEY_B: return "B"; case HID_KEY_C: return "C"; case HID_KEY_D: return "D";
    case HID_KEY_E: return "E"; case HID_KEY_F: return "F"; case HID_KEY_G: return "G"; case HID_KEY_H: return "H";
    case HID_KEY_I: return "I"; case HID_KEY_J: return "J"; case HID_KEY_K: return "K"; case HID_KEY_L: return "L";
    case HID_KEY_M: return "M"; case HID_KEY_N: return "N"; case HID_KEY_O: return "O"; case HID_KEY_P: return "P";
    case HID_KEY_Q: return "Q"; case HID_KEY_R: return "R"; case HID_KEY_S: return "S"; case HID_KEY_T: return "T";
    case HID_KEY_U: return "U"; case HID_KEY_V: return "V"; case HID_KEY_W: return "W"; case HID_KEY_X: return "X";
    case HID_KEY_Y: return "Y"; case HID_KEY_Z: return "Z";
    case HID_KEY_1: return "1"; case HID_KEY_2: return "2"; case HID_KEY_3: return "3"; case HID_KEY_4: return "4";
    case HID_KEY_5: return "5"; case HID_KEY_6: return "6"; case HID_KEY_7: return "7"; case HID_KEY_8: return "8";
    case HID_KEY_9: return "9"; case HID_KEY_0: return "0";
    case HID_KEY_ENTER: return "ENTER"; case HID_KEY_BACKSPACE: return "BACKSPACE"; case HID_KEY_TAB: return "TAB";
    case HID_KEY_SPACE: return "SPACE"; case HID_KEY_MINUS: return "-"; case HID_KEY_EQUAL: return "=";
    case HID_KEY_LEFT_BRACKET: return "["; case HID_KEY_RIGHT_BRACKET: return "]"; case HID_KEY_BACKSLASH: return "\\";
    case HID_KEY_SEMICOLON: return ";"; case HID_KEY_APOSTROPHE: return "'"; case HID_KEY_COMMA: return ",";
    case HID_KEY_DOT: return "."; case HID_KEY_SLASH: return "/"; case HID_KEY_CAPS_LOCK: return "CAPS";
    case HID_KEY_INSERT: return "INS"; case HID_KEY_HOME: return "HOME"; case HID_KEY_PAGE_UP: return "PGUP";
    case HID_KEY_DELETE: return "DEL"; case HID_KEY_END: return "END"; case HID_KEY_PAGE_DOWN: return "PGDN";
    case HID_KEY_RIGHT: return "RIGHT"; case HID_KEY_LEFT: return "LEFT"; case HID_KEY_DOWN: return "DOWN"; case HID_KEY_UP: return "UP";
    default: return "UNMAPPED";
  }
}

static void Keyboard_Init(void)
{
  memset(g_debounce, 0, sizeof(g_debounce));
  memset(g_key_state, 0, sizeof(g_key_state));
  memset(g_hid_report, 0, sizeof(g_hid_report));
  memset(g_mcp_ready, 0, sizeof(g_mcp_ready));
  memset(g_scan_raw_rbits, 0x0F, sizeof(g_scan_raw_rbits));
  g_report_pending = 1U;
}

static void Keyboard_BuildReport(void)
{
  uint8_t report_index = 2U;
  memset(g_hid_report, 0, sizeof(g_hid_report));

  for (uint8_t i = 0U; i < KEY_COUNT; i++)
  {
    if (g_key_state[i] == 0U) continue;
    g_hid_report[0] |= g_keymap[i].mod;
    if ((g_keymap[i].hid != 0U) && (report_index < sizeof(g_hid_report)))
      g_hid_report[report_index++] = g_keymap[i].hid;
  }
}

static void Keyboard_Scan(void)
{
  uint8_t raw_pressed[KEY_COUNT] = {0};
  uint8_t state_changed = 0U;

  for (uint8_t dev = 0U; dev < MCP23008_COUNT; dev++)
  {
    uint8_t addr = g_mcp_addrs[dev];
    if (g_mcp_ready[dev] == 0U) continue;

    for (uint8_t col = 0U; col < 4U; col++)
    {
      uint8_t gpio;
      uint8_t col_bit = (uint8_t)(7U - col);
      uint8_t outputs = (uint8_t)(MCP_COL_OUT_MASK & (uint8_t)~(1U << col_bit));

      if (SoftI2C_WriteReg(addr, MCP23008_REG_OLAT, outputs) == 0U)
      {
        g_mcp_ready[dev] = 0U;
        break;
      }

      DelayUs(5U);
      if (SoftI2C_ReadReg(addr, MCP23008_REG_GPIO, &gpio) == 0U)
      {
        g_mcp_ready[dev] = 0U;
        break;
      }

      g_scan_raw_rbits[dev][col] = (uint8_t)(gpio & 0x0FU);
      for (uint8_t i = 0U; i < KEY_COUNT; i++)
      {
        if ((g_keymap[i].addr == addr) && (g_keymap[i].col == col))
        {
          uint8_t row_bit = (uint8_t)(3U - g_keymap[i].row);
          raw_pressed[i] = ((gpio & (uint8_t)(1U << row_bit)) == 0U) ? 1U : 0U;
        }
      }
    }

    (void)SoftI2C_WriteReg(addr, MCP23008_REG_OLAT, MCP_COL_OUT_MASK);
  }

  for (uint8_t i = 0U; i < KEY_COUNT; i++)
  {
    if (raw_pressed[i] != 0U)
    {
      if (g_debounce[i] < KEY_DEBOUNCE_COUNT) g_debounce[i]++;
    }
    else
    {
      if (g_debounce[i] > 0U) g_debounce[i]--;
    }

    if ((g_debounce[i] >= KEY_DEBOUNCE_COUNT) && (g_key_state[i] == 0U))
    {
      g_key_state[i] = 1U;
      state_changed = 1U;
      printf("KEY DOWN: key=%s addr=0x%02X R%u C%u hid=0x%02X mod=0x%02X\r\n",
             Keyboard_KeyName(g_keymap[i].hid, g_keymap[i].mod),
             (unsigned int)g_keymap[i].addr, (unsigned int)g_keymap[i].row,
             (unsigned int)g_keymap[i].col, (unsigned int)g_keymap[i].hid,
             (unsigned int)g_keymap[i].mod);
    }
    else if ((g_debounce[i] == 0U) && (g_key_state[i] != 0U))
    {
      g_key_state[i] = 0U;
      state_changed = 1U;
      printf("KEY UP  : key=%s addr=0x%02X R%u C%u hid=0x%02X mod=0x%02X\r\n",
             Keyboard_KeyName(g_keymap[i].hid, g_keymap[i].mod),
             (unsigned int)g_keymap[i].addr, (unsigned int)g_keymap[i].row,
             (unsigned int)g_keymap[i].col, (unsigned int)g_keymap[i].hid,
             (unsigned int)g_keymap[i].mod);
    }
  }

  if (state_changed != 0U)
  {
    Keyboard_BuildReport();
    g_report_pending = 1U;
  }
}

static void Keyboard_USBService(void)
{
  if (g_report_pending == 0U) return;
  if (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED) return;
  if (USBD_HID_SendReport(&hUsbDeviceFS, g_hid_report, sizeof(g_hid_report)) == USBD_OK) g_report_pending = 0U;
}

static __attribute__((always_inline)) inline void LED6028_DIN_High(void)
{
  LED6028_GPIO_PORT->BRR = LED6028_PIN;
}

static __attribute__((always_inline)) inline void LED6028_DIN_Low(void)
{
  LED6028_GPIO_PORT->BSRR = LED6028_PIN;
}

static void LED6028_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  memset(g_led6028, 0, sizeof(g_led6028));
  __HAL_RCC_GPIOB_CLK_ENABLE();
  LED6028_DIN_Low();
  GPIO_InitStruct.Pin = LED6028_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(LED6028_GPIO_PORT, &GPIO_InitStruct);
  LED6028_DIN_Low();
  DelayUs(LED6028_RESET_US);
}

static void LED6028_SetPixel(uint8_t index, uint8_t red, uint8_t green, uint8_t blue)
{
  if (index >= LED6028_COUNT) return;
  g_led6028[index][0] = green;
  g_led6028[index][1] = red;
  g_led6028[index][2] = blue;
}

static void LED6028_SetAll(uint8_t red, uint8_t green, uint8_t blue)
{
  for (uint8_t i = 0U; i < LED6028_COUNT; i++) LED6028_SetPixel(i, red, green, blue);
}

static __attribute__((always_inline)) inline void LED6028_SendBit(uint8_t one)
{
  uint32_t start;
  uint32_t high_cycles = (one != 0U) ? LED6028_T1H_CYCLES : LED6028_T0H_CYCLES;
  start = DWT->CYCCNT;
  LED6028_GPIO_PORT->BRR = LED6028_PIN;
  while ((uint32_t)(DWT->CYCCNT - start) < high_cycles) {}
  LED6028_GPIO_PORT->BSRR = LED6028_PIN;
  while ((uint32_t)(DWT->CYCCNT - start) < LED6028_BIT_CYCLES) {}
}

static __attribute__((always_inline)) inline void LED6028_SendByte(uint8_t value)
{
  for (uint8_t mask = 0x80U; mask != 0U; mask >>= 1U) LED6028_SendBit((value & mask) != 0U ? 1U : 0U);
}

static void LED6028_Show(void)
{
  uint32_t primask;
  LED6028_DIN_Low();
  DelayUs(LED6028_RESET_US);
  primask = __get_PRIMASK();
  __disable_irq();
  for (uint8_t led = 0U; led < LED6028_COUNT; led++)
    for (uint8_t color = 0U; color < 3U; color++) LED6028_SendByte(g_led6028[led][color]);
  LED6028_DIN_Low();
  if (primask == 0U) __enable_irq();
  DelayUs(LED6028_RESET_US);
}

static void Keyboard_UpdateLEDs(uint8_t breath_level)
{
  uint8_t r = (uint8_t)(((uint16_t)LED6028_BG_R_MAX * breath_level) / LED6028_BREATH_MAX);
  uint8_t g = (uint8_t)(((uint16_t)LED6028_BG_G_MAX * breath_level) / LED6028_BREATH_MAX);
  uint8_t b = (uint8_t)(((uint16_t)LED6028_BG_B_MAX * breath_level) / LED6028_BREATH_MAX);

  LED6028_SetAll(r, g, b);
  for (uint8_t i = 0U; i < KEY_COUNT; i++)
  {
    if ((g_key_state[i] != 0U) && (g_keymap[i].led != LED_NONE))
      LED6028_SetPixel(g_keymap[i].led, LED6028_PRESSED_LEVEL, LED6028_PRESSED_LEVEL, LED6028_PRESSED_LEVEL);
  }
  LED6028_Show();
}

static void LED6028_BreathService(uint32_t ms)
{
  static uint32_t last_update = 0U;
  float phase, breath;
  uint32_t level;

  if ((uint32_t)(ms - last_update) < LED6028_BREATH_UPDATE_MS) return;
  last_update = ms;
  phase = (float)(ms % LED6028_BREATH_PERIOD_MS) / (float)LED6028_BREATH_PERIOD_MS;
  breath = 0.5f - 0.5f * cosf(2.0f * 3.1415926f * phase);
  level = (uint32_t)((float)LED6028_BREATH_RANGE * breath);
  Keyboard_UpdateLEDs((uint8_t)(LED6028_BREATH_MIN + level));
}
/* USER CODE END 0 */

int main(void)
{
  uint32_t last_scan_tick = 0U;
  uint32_t last_log_tick = 0U;
  uint32_t last_mcp_retry_tick = 0U;
  uint32_t last_mcp_poll_tick = 0U;
  int frame = 0;

  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_USB_DEVICE_Init();

  CycleCounter_Init();
  SoftI2C_Init();
  Keyboard_Init();
  LED6028_Init();
  HAL_Delay(100U);

  (void)MCP23008_InitAll();
  MCP23008_PollAll();

  printf("\r\n========================================\r\n");
  printf("MultiPad 5-board keyboard start\r\n");
  printf("MCU              : STM32F103C8T6\r\n");
  printf("SystemCoreClock  : %lu Hz\r\n", (unsigned long)SystemCoreClock);
  printf("USB              : PA11 D- / PA12 D+\r\n");
  printf("MCP23008         : 0x20 direction, 0x23 left1, 0x24 left2, 0x22 left3, 0x21 left4\r\n");
  printf("MCP ready        : %u/%u\r\n", (unsigned int)g_mcp_found_count, (unsigned int)MCP23008_COUNT);
  printf("Keys             : %u mapped keys\r\n", (unsigned int)KEY_COUNT);
  printf("RGB              : PB6 GPIO bit-bang, %u LEDs\r\n", (unsigned int)LED6028_COUNT);
  printf("========================================\r\n");

  while (1)
  {
    uint32_t now = HAL_GetTick();
    frame++;

    if ((uint32_t)(now - last_scan_tick) >= KEY_SCAN_PERIOD_MS)
    {
      last_scan_tick = now;
      Keyboard_Scan();
    }

    Keyboard_USBService();
    LED6028_BreathService(now);

    if ((uint32_t)(now - last_mcp_poll_tick) >= MCP23008_POLL_PERIOD_MS)
    {
      last_mcp_poll_tick = now;
      MCP23008_PollAll();
    }

    if ((g_mcp_found_count < MCP23008_COUNT) && ((uint32_t)(now - last_mcp_retry_tick) >= 1000U))
    {
      last_mcp_retry_tick = now;
      (void)MCP23008_InitAll();
    }

    if ((uint32_t)(now - last_log_tick) >= 1000U)
    {
      last_log_tick = now;
      printf("[%lu ms] USB=%s MCP=%u/%u",
             (unsigned long)now,
             (hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED) ? "configured" : "waiting",
             (unsigned int)g_mcp_found_count, (unsigned int)MCP23008_COUNT);

      for (uint8_t dev = 0U; dev < MCP23008_COUNT; dev++)
      {
        printf(" %02X=%X,%X,%X,%X",
               (unsigned int)g_mcp_addrs[dev],
               (unsigned int)g_scan_raw_rbits[dev][0], (unsigned int)g_scan_raw_rbits[dev][1],
               (unsigned int)g_scan_raw_rbits[dev][2], (unsigned int)g_scan_raw_rbits[dev][3]);
      }
      printf(" frame=%d\r\n", frame);
    }
  }
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) Error_Handler();

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLL_DIV1_5;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) Error_Handler();
}

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
  if (HAL_UART_Init(&huart1) != HAL_OK) Error_Handler();
}

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOB, LED6028_PIN, GPIO_PIN_SET);
  GPIO_InitStruct.Pin = LED6028_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  HAL_GPIO_WritePin(GPIOB, SOFT_I2C_SCL_PIN | SOFT_I2C_SDA_PIN, GPIO_PIN_SET);
  GPIO_InitStruct.Pin = SOFT_I2C_SCL_PIN | SOFT_I2C_SDA_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

void Error_Handler(void)
{
  __disable_irq();
  while (1) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  printf("Assert failed: file=%s line=%lu\r\n", file, (unsigned long)line);
}
#endif
