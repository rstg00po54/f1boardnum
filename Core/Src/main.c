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
#include <stddef.h>
#include "usbd_hid.h"
/* USER CODE END Includes */

UART_HandleTypeDef huart1;
I2C_HandleTypeDef hi2c1;

/* USER CODE BEGIN PV */
extern USBD_HandleTypeDef hUsbDeviceFS;

#define KEY_SCAN_PERIOD_MS             2U
#define METRICS_SAMPLE_PERIOD_MS      1000U
/* I2C1 remapped to PB8=SCL, PB9=SDA (STM32F103). 400kHz Fast-mode.
 * If long cables cause NACKs, try 100000U before changing any PCB wiring. */
#define HW_I2C_CLOCK_HZ             400000U
#define HW_I2C_TIMEOUT_MS                3U
#define KEY_DEBOUNCE_COUNT             1U
/* Hold physical Fn (0x21 R2/C3) + Backspace (0x21 R3/C0)
 * for three seconds to reboot. The combo cannot be triggered by USB.
 */
#define KEY_REBOOT_HOLD_MS           3000U
#define KEY_FN_MCP_ADDR              0x21U
#define KEY_FN_ROW                   2U
#define KEY_FN_COL                   3U
#define KEY_BACKSPACE_MCP_ADDR       0x21U
#define KEY_BACKSPACE_ROW            3U
#define KEY_BACKSPACE_COL            0U
#define MCP23008_POLL_PERIOD_MS     1000U

#define MCP23008_ADDR_FIRST          0x20U
#define MCP23008_ADDR_LAST           0x27U
#define MCP23008_COUNT                  6U
#define MCP23008_REG_IODIR           0x00U
#define MCP23008_REG_IPOL            0x01U
#define MCP23008_REG_GPINTEN         0x02U
#define MCP23008_REG_IOCON           0x05U
#define MCP23008_REG_GPPU            0x06U
#define MCP23008_REG_GPIO            0x09U
#define MCP23008_REG_OLAT            0x0AU
#define MCP_COL_OUT_MASK             0xF0U

#define HW_I2C_SCL_PIN               GPIO_PIN_8
#define HW_I2C_SDA_PIN               GPIO_PIN_9

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
#define HID_KEY_GRAVE                0x35U
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

/* USB HID keyboard/keypad page: dedicated numeric keypad usages. */
#define HID_KEY_KP_NUM_LOCK          0x53U
#define HID_KEY_KP_DIVIDE            0x54U
#define HID_KEY_KP_MULTIPLY          0x55U
#define HID_KEY_KP_MINUS             0x56U
#define HID_KEY_KP_PLUS              0x57U
#define HID_KEY_KP_ENTER             0x58U
#define HID_KEY_KP_1                 0x59U
#define HID_KEY_KP_2                 0x5AU
#define HID_KEY_KP_3                 0x5BU
#define HID_KEY_KP_4                 0x5CU
#define HID_KEY_KP_5                 0x5DU
#define HID_KEY_KP_6                 0x5EU
#define HID_KEY_KP_7                 0x5FU
#define HID_KEY_KP_8                 0x60U
#define HID_KEY_KP_9                 0x61U
#define HID_KEY_KP_0                 0x62U
#define HID_KEY_KP_DOT               0x63U

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

static const uint8_t g_mcp_addrs[MCP23008_COUNT] = {0x20U, 0x21U, 0x22U, 0x23U, 0x24U, 0x27U};

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
  {0x23U, 0U, 0U, HID_KEY_GRAVE,        0U, LED_NONE},
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
  {0x21U, 3U, 3U, 0U,         HID_MOD_RGUI, LED_NONE},

  /* Numpad 0: the user-approved 2026-11-03 physical wiring is on
   * the direction board (MCP 0x20), NOT MCP 0x21:
   * U3 KEY1 -> PgDn/U88 K1 (direction-board column C2)
   * U3 KEY2 -> Up/U93   K2 (direction-board row R3).
   * R3/C2 is unused by the direction-board keys. Respect diode
   * orientation; this key is not physically connected until wired.
   * U3 RGB is pixel #87 in the existing serial LED chain.
   */
  {0x20U, 3U, 2U, HID_KEY_KP_0,        0U, 87U},

  /* 0x27: 键盘87_小键盘. Verified from EasyEDA Pro PCB PAD_NET.
   * MCP GP7..GP4 = C0..C3; GP3..GP0 = R0..R3.
   * The extra U3 switch connects to isolated KEY1/KEY2 nets and has
   * no MCP 0x27 matrix connection. Keypad 0 is mapped to the unused
   * direction-board 0x20 R3/C2 slot above, as previously agreed.
   */
  {0x27U, 0U, 0U, HID_KEY_KP_NUM_LOCK, 0U, LED_NONE}, /* U1  Num */
  {0x27U, 0U, 1U, HID_KEY_KP_7,        0U, LED_NONE}, /* U2  7 */
  {0x27U, 0U, 2U, HID_KEY_KP_4,        0U, LED_NONE}, /* U56 4 */
  {0x27U, 0U, 3U, HID_KEY_KP_1,        0U, LED_NONE}, /* U62 1 */

  {0x27U, 1U, 0U, HID_KEY_KP_DIVIDE,   0U, LED_NONE}, /* U44 / */
  {0x27U, 1U, 1U, HID_KEY_KP_8,        0U, LED_NONE}, /* U51 8 */
  {0x27U, 1U, 2U, HID_KEY_KP_5,        0U, LED_NONE}, /* U57 5 */
  {0x27U, 1U, 3U, HID_KEY_KP_2,        0U, LED_NONE}, /* U63 2 */

  {0x27U, 2U, 0U, HID_KEY_KP_MULTIPLY, 0U, LED_NONE}, /* U71 * */
  {0x27U, 2U, 1U, HID_KEY_KP_9,        0U, LED_NONE}, /* U73 9 */
  {0x27U, 2U, 2U, HID_KEY_KP_6,        0U, LED_NONE}, /* U75 6 */
  {0x27U, 2U, 3U, HID_KEY_KP_3,        0U, LED_NONE}, /* U66 3 */

  {0x27U, 3U, 0U, HID_KEY_KP_MINUS,    0U, LED_NONE}, /* U72 - */
  {0x27U, 3U, 1U, HID_KEY_KP_PLUS,     0U, LED_NONE}, /* U74 + */
  {0x27U, 3U, 2U, HID_KEY_KP_ENTER,    0U, LED_NONE}, /* U68 Enter */
  {0x27U, 3U, 3U, HID_KEY_KP_DOT,      0U, LED_NONE}  /* U67 . */
};

#define KEY_COUNT ((uint8_t)(sizeof(g_keymap) / sizeof(g_keymap[0])))

static uint8_t g_debounce[KEY_COUNT];
static uint8_t g_key_state[KEY_COUNT];
static uint8_t g_hid_report[8];
static uint8_t g_report_pending = 1U;
static uint8_t g_mcp_ready[MCP23008_COUNT];
static uint8_t g_mcp_found_count = 0U;
static uint8_t g_scan_raw_rbits[MCP23008_COUNT][4];
/* Previous raw row bits for every column, including currently unmapped keys. */
static uint8_t g_matrix_prev_rbits[MCP23008_COUNT][4];
static uint8_t g_led6028[LED6028_COUNT][3];

/* RGB physical coordinates: LEDs 0..70 main + 71..87 numpad.
 * Both use the same X half-key unit coordinate; row Y is 0..4.
 * Numpad X=38..44, main X=0..34. Keep the original 100 LED output size:
 * LED88..99 remain spatially unmapped until their wiring is confirmed.
 */
#define LED6028_SPATIAL_COUNT        88U
#define LED6028_SPATIAL_X_PERIOD     46U
#define LED6028_SPATIAL_Y_PERIOD      5U
typedef struct
{
  uint8_t x;
  uint8_t y;
} LED6028_Position;
static const LED6028_Position g_led6028_xy[LED6028_SPATIAL_COUNT] =
{
  {30U, 0U}, /* LED #00 Ins */
  {32U, 0U}, /* LED #01 Home */
  {34U, 0U}, /* LED #02 PgUp */
  {30U, 1U}, /* LED #03 Del */
  {32U, 1U}, /* LED #04 End */
  {34U, 1U}, /* LED #05 PgDn */
  {30U, 4U}, /* LED #06 ← */
  {32U, 4U}, /* LED #07 ↓ */
  {34U, 4U}, /* LED #08 → */
  {32U, 3U}, /* LED #09 ↑ */
  {22U, 0U}, /* LED #10 - */
  {24U, 0U}, /* LED #11 = */
  {26U, 0U}, /* LED #12 Back */
  {23U, 1U}, /* LED #13 [ */
  {25U, 1U}, /* LED #14 ] */
  {27U, 1U}, /* LED #15 Backslash */
  {22U, 2U}, /* LED #16 ; */
  {24U, 2U}, /* LED #17 ' */
  {26U, 2U}, /* LED #18 Enter */
  {26U, 4U}, /* LED #19 Ctrl-R */
  {22U, 3U}, /* LED #20 ? */
  {26U, 3U}, /* LED #21 Shift-R */
  {22U, 4U}, /* LED #22 Fn */
  {24U, 4U}, /* LED #23 Win-R */
  {16U, 0U}, /* LED #24 8 */
  {18U, 0U}, /* LED #25 9 */
  {20U, 0U}, /* LED #26 0 */
  {15U, 1U}, /* LED #27 U */
  {17U, 1U}, /* LED #28 I */
  {19U, 1U}, /* LED #29 O */
  {21U, 1U}, /* LED #30 P */
  {16U, 2U}, /* LED #31 J */
  {18U, 2U}, /* LED #32 K */
  {20U, 2U}, /* LED #33 L */
  {20U, 4U}, /* LED #34 Alt-R */
  {14U, 3U}, /* LED #35 N */
  {16U, 3U}, /* LED #36 M */
  {18U, 3U}, /* LED #37 < */
  {20U, 3U}, /* LED #38 > */
  { 8U, 0U}, /* LED #39 4 */
  {10U, 0U}, /* LED #40 5 */
  {12U, 0U}, /* LED #41 6 */
  {14U, 0U}, /* LED #42 7 */
  { 7U, 1U}, /* LED #43 E */
  { 9U, 1U}, /* LED #44 R */
  {11U, 1U}, /* LED #45 T */
  {13U, 1U}, /* LED #46 Y */
  { 8U, 2U}, /* LED #47 D */
  {10U, 2U}, /* LED #48 F */
  {12U, 2U}, /* LED #49 G */
  {14U, 2U}, /* LED #50 H */
  { 8U, 3U}, /* LED #51 C */
  {10U, 3U}, /* LED #52 V */
  {12U, 3U}, /* LED #53 B */
  {12U, 4U}, /* LED #54 Space */
  { 0U, 0U}, /* LED #55 ~ */
  { 2U, 0U}, /* LED #56 1 */
  { 4U, 0U}, /* LED #57 2 */
  { 6U, 0U}, /* LED #58 3 */
  { 1U, 1U}, /* LED #59 Tab */
  { 3U, 1U}, /* LED #60 Q */
  { 5U, 1U}, /* LED #61 W */
  { 2U, 2U}, /* LED #62 Caps */
  { 4U, 2U}, /* LED #63 A */
  { 6U, 2U}, /* LED #64 S */
  { 0U, 3U}, /* LED #65 Shift-L */
  { 4U, 3U}, /* LED #66 Z */
  { 6U, 3U}, /* LED #67 X */
  { 0U, 4U}, /* LED #68 Ctrl-L */
  { 3U, 4U}, /* LED #69 Win-L */
  { 5U, 4U}, /* LED #70 Alt-L */

  /* Numpad DIN->DOUT order verified from supplied PCB, indices 71..87. */
  {38U, 0U}, /* LED #71 Num */
  {40U, 0U}, /* LED #72 / */
  {42U, 0U}, /* LED #73 * */
  {44U, 0U}, /* LED #74 - */
  {38U, 1U}, /* LED #75 7 */
  {40U, 1U}, /* LED #76 8 */
  {42U, 1U}, /* LED #77 9 */
  {44U, 2U}, /* LED #78 + (middle of two rows) */
  {38U, 2U}, /* LED #79 4 */
  {40U, 2U}, /* LED #80 5 */
  {42U, 2U}, /* LED #81 6 */
  {44U, 4U}, /* LED #82 Enter (middle of two rows) */
  {38U, 3U}, /* LED #83 1 */
  {40U, 3U}, /* LED #84 2 */
  {42U, 3U}, /* LED #85 3 */
  {42U, 4U}, /* LED #86 . */
  {39U, 4U}, /* LED #87 0 / U3 */
};


/* WebHID RGB controller. Mode: off=0, breathe=1, solid=2, horizontal rainbow=3, vertical rainbow=4. */
#define WEBHID_LOG_QUEUE_SIZE        16U
static uint8_t g_rgb_mode = 1U;
static uint8_t g_rgb_red = 128U;
static uint8_t g_rgb_green = 32U;
static uint8_t g_rgb_blue = 50U;
static uint8_t g_rgb_brightness = 180U;
static uint8_t g_rgb_speed = 5U;
/* Temporary LED diagnostic mode, independent of the 5 PCB physical layouts.
 * 0=normal, 1=one LED at index, 2=all LEDs in one low-power test color.
 * Deliberately never persisted to flash.
 */
static uint8_t g_rgb_test_mode = 0U;
static uint8_t g_rgb_test_index = 0U;
static uint8_t g_rgb_test_red = 48U;
static uint8_t g_rgb_test_green = 0U;
static uint8_t g_rgb_test_blue = 0U;
/* Persistent RGB configuration for STM32F103C8 (64KB, 1KB erase pages).
 * Linked firmware ends before 0x0800F800. Use two alternating pages so
 * power loss during an update leaves the previously valid record intact.
 * No automatic writes while moving a slider; only explicit SAVE (0x08).
 */
#define RGB_STORE_PAGE_A        0x0800F800UL
#define RGB_STORE_PAGE_B        0x0800FC00UL
#define RGB_STORE_MAGIC         0x4D505247UL  /* "MPRG" */
#define RGB_STORE_COMMITTED     0xA55AU

typedef struct
{
  uint32_t magic;
  uint32_t sequence;
  uint8_t mode;
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  uint8_t brightness;
  uint8_t speed;
  uint8_t reserved0;
  uint8_t reserved1;
  uint16_t checksum;
  uint16_t committed;  /* Program LAST to make partial writes invalid */
} RGB_StoreRecord;

_Static_assert(sizeof(RGB_StoreRecord) == 20U, "RGB flash record must be 20 bytes");
_Static_assert(offsetof(RGB_StoreRecord, checksum) == 16U, "Bad RGB record checksum offset");
_Static_assert(offsetof(RGB_StoreRecord, committed) == 18U, "Bad RGB record commit offset");

static RGB_StoreRecord g_rgb_saved_record;
static uint8_t g_rgb_saved_valid = 0U;
static uint8_t g_rgb_saved_slot = 0U;  /* 0=A, 1=B. Meaningful only if valid. */

static uint8_t g_webhid_log_on = 0U;
static uint8_t g_webhid_reply[WEBHID_PACKET_SIZE];
static uint8_t g_webhid_reply_pending = 0U;
static uint8_t g_webhid_log_queue[WEBHID_LOG_QUEUE_SIZE][WEBHID_PACKET_SIZE];
static uint8_t g_webhid_log_head = 0U;
static uint8_t g_webhid_log_tail = 0U;

/* DWT CYCCNT is already enabled for software I2C. WFI idle is excluded
 * from the active time estimate (interrupts during WFI may be undercounted). */
typedef struct
{
  uint16_t scan_hz;
  uint16_t scan_avg_us;
  uint16_t scan_max_us;
  uint16_t hid_reports_hz;
  uint16_t active_permille;
  uint16_t window_ms;
  uint8_t ready;
} KeyboardMetrics;

static KeyboardMetrics g_metrics;
static uint32_t g_metrics_scan_count;
static uint64_t g_metrics_scan_cycles;
static uint32_t g_metrics_max_scan_cycles;
static uint32_t g_metrics_report_count;
static uint64_t g_metrics_busy_cycles;
/* USER CODE END PV */

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);

/* USER CODE BEGIN PFP */
static void CycleCounter_Init(void);
static void Metrics_FinishWindow(uint32_t elapsed_ms);
static void Metrics_PackReply(uint8_t reply[WEBHID_PACKET_SIZE]);
static void DelayUs(uint32_t us);
static void HwI2C_Init(void);
static uint8_t MCP23008_ColumnRead(uint8_t addr7, uint8_t outputs, uint8_t *gpio);
static uint8_t HwI2C_Probe(uint8_t addr7);
static void MCP23008_ScanBus(void);
static uint8_t HwI2C_WriteReg(uint8_t addr7, uint8_t reg, uint8_t value);
static uint8_t HwI2C_ReadReg(uint8_t addr7, uint8_t reg, uint8_t *value);
static uint8_t MCP23008_CountReady(void);
static uint8_t MCP23008_InitAll(void);
static void MCP23008_PollAll(void);
static void Keyboard_Init(void);
static void Keyboard_Scan(void);
static uint8_t Keyboard_KeyDown(uint8_t addr, uint8_t row, uint8_t col);
static void Keyboard_RebootShortcut(uint32_t now);
static void Keyboard_BuildReport(void);
static void Keyboard_USBService(void);
static void WebHID_Service(void);
static void WebHID_QueueLog(const char *text, int len);
static void RGB_StoreLoad(void);
static uint8_t RGB_StoreSave(void);
static uint8_t RGB_StoreMatchesCurrent(void);
static void USB_ForceReenumeration(void);
static void Keyboard_UpdateLEDs(uint32_t now);
static void LED6028_Init(void);
static void LED6028_SetPixel(uint8_t index, uint8_t red, uint8_t green, uint8_t blue);
static void LED6028_SetAll(uint8_t red, uint8_t green, uint8_t blue);
static void LED6028_Show(void);
static void LED6028_BreathService(uint32_t now);
/* USER CODE END PFP */

/* USER CODE BEGIN 0 */
/* Log packets are queued in the main loop, not transmitted during UART
 * printf. They are dropped if the browser cannot keep up.
 */
static void WebHID_QueueLog(const char *message, int len)
{
  if ((g_webhid_log_on == 0U) || (message == NULL) || (len <= 0)) return;

  int offset = 0;
  while (offset < len)
  {
    uint8_t next = (uint8_t)((g_webhid_log_head + 1U) % WEBHID_LOG_QUEUE_SIZE);
    if (next == g_webhid_log_tail) return; /* Queue full, do not block typing. */

    uint8_t chunk = (uint8_t)(((len - offset) > 30) ? 30 : (len - offset));
    uint8_t *packet = g_webhid_log_queue[g_webhid_log_head];
    memset(packet, 0, WEBHID_PACKET_SIZE);
    packet[0] = 0x80U; /* Text log */
    packet[1] = chunk;
    memcpy(packet + 2, message + offset, chunk);
    g_webhid_log_head = next;
    offset += chunk;
  }
}

/* WebHID application-level protocol, always 32 bytes:
 * PC->keyboard: 01 status, 02 mode, 03 RGB, 04 brightness,
 *               05 speed, 06 log on/off, 07 LED diagnostic mode/index/RGB.
 * Diagnostic 07: [07, mode(0/1/2), index(0..99), R, G, B, ...].
 * Keyboard->PC: 81 state, 82 command ACK, 80 text log.
 */
/* CRC-16/CCITT-FALSE over exactly the first 16 bytes, independent of
 * any padding. Read-only validation runs on every boot, before USB starts.
 */
static uint16_t RGB_StoreChecksum(const RGB_StoreRecord *record)
{
  const uint8_t *data = (const uint8_t *)record;
  uint16_t crc = 0xFFFFU;
  for (uint32_t i = 0U; i < offsetof(RGB_StoreRecord, checksum); i++)
  {
    crc ^= (uint16_t)((uint16_t)data[i] << 8);
    for (uint8_t bit = 0U; bit < 8U; bit++)
      crc = (crc & 0x8000U) ? (uint16_t)((crc << 1) ^ 0x1021U)
                             : (uint16_t)(crc << 1);
  }
  return crc;
}

static uint8_t RGB_StoreIsValid(const RGB_StoreRecord *record)
{
  if ((record->magic != RGB_STORE_MAGIC) ||
      (record->committed != RGB_STORE_COMMITTED) ||
      (record->mode > 4U) ||
      (record->speed < 1U) ||
      (record->speed > 10U) ||
      (record->reserved0 != 0U) ||
      (record->reserved1 != 0U))
    return 0U;
  return (record->checksum == RGB_StoreChecksum(record)) ? 1U : 0U;
}

static void RGB_StoreLoad(void)
{
  const RGB_StoreRecord *a = (const RGB_StoreRecord *)RGB_STORE_PAGE_A;
  const RGB_StoreRecord *b = (const RGB_StoreRecord *)RGB_STORE_PAGE_B;
  const uint8_t valid_a = RGB_StoreIsValid(a);
  const uint8_t valid_b = RGB_StoreIsValid(b);
  const RGB_StoreRecord *chosen = NULL;

  if (valid_a && valid_b)
  {
    /* Signed difference handles sequence wrap; saves cannot jump 2^31. */
    g_rgb_saved_slot = ((int32_t)(b->sequence - a->sequence) > 0) ? 1U : 0U;
    chosen = (g_rgb_saved_slot == 0U) ? a : b;
  }
  else if (valid_a)
  {
    g_rgb_saved_slot = 0U;
    chosen = a;
  }
  else if (valid_b)
  {
    g_rgb_saved_slot = 1U;
    chosen = b;
  }

  if (chosen != NULL)
  {
    memcpy(&g_rgb_saved_record, chosen, sizeof(g_rgb_saved_record));
    g_rgb_saved_valid = 1U;
    g_rgb_mode = chosen->mode;
    g_rgb_red = chosen->red;
    g_rgb_green = chosen->green;
    g_rgb_blue = chosen->blue;
    g_rgb_brightness = chosen->brightness;
    g_rgb_speed = chosen->speed;
  }
}

static uint8_t RGB_StoreMatchesCurrent(void)
{
  return (g_rgb_saved_valid &&
          g_rgb_saved_record.mode == g_rgb_mode &&
          g_rgb_saved_record.red == g_rgb_red &&
          g_rgb_saved_record.green == g_rgb_green &&
          g_rgb_saved_record.blue == g_rgb_blue &&
          g_rgb_saved_record.brightness == g_rgb_brightness &&
          g_rgb_saved_record.speed == g_rgb_speed) ? 1U : 0U;
}

static uint8_t RGB_StoreSave(void)
{
  /* Diagnostic LEDs and USB logs are intentionally not persisted. */
  if (g_rgb_test_mode != 0U) return 0U;
  if (RGB_StoreMatchesCurrent() != 0U) return 1U; /* Avoid Flash wear. */

  RGB_StoreRecord next = {0};
  next.magic = RGB_STORE_MAGIC;
  next.sequence = g_rgb_saved_valid ? (g_rgb_saved_record.sequence + 1U) : 1U;
  next.mode = g_rgb_mode;
  next.red = g_rgb_red;
  next.green = g_rgb_green;
  next.blue = g_rgb_blue;
  next.brightness = g_rgb_brightness;
  next.speed = g_rgb_speed;
  next.checksum = RGB_StoreChecksum(&next);
  next.committed = RGB_STORE_COMMITTED;

  /* Never erase the page containing the most recently committed record. */
  const uint8_t next_slot = g_rgb_saved_valid ?
    (uint8_t)(1U - g_rgb_saved_slot) : 0U;
  const uint32_t address = next_slot ? RGB_STORE_PAGE_B : RGB_STORE_PAGE_A;
  FLASH_EraseInitTypeDef erase = {0};
  uint32_t page_error = 0xFFFFFFFFUL;
  uint8_t ok = 1U;

  erase.TypeErase = FLASH_TYPEERASE_PAGES;
  erase.PageAddress = address;
  erase.NbPages = 1U;
  if (HAL_FLASH_Unlock() != HAL_OK) return 0U;

  if (HAL_FLASHEx_Erase(&erase, &page_error) != HAL_OK)
  {
    ok = 0U;
  }
  else
  {
    /* F103 Flash writes are halfword-sized. Commit marker MUST be last.
     * All other fields and the checksum are programmed before this marker.
     */
    const uint8_t *bytes = (const uint8_t *)&next;
    for (uint32_t offset = 0U;
         offset < offsetof(RGB_StoreRecord, committed); offset += 2U)
    {
      uint16_t halfword = (uint16_t)bytes[offset] |
                          (uint16_t)((uint16_t)bytes[offset + 1U] << 8);
      if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,
                            address + offset, halfword) != HAL_OK)
      {
        ok = 0U;
        break;
      }
    }
    if (ok != 0U &&
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,
                          address + offsetof(RGB_StoreRecord, committed),
                          next.committed) != HAL_OK)
      ok = 0U;
  }
  (void)HAL_FLASH_Lock();

  /* Verify actual Flash contents before acknowledging success to the PC. */
  if ((ok == 0U) ||
      (RGB_StoreIsValid((const RGB_StoreRecord *)address) == 0U))
    return 0U;

  memcpy(&g_rgb_saved_record, (const void *)address, sizeof(g_rgb_saved_record));
  g_rgb_saved_slot = next_slot;
  g_rgb_saved_valid = 1U;
  return 1U;
}

static void Metrics_FinishWindow(uint32_t elapsed_ms)
{
  const uint32_t cycles_per_us = SystemCoreClock / 1000000U;
  const uint64_t expected_cycles = (uint64_t)SystemCoreClock * elapsed_ms / 1000U;
  uint64_t permille = expected_cycles == 0U ? 0U :
                       (g_metrics_busy_cycles * 1000U + expected_cycles / 2U) / expected_cycles;
  if (permille > 1000U) permille = 1000U;

  uint32_t rate = elapsed_ms ? (g_metrics_scan_count * 1000U) / elapsed_ms : 0U;
  g_metrics.scan_hz = rate > 65535U ? 65535U : (uint16_t)rate;
  uint32_t report_rate = elapsed_ms ? (g_metrics_report_count * 1000U) / elapsed_ms : 0U;
  g_metrics.hid_reports_hz = report_rate > 65535U ? 65535U : (uint16_t)report_rate;
  uint64_t avg_us = (g_metrics_scan_count != 0U && cycles_per_us != 0U) ?
                     g_metrics_scan_cycles / (g_metrics_scan_count * (uint64_t)cycles_per_us) : 0U;
  g_metrics.scan_avg_us = avg_us > 65535U ? 65535U : (uint16_t)avg_us;
  uint32_t max_us = cycles_per_us ? g_metrics_max_scan_cycles / cycles_per_us : 0U;
  g_metrics.scan_max_us = max_us > 65535U ? 65535U : (uint16_t)max_us;
  g_metrics.active_permille = (uint16_t)permille;
  g_metrics.window_ms = elapsed_ms > 65535U ? 65535U : (uint16_t)elapsed_ms;
  g_metrics.ready = 1U;

  g_metrics_scan_count = 0U;
  g_metrics_scan_cycles = 0U;
  g_metrics_max_scan_cycles = 0U;
  g_metrics_report_count = 0U;
  g_metrics_busy_cycles = 0U;
}

/* Protocol extension: GET_METRICS 0x09 replies 0x83 with 32 bytes.
 * [1]=format v1; [2]=USB FS bInterval (ms);
 * little-endian 16-bit [3]=scanHz, [5]=avgScanUs, [7]=maxScanUs,
 * [9]=HID reports submitted/s, [11]=active CPU permille,
 * [13]=sample window ms; [15]=valid snapshot.
 * Reports/s != host polling Hz; endpoints poll even without key events. */
static void Metrics_PackReply(uint8_t reply[WEBHID_PACKET_SIZE])
{
  reply[0] = 0x83U;
  reply[1] = 1U;
  reply[2] = HID_FS_BINTERVAL;
  const uint16_t values[6] = {
    g_metrics.scan_hz, g_metrics.scan_avg_us, g_metrics.scan_max_us,
    g_metrics.hid_reports_hz, g_metrics.active_permille, g_metrics.window_ms
  };
  for (uint8_t i = 0U; i < 6U; ++i)
  {
    reply[3U + 2U * i] = (uint8_t)(values[i] & 0xFFU);
    reply[4U + 2U * i] = (uint8_t)(values[i] >> 8U);
  }
  reply[15] = g_metrics.ready;
}

static void WebHID_Service(void)
{
  uint8_t command[WEBHID_PACKET_SIZE];

  if ((g_webhid_reply_pending == 0U) &&
      (USBD_HID_WebhidRead(&hUsbDeviceFS, command) != 0U))
  {
    uint8_t ok = 1U;
    memset(g_webhid_reply, 0, WEBHID_PACKET_SIZE);
    g_webhid_reply[0] = 0x82U; /* ACK */
    g_webhid_reply[1] = command[0];

    switch (command[0])
    {
      case 0x01U: /* GET_STATE */
        g_webhid_reply[0] = 0x81U;
        g_webhid_reply[1] = 3U; /* Protocol version */
        g_webhid_reply[2] = g_rgb_mode;
        g_webhid_reply[3] = g_rgb_brightness;
        g_webhid_reply[4] = g_rgb_red;
        g_webhid_reply[5] = g_rgb_green;
        g_webhid_reply[6] = g_rgb_blue;
        g_webhid_reply[7] = g_rgb_speed;
        g_webhid_reply[8] = g_webhid_log_on;
        g_webhid_reply[9] = g_mcp_found_count;
        g_webhid_reply[10] = MCP23008_COUNT;
        g_webhid_reply[11] = KEY_COUNT;
        g_webhid_reply[12] = LED6028_COUNT;
        g_webhid_reply[13] = g_rgb_test_mode;
        g_webhid_reply[14] = g_rgb_test_index;
        g_webhid_reply[15] = g_rgb_test_red;
        g_webhid_reply[16] = g_rgb_test_green;
        g_webhid_reply[17] = g_rgb_test_blue;
        g_webhid_reply[18] = g_rgb_saved_valid;
        g_webhid_reply[19] = RGB_StoreMatchesCurrent();
        break;
      case 0x02U:
        if (command[1] <= 4U)
        {
          g_rgb_test_mode = 0U;
          g_rgb_mode = command[1];
        }
        else ok = 0U;
        break;
      case 0x03U:
        g_rgb_test_mode = 0U;
        g_rgb_red = command[1];
        g_rgb_green = command[2];
        g_rgb_blue = command[3];
        break;
      case 0x04U:
        g_rgb_test_mode = 0U;
        g_rgb_brightness = command[1];
        break;
      case 0x05U:
        if ((command[1] >= 1U) && (command[1] <= 10U))
        {
          g_rgb_test_mode = 0U;
          g_rgb_speed = command[1];
        }
        else ok = 0U;
        break;
      case 0x06U:
        if (command[1] <= 1U)
        {
          g_webhid_log_on = command[1];
          if (g_webhid_log_on == 0U)
            g_webhid_log_head = g_webhid_log_tail = 0U;
        }
        else ok = 0U;
        break;
      case 0x07U: /* RGB diagnostic: test physical chain, not key->LED map. */
        if ((command[1] <= 2U) &&
            ((command[1] != 1U) || (command[2] < LED6028_COUNT)))
        {
          g_rgb_test_mode = command[1];
          g_rgb_test_index = command[2];
          /* Keep all-board tests at deliberately conservative brightness. */
          g_rgb_test_red   = (command[3] > 64U) ? 64U : command[3];
          g_rgb_test_green = (command[4] > 64U) ? 64U : command[4];
          g_rgb_test_blue  = (command[5] > 64U) ? 64U : command[5];
        }
        else ok = 0U;
        break;
      case 0x08U: /* SAVE current RGB mode/color/brightness/speed to Flash */
        ok = RGB_StoreSave();
        break;
      case 0x09U: /* GET_METRICS: read-only snapshot */
        Metrics_PackReply(g_webhid_reply);
        break;
      default:
        ok = 0U;
        break;
    }

    if (g_webhid_reply[0] == 0x82U)
      g_webhid_reply[2] = ok;
    g_webhid_reply_pending = 1U;
  }

  /* Replies outrank logs, so sliders remain responsive with verbose logging. */
  if (g_webhid_reply_pending != 0U)
  {
    if (USBD_HID_WebhidSend(&hUsbDeviceFS, g_webhid_reply) != 0U)
      g_webhid_reply_pending = 0U;
  }
  else if (g_webhid_log_head != g_webhid_log_tail)
  {
    if (USBD_HID_WebhidSend(&hUsbDeviceFS, g_webhid_log_queue[g_webhid_log_tail]) != 0U)
      g_webhid_log_tail = (uint8_t)((g_webhid_log_tail + 1U) % WEBHID_LOG_QUEUE_SIZE);
  }
}

int _write(int file, char *ptr, int len)
{
  (void)file;
  if ((ptr == NULL) || (len <= 0)) return 0;
  if (HAL_UART_Transmit(&huart1, (uint8_t *)ptr, (uint16_t)len, HAL_MAX_DELAY) != HAL_OK) return -1;
  WebHID_QueueLog(ptr, len);
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

/* STM32F103 I2C1 hardware bus. Keep manual recovery pulses only while
 * I2C1 is disabled; all normal I/O is delegated to the ST HAL peripheral.
 * This preserves the existing six-board PCB wiring (PB8/PB9 shared bus).
 * The nine clocks can release a slave stuck waiting to finish a byte. */
static uint8_t g_hw_i2c_online = 0U;
static uint8_t g_i2c_software_fallback = 0U;

/* Recovery path uses original GPIO input-pull-up I2C when the PCB cannot
 * acknowledge any hardware I2C transaction. This is a compatibility
 * fallback, not 400kHz operation; status is printed to USB/UART logs.
 * Real Fast-mode hardware I2C still needs suitable EXTERNAL pull-ups. */
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
  GPIOB->BRR = HW_I2C_SCL_PIN;
  GPIOB_SetCRHNibble(8U, 0x6U);
}

static void SoftI2C_SCL_Release(void)
{
  GPIOB->BSRR = HW_I2C_SCL_PIN;
  GPIOB_SetCRHNibble(8U, 0x8U);
}

static void SoftI2C_SDA_Low(void)
{
  GPIOB->BRR = HW_I2C_SDA_PIN;
  GPIOB_SetCRHNibble(9U, 0x6U);
}

static void SoftI2C_SDA_Release(void)
{
  GPIOB->BSRR = HW_I2C_SDA_PIN;
  GPIOB_SetCRHNibble(9U, 0x8U);
}

static uint8_t SoftI2C_SDA_Read(void)
{
  return ((GPIOB->IDR & HW_I2C_SDA_PIN) != 0U) ? 1U : 0U;
}

static void SoftI2C_WaitSCLHigh(void)
{
  uint32_t timeout = 2000U;
  SoftI2C_SCL_Release();
  while (((GPIOB->IDR & HW_I2C_SCL_PIN) == 0U) && (timeout > 0U)) timeout--;
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

/* Fast matrix transaction: one address selection, a GPIO write,
 * REPEATED START, GPIO read. IOCON.SEQOP=1 is required to keep the
 * address pointer at GPIO (0x09) after writing GPIO.
 * Writing GPIO updates OLAT for output bits GP4..GP7, while reading
 * GPIO samples the live row pins GP0..GP3.
 * The 5us settle time after column latch is retained. */
static uint8_t SoftI2C_ColumnRead(uint8_t addr7, uint8_t outputs, uint8_t *gpio)
{
  if (gpio == NULL) return 0U;

  SoftI2C_Start();
  const uint8_t cmd_ok =
       SoftI2C_WriteByte((uint8_t)(addr7 << 1U)) &&
       SoftI2C_WriteByte(MCP23008_REG_GPIO) &&
       SoftI2C_WriteByte(outputs);
  if (cmd_ok == 0U)
  {
    SoftI2C_Stop();
    return 0U;
  }

  DelayUs(5U);
  SoftI2C_Start();
  if (SoftI2C_WriteByte((uint8_t)((addr7 << 1U) | 1U)) == 0U)
  {
    SoftI2C_Stop();
    return 0U;
  }
  *gpio = SoftI2C_ReadByte(0U);
  SoftI2C_Stop();
  return 1U;
}


static uint8_t HwI2C_AnyDeviceReady(void)
{
  if (g_hw_i2c_online == 0U) return 0U;
  for (uint8_t i = 0U; i < MCP23008_COUNT; ++i)
  {
    if (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(g_mcp_addrs[i] << 1U), 1U,
                              HW_I2C_TIMEOUT_MS) == HAL_OK) return 1U;
  }
  return 0U;
}

static void HwI2C_FallbackToSoftware(void)
{
  if (hi2c1.Instance == I2C1 && hi2c1.State != HAL_I2C_STATE_RESET)
    (void)HAL_I2C_DeInit(&hi2c1);
  __HAL_RCC_I2C1_CLK_DISABLE();
  __HAL_RCC_I2C1_FORCE_RESET();
  __HAL_RCC_I2C1_RELEASE_RESET();
  g_hw_i2c_online = 0U;
  g_i2c_software_fallback = 1U;
  SoftI2C_Init();

  printf("[I2C] SOFTWARE FALLBACK: PB8 SCL=%u PB9 SDA=%u, MCP 0x%02X ACK=%u\r\n",
         (unsigned int)((GPIOB->IDR & HW_I2C_SCL_PIN) != 0U),
         (unsigned int)((GPIOB->IDR & HW_I2C_SDA_PIN) != 0U),
         (unsigned int)g_mcp_addrs[0],
         (unsigned int)SoftI2C_Probe(g_mcp_addrs[0]));
}

static void HwI2C_Init(void)
{
  GPIO_InitTypeDef gpio = {0};
  if (g_i2c_software_fallback != 0U)
  {
    SoftI2C_Init();
    return;
  }
  g_hw_i2c_online = 0U;

  if (hi2c1.Instance == I2C1 && hi2c1.State != HAL_I2C_STATE_RESET)
    (void)HAL_I2C_DeInit(&hi2c1);
  __HAL_RCC_I2C1_CLK_DISABLE();
  /* Reset the F1 peripheral's BUSY/STOP state before reinitialization. */
  __HAL_RCC_I2C1_FORCE_RESET();
  __HAL_RCC_I2C1_RELEASE_RESET();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_AFIO_CLK_ENABLE();
  __HAL_AFIO_REMAP_I2C1_ENABLE();

  /* Temporarily use open-drain GPIO for bus recovery. SDA must be released
   * during SCL pulses. Do not use push-pull on a shared I2C bus. */
  HAL_GPIO_WritePin(GPIOB, HW_I2C_SCL_PIN | HW_I2C_SDA_PIN, GPIO_PIN_SET);
  gpio.Pin = HW_I2C_SCL_PIN | HW_I2C_SDA_PIN;
  gpio.Mode = GPIO_MODE_OUTPUT_OD;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &gpio);

  DelayUs(10U);
  for (uint8_t bit = 0U; bit < 9U; ++bit)
  {
    HAL_GPIO_WritePin(GPIOB, HW_I2C_SCL_PIN, GPIO_PIN_RESET);
    DelayUs(5U);
    HAL_GPIO_WritePin(GPIOB, HW_I2C_SCL_PIN, GPIO_PIN_SET);
    DelayUs(5U);
    if ((GPIOB->IDR & HW_I2C_SDA_PIN) != 0U) break;
  }
  /* STOP: release SCL, then SDA. */
  HAL_GPIO_WritePin(GPIOB, HW_I2C_SDA_PIN, GPIO_PIN_RESET);
  DelayUs(5U);
  HAL_GPIO_WritePin(GPIOB, HW_I2C_SCL_PIN, GPIO_PIN_SET);
  DelayUs(5U);
  HAL_GPIO_WritePin(GPIOB, HW_I2C_SDA_PIN, GPIO_PIN_SET);
  DelayUs(5U);

  gpio.Mode = GPIO_MODE_AF_OD;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &gpio);

  __HAL_RCC_I2C1_CLK_ENABLE();
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = HW_I2C_CLOCK_HZ;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0U;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0U;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  g_hw_i2c_online = HAL_I2C_Init(&hi2c1) == HAL_OK ? 1U : 0U;

  if (HwI2C_AnyDeviceReady() == 0U)
  {
    printf("[I2C] 400kHz NO ACK; AF_OD idle SCL=%u SDA=%u, SR2=0x%04X, Error=0x%lX\r\n",
           (unsigned int)((GPIOB->IDR & HW_I2C_SCL_PIN) != 0U),
           (unsigned int)((GPIOB->IDR & HW_I2C_SDA_PIN) != 0U),
           (unsigned int)I2C1->SR2, (unsigned long)hi2c1.ErrorCode);

    /* Retry at 100kHz to distinguish Fast-mode rise-time problems from
     * missing pull-ups or permanently held-low lines. */
    if (g_hw_i2c_online != 0U)
    {
      (void)HAL_I2C_DeInit(&hi2c1);
      __HAL_RCC_I2C1_FORCE_RESET();
      __HAL_RCC_I2C1_RELEASE_RESET();
      hi2c1.Init.ClockSpeed = 100000U;
      g_hw_i2c_online = HAL_I2C_Init(&hi2c1) == HAL_OK ? 1U : 0U;
    }
    if (HwI2C_AnyDeviceReady() == 0U)
    {
      printf("[I2C] 100kHz NO ACK; using original software I2C for recovery\r\n");
      HwI2C_FallbackToSoftware();
    }
    else
      printf("[I2C] Hardware I2C recovered at 100kHz\r\n");
  }
  else
    printf("[I2C] Hardware I2C 400kHz OK\r\n");
}

static uint8_t HwI2C_Probe(uint8_t addr7)
{
  if (g_i2c_software_fallback != 0U) return SoftI2C_Probe(addr7);
  if (g_hw_i2c_online == 0U) return 0U;
  return HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(addr7 << 1U), 1U,
                               HW_I2C_TIMEOUT_MS) == HAL_OK ? 1U : 0U;
}

/* Diagnostic only: probe every valid MCP23008 7-bit address.
 * This does not change the configured devices or key mapping.
 */
static void MCP23008_ScanBus(void)
{
  uint8_t found = 0U;

  printf("\r\n=== I2C address scan (0x20-0x27) ===\r\n");
  for (uint8_t addr = MCP23008_ADDR_FIRST; addr <= MCP23008_ADDR_LAST; addr++)
  {
    uint8_t ack = HwI2C_Probe(addr);
    if (ack != 0U) found++;
    printf("I2C 0x%02X : %s\r\n", (unsigned int)addr,
           (ack != 0U) ? "FOUND" : "MISS");
  }
  printf("I2C scan: %u responding address(es); firmware configured: %u\r\n",
         (unsigned int)found, (unsigned int)MCP23008_COUNT);
  printf("=======================================\r\n");
}

static uint8_t HwI2C_WriteReg(uint8_t addr7, uint8_t reg, uint8_t value)
{
  if (g_i2c_software_fallback != 0U) return SoftI2C_WriteReg(addr7, reg, value);
  if (g_hw_i2c_online == 0U) return 0U;
  return HAL_I2C_Mem_Write(&hi2c1, (uint16_t)(addr7 << 1U), reg,
                           I2C_MEMADD_SIZE_8BIT, &value, 1U,
                           HW_I2C_TIMEOUT_MS) == HAL_OK ? 1U : 0U;
}

static uint8_t HwI2C_ReadReg(uint8_t addr7, uint8_t reg, uint8_t *value)
{
  if (g_i2c_software_fallback != 0U) return SoftI2C_ReadReg(addr7, reg, value);
  if ((g_hw_i2c_online == 0U) || (value == NULL)) return 0U;
  return HAL_I2C_Mem_Read(&hi2c1, (uint16_t)(addr7 << 1U), reg,
                          I2C_MEMADD_SIZE_8BIT, value, 1U,
                          HW_I2C_TIMEOUT_MS) == HAL_OK ? 1U : 0U;
}

/* IOCON.SEQOP=1 fixes the pointer at GPIO after a GPIO write.
 * First send [GPIO register address, selected column], STOP,
 * then read the GPIO register. Each step is hardware-clocked at 400kHz.
 * The extra 5us preserves row settling before reading. */
static uint8_t MCP23008_ColumnRead(uint8_t addr7, uint8_t outputs, uint8_t *gpio)
{
  if (g_i2c_software_fallback != 0U) return SoftI2C_ColumnRead(addr7, outputs, gpio);
  if ((g_hw_i2c_online == 0U) || (gpio == NULL)) return 0U;
  const uint16_t addr8 = (uint16_t)(addr7 << 1U);
  uint8_t payload[2] = {MCP23008_REG_GPIO, outputs};

  if (HAL_I2C_Master_Transmit(&hi2c1, addr8, payload, sizeof(payload),
                              HW_I2C_TIMEOUT_MS) != HAL_OK) return 0U;
  DelayUs(5U);
  return HAL_I2C_Master_Receive(&hi2c1, addr8, gpio, 1U,
                               HW_I2C_TIMEOUT_MS) == HAL_OK ? 1U : 0U;
}

static uint8_t MCP23008_Config(uint8_t addr)
{
  uint8_t iodir = 0U, gppu = 0U, iocon = 0U;
  if (HwI2C_Probe(addr) == 0U) return 0U;
  if (HwI2C_WriteReg(addr, MCP23008_REG_IODIR, 0x0FU) == 0U) return 0U;
  if (HwI2C_WriteReg(addr, MCP23008_REG_IPOL, 0x00U) == 0U) return 0U;
  if (HwI2C_WriteReg(addr, MCP23008_REG_GPINTEN, 0x00U) == 0U) return 0U;
  /* Byte mode keeps pointer at GPIO for back-to-back write/read. */
  if (HwI2C_WriteReg(addr, MCP23008_REG_IOCON, 0x20U) == 0U) return 0U;
  if (HwI2C_WriteReg(addr, MCP23008_REG_GPPU, 0x0FU) == 0U) return 0U;
  if (HwI2C_WriteReg(addr, MCP23008_REG_OLAT, 0xF0U) == 0U) return 0U;
  if (HwI2C_ReadReg(addr, MCP23008_REG_IODIR, &iodir) == 0U) return 0U;
  if (HwI2C_ReadReg(addr, MCP23008_REG_GPPU, &gppu) == 0U) return 0U;
  if (HwI2C_ReadReg(addr, MCP23008_REG_IOCON, &iocon) == 0U) return 0U;
  return ((iodir == 0x0FU) && ((gppu & 0x0FU) == 0x0FU) &&
          ((iocon & 0x20U) != 0U)) ? 1U : 0U;
}

/* Recalculate actual online devices, including failures during key scanning.
 * Never use the last initialization result as live readiness status.
 */
static uint8_t MCP23008_CountReady(void)
{
  uint8_t count = 0U;
  for (uint8_t i = 0U; i < MCP23008_COUNT; i++)
  {
    if (g_mcp_ready[i] != 0U) count++;
  }
  g_mcp_found_count = count;
  return count;
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
    if ((g_mcp_ready[i] != 0U) && (HwI2C_ReadReg(addr, MCP23008_REG_GPIO, &gpio) != 0U))
    {
      printf(" %02X=%02X", (unsigned int)addr, (unsigned int)gpio);
    }
    else
    {
      g_mcp_ready[i] = 0U;
      printf(" %02X=ERR", (unsigned int)addr);
    }
  }
  (void)MCP23008_CountReady();
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
    case HID_KEY_SEMICOLON: return ";"; case HID_KEY_APOSTROPHE: return "'"; case HID_KEY_GRAVE: return "`"; case HID_KEY_COMMA: return ",";
    case HID_KEY_DOT: return "."; case HID_KEY_SLASH: return "/"; case HID_KEY_CAPS_LOCK: return "CAPS";
    case HID_KEY_INSERT: return "INS"; case HID_KEY_HOME: return "HOME"; case HID_KEY_PAGE_UP: return "PGUP";
    case HID_KEY_DELETE: return "DEL"; case HID_KEY_END: return "END"; case HID_KEY_PAGE_DOWN: return "PGDN";
    case HID_KEY_RIGHT: return "RIGHT"; case HID_KEY_LEFT: return "LEFT"; case HID_KEY_DOWN: return "DOWN"; case HID_KEY_UP: return "UP";
    case HID_KEY_KP_NUM_LOCK: return "KP_NUMLOCK"; case HID_KEY_KP_DIVIDE: return "KP_DIVIDE";
    case HID_KEY_KP_MULTIPLY: return "KP_MULTIPLY"; case HID_KEY_KP_MINUS: return "KP_MINUS";
    case HID_KEY_KP_PLUS: return "KP_PLUS"; case HID_KEY_KP_ENTER: return "KP_ENTER";
    case HID_KEY_KP_1: return "KP_1"; case HID_KEY_KP_2: return "KP_2"; case HID_KEY_KP_3: return "KP_3";
    case HID_KEY_KP_4: return "KP_4"; case HID_KEY_KP_5: return "KP_5"; case HID_KEY_KP_6: return "KP_6";
    case HID_KEY_KP_7: return "KP_7"; case HID_KEY_KP_8: return "KP_8"; case HID_KEY_KP_9: return "KP_9";
    case HID_KEY_KP_DOT: return "KP_DOT";
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
  memset(g_matrix_prev_rbits, 0x0F, sizeof(g_matrix_prev_rbits));
  g_report_pending = 1U;
}

/* Query the debounced key state by physical coordinates, so VIA/HID
 * remapping does not change the hardware recovery shortcut.
 */
static uint8_t Keyboard_KeyDown(uint8_t addr, uint8_t row, uint8_t col)
{
  for (uint8_t i = 0U; i < KEY_COUNT; i++)
  {
    if ((g_keymap[i].addr == addr) &&
        (g_keymap[i].row == row) &&
        (g_keymap[i].col == col))
      return g_key_state[i];
  }
  return 0U;
}

static void Keyboard_RebootShortcut(uint32_t now)
{
  static uint8_t armed = 0U;
  static uint8_t holding = 0U;
  static uint32_t hold_start = 0U;

  const uint8_t fn_down = Keyboard_KeyDown(KEY_FN_MCP_ADDR, KEY_FN_ROW, KEY_FN_COL);
  const uint8_t backspace_down = Keyboard_KeyDown(KEY_BACKSPACE_MCP_ADDR,
                                                  KEY_BACKSPACE_ROW, KEY_BACKSPACE_COL);

  /* Do not repeatedly reboot if both keys remain held after a reset.
   * The shortcut is rearmed once either key has been released.
   */
  if ((fn_down == 0U) || (backspace_down == 0U))
  {
    armed = 1U;
    holding = 0U;
    return;
  }

  if (armed == 0U) return;

  if (holding == 0U)
  {
    hold_start = now;
    holding = 1U;
  }
  else if ((uint32_t)(now - hold_start) >= KEY_REBOOT_HOLD_MS)
  {
    printf("[KEYBOARD] Fn+Backspace held 3s: software reboot\r\n");
    HAL_Delay(25U);
    NVIC_SystemReset();
  }
}

static void Keyboard_BuildReport(void)
{
  uint8_t report_index = 2U;
  const uint8_t fn_down = Keyboard_KeyDown(KEY_FN_MCP_ADDR, KEY_FN_ROW, KEY_FN_COL);
  memset(g_hid_report, 0, sizeof(g_hid_report));

  for (uint8_t i = 0U; i < KEY_COUNT; i++)
  {
    if (g_key_state[i] == 0U) continue;
    /* Fn+Backspace must not delete text while waiting for the reboot. */
    if ((fn_down != 0U) &&
        (g_keymap[i].addr == KEY_BACKSPACE_MCP_ADDR) &&
        (g_keymap[i].row == KEY_BACKSPACE_ROW) &&
        (g_keymap[i].col == KEY_BACKSPACE_COL))
      continue;
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

      if (MCP23008_ColumnRead(addr, outputs, &gpio) == 0U)
      {
        g_mcp_ready[dev] = 0U;
        break;
      }

      /* Log physical switch edges on all six MCPs, even for keys without
       * entries in g_keymap (e.g. an Fn switch whose wiring is unverified).
       * A row is active low; GP3=R0, GP2=R1, GP1=R2, GP0=R3.
       */
      uint8_t rows_now = (uint8_t)(gpio & 0x0FU);
      uint8_t changed = (uint8_t)(g_matrix_prev_rbits[dev][col] ^ rows_now);
      g_scan_raw_rbits[dev][col] = rows_now;
      g_matrix_prev_rbits[dev][col] = rows_now;
      if (changed != 0U)
      {
        for (uint8_t row = 0U; row < 4U; row++)
        {
          uint8_t bit = (uint8_t)(1U << (3U - row));
          if ((changed & bit) != 0U)
          {
            printf("[MATRIX] %s MCP=0x%02X R%u C%u (GP%u)\r\n",
                   ((rows_now & bit) == 0U) ? "DOWN" : "UP",
                   (unsigned int)addr, (unsigned int)row,
                   (unsigned int)col, (unsigned int)(3U - row));
          }
        }
      }
      for (uint8_t i = 0U; i < KEY_COUNT; i++)
      {
        if ((g_keymap[i].addr == addr) && (g_keymap[i].col == col))
        {
          uint8_t row_bit = (uint8_t)(3U - g_keymap[i].row);
          raw_pressed[i] = ((gpio & (uint8_t)(1U << row_bit)) == 0U) ? 1U : 0U;
        }
      }
    }

    if ((g_mcp_ready[dev] != 0U) &&
        (HwI2C_WriteReg(addr, MCP23008_REG_OLAT, MCP_COL_OUT_MASK) == 0U))
      g_mcp_ready[dev] = 0U;
  }

  (void)MCP23008_CountReady();

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
  if (USBD_HID_SendReport(&hUsbDeviceFS, g_hid_report, sizeof(g_hid_report)) == USBD_OK)
  {
    g_report_pending = 0U;
    ++g_metrics_report_count; /* HID IN transfer submitted, not completed. */
  }
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

/* Standard RGB color wheel: 0..255 -> red/green/blue. */
static void LED6028_ColorWheel(uint8_t hue, uint8_t *red, uint8_t *green, uint8_t *blue)
{
  if (hue < 85U)
  {
    *red = (uint8_t)(255U - hue * 3U);
    *green = (uint8_t)(hue * 3U);
    *blue = 0U;
  }
  else if (hue < 170U)
  {
    hue = (uint8_t)(hue - 85U);
    *red = 0U;
    *green = (uint8_t)(255U - hue * 3U);
    *blue = (uint8_t)(hue * 3U);
  }
  else
  {
    hue = (uint8_t)(hue - 170U);
    *red = (uint8_t)(hue * 3U);
    *green = 0U;
    *blue = (uint8_t)(255U - hue * 3U);
  }
}

static void Keyboard_UpdateLEDs(uint32_t now)
{
  /* Diagnostics bypass both RGB animation and press highlights, so the
   * selected physical address is always the ONLY illuminated LED.
   */
  if (g_rgb_test_mode != 0U)
  {
    if (g_rgb_test_mode == 1U)
    {
      LED6028_SetAll(0U, 0U, 0U);
      LED6028_SetPixel(g_rgb_test_index,
                       g_rgb_test_red, g_rgb_test_green, g_rgb_test_blue);
    }
    else
    {
      LED6028_SetAll(g_rgb_test_red, g_rgb_test_green, g_rgb_test_blue);
    }
    LED6028_Show();
    return;
  }

  uint8_t scale = g_rgb_brightness;
  uint8_t red, green, blue;

  if (g_rgb_mode == 0U)
    scale = 0U;
  else if (g_rgb_mode == 1U)
  {
    uint32_t period = (LED6028_BREATH_PERIOD_MS * 5U) / g_rgb_speed;
    float phase = (float)(now % period) / (float)period;
    float breath = 0.5f - 0.5f * cosf(2.0f * 3.1415926f * phase);
    scale = (uint8_t)((float)scale * breath);
  }

  if ((g_rgb_mode == 3U) || (g_rgb_mode == 4U)) /* Spatial rainbow */
  {
    /* A pattern hue = position + phase travels toward smaller coordinates.
     * Use position - phase so mode 3 travels LEFT->RIGHT and mode 4
     * travels TOP->BOTTOM, regardless of the physical PCB wiring order.
     * Division before multiplication avoids 32-bit now*speed overflow.
     */
    const uint8_t phase = (uint8_t)((now / 80U) * g_rgb_speed);
    for (uint8_t i = 0U; i < LED6028_COUNT; i++)
    {
      uint8_t hue;
      if (i < LED6028_SPATIAL_COUNT)
      {
        const LED6028_Position *pos = &g_led6028_xy[i];
        /* One coherent wavelength across the keyboard's visible width/
         * height. No discontinuity at the four PCB boundaries.
         */
        hue = (g_rgb_mode == 3U)
          ? (uint8_t)(((uint16_t)pos->x * 256U) / LED6028_SPATIAL_X_PERIOD - phase)
          : (uint8_t)(((uint16_t)pos->y * 256U) / LED6028_SPATIAL_Y_PERIOD - phase);
      }
      else
      {
        /* Unmapped LED88..99: leave working
         * with the previous wire-index rainbow until their physical layout
         * is verified in hardware.
         */
        hue = (uint8_t)(((uint32_t)i * 256U / LED6028_COUNT) + phase);
      }
      LED6028_ColorWheel(hue, &red, &green, &blue);
      LED6028_SetPixel(i, (uint8_t)(((uint16_t)red * scale) / 255U),
                         (uint8_t)(((uint16_t)green * scale) / 255U),
                         (uint8_t)(((uint16_t)blue * scale) / 255U));
    }
  }
  else /* Off, breathing or solid */
  {
    red = (uint8_t)(((uint16_t)g_rgb_red * scale) / 255U);
    green = (uint8_t)(((uint16_t)g_rgb_green * scale) / 255U);
    blue = (uint8_t)(((uint16_t)g_rgb_blue * scale) / 255U);
    LED6028_SetAll(red, green, blue);
  }

  /* Preserve the ten existing direction-board press highlights. */
  if (g_rgb_mode != 0U)
  {
    for (uint8_t i = 0U; i < KEY_COUNT; i++)
    {
      if ((g_key_state[i] != 0U) && (g_keymap[i].led != LED_NONE))
        LED6028_SetPixel(g_keymap[i].led, LED6028_PRESSED_LEVEL,
                        LED6028_PRESSED_LEVEL, LED6028_PRESSED_LEVEL);
    }
  }
  LED6028_Show();
}

static void LED6028_BreathService(uint32_t now)
{
  static uint32_t last_update = 0U;
  if ((uint32_t)(now - last_update) < LED6028_BREATH_UPDATE_MS) return;
  last_update = now;
  Keyboard_UpdateLEDs(now);
}
/* USER CODE END 0 */

int main(void)
{
  uint32_t last_scan_tick = 0U;
  uint32_t last_log_tick = 0U;
  uint32_t last_mcp_retry_tick = 0U;
  uint32_t last_mcp_poll_tick = 0U;
  uint32_t last_metrics_tick = 0U;
  int frame = 0;

  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  RGB_StoreLoad();  /* Restore saved RGB settings before any LED update. */
  /* R14 is a fixed 1.5k pull-up from PA12 (HID_DP) to 3.3V. A firmware
   * reboot / ST-Link flash does not remove that pull-up, so Windows may
   * retain the previous USB connection. Generate a real D+ low interval
   * before starting the USB peripheral, without resetting other GPIOs.
   */
  USB_ForceReenumeration();
  MX_USB_DEVICE_Init();

  CycleCounter_Init();
  HwI2C_Init();
  Keyboard_Init();
  LED6028_Init();
  HAL_Delay(100U);

  /* Enumerate all possible MCP23008 addresses before configuring the boards. */
  MCP23008_ScanBus();
  (void)MCP23008_InitAll();
  MCP23008_PollAll();

  printf("\r\n========================================\r\n");
  printf("MultiPad 6-board keyboard start\r\n");
  printf("MCU              : STM32F103C8T6\r\n");
  printf("SystemCoreClock  : %lu Hz\r\n", (unsigned long)SystemCoreClock);
  printf("USB              : PA11 D- / PA12 D+\r\n");
  printf("MCP23008         : 0x20 direction, 0x23 left1, 0x24 left2, 0x22 left3, 0x21 left4, 0x27 extra\r\n");
  printf("MCP ready        : %u/%u\r\n", (unsigned int)g_mcp_found_count, (unsigned int)MCP23008_COUNT);
  printf("I2C transport    : %s, %lu Hz\r\n",
         g_i2c_software_fallback ? "SOFTWARE FALLBACK (internal PU)" : "HARDWARE I2C1",
         g_i2c_software_fallback ? 0UL : (unsigned long)hi2c1.Init.ClockSpeed);
  printf("Keys             : %u mapped keys\r\n", (unsigned int)KEY_COUNT);
  printf("RGB              : PB6 GPIO bit-bang, %u LEDs\r\n", (unsigned int)LED6028_COUNT);
  printf("========================================\r\n");

  last_metrics_tick = HAL_GetTick();
  while (1)
  {
    const uint32_t active_start = DWT->CYCCNT;
    uint32_t now = HAL_GetTick();
    frame++;

    if ((uint32_t)(now - last_scan_tick) >= KEY_SCAN_PERIOD_MS)
    {
      last_scan_tick = now;
      const uint32_t scan_start = DWT->CYCCNT;
      Keyboard_Scan();
      const uint32_t scan_cycles = DWT->CYCCNT - scan_start;
      ++g_metrics_scan_count;
      g_metrics_scan_cycles += scan_cycles;
      if (scan_cycles > g_metrics_max_scan_cycles)
        g_metrics_max_scan_cycles = scan_cycles;
      Keyboard_RebootShortcut(now);
    }

    Keyboard_USBService();
    WebHID_Service();
    LED6028_BreathService(now);

    if ((uint32_t)(now - last_mcp_poll_tick) >= MCP23008_POLL_PERIOD_MS)
    {
      last_mcp_poll_tick = now;
      MCP23008_PollAll();
    }

    if ((g_mcp_found_count < MCP23008_COUNT) && ((uint32_t)(now - last_mcp_retry_tick) >= 1000U))
    {
      last_mcp_retry_tick = now;
      printf("[I2C] MCP online=%u/%u, recovering bus (PB8 SCL=%u, PB9 SDA=%u)\r\n",
             (unsigned int)g_mcp_found_count, (unsigned int)MCP23008_COUNT,
             (unsigned int)((GPIOB->IDR & HW_I2C_SCL_PIN) != 0U),
             (unsigned int)((GPIOB->IDR & HW_I2C_SDA_PIN) != 0U));
      HwI2C_Init();
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

    /* Count foreground CPU cycles and let Cortex-M3 idle until the next
     * SysTick/USB interrupt. No clock frequency or USB timers are changed. */
    g_metrics_busy_cycles += (uint32_t)(DWT->CYCCNT - active_start);
    const uint32_t metrics_now = HAL_GetTick();
    const uint32_t sample_ms = metrics_now - last_metrics_tick;
    if (sample_ms >= METRICS_SAMPLE_PERIOD_MS)
    {
      Metrics_FinishWindow(sample_ms);
      last_metrics_tick = metrics_now;
    }
    __WFI();
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

/* STM32F103 USB Full Speed: PA12 = D+.
 * Direction-board schematic: R14 (1.5k) connects HID_DP to VCC3V3.
 * Since the pull-up is always powered, an MCU reset / SWD programming
 * sequence need not produce a physical USB detach.  Override the pull-up
 * with PA12 push-pull LOW for 150ms (~2.2mA through R14), release it,
 * and immediately let STM32Cube take ownership of USB pins.
 *
 * Only call before MX_USB_DEVICE_Init(); NEVER while the USB peripheral
 * is active.  This does not depend on browser state or RGB mode.
 */
static void USB_ForceReenumeration(void)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET);
  gpio.Pin = GPIO_PIN_12;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &gpio);

  /* Let the host/hub observe a stable USB detach, even on SWD reset. */
  HAL_Delay(150U);

  /* Return PA12 to its reset/default configuration. The hardware 1.5k
   * pull-up restores D+ HIGH; MX_USB_DEVICE_Init() starts the USB IP.
   */
  HAL_GPIO_DeInit(GPIOA, GPIO_PIN_12);
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

  HAL_GPIO_WritePin(GPIOB, HW_I2C_SCL_PIN | HW_I2C_SDA_PIN, GPIO_PIN_SET);
  GPIO_InitStruct.Pin = HW_I2C_SCL_PIN | HW_I2C_SDA_PIN;
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
