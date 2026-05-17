/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Camera light meter top main program
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ssd1306_i2c.h"
#include "veml7700.h"
#include "vl53l1x_hal.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum {
  MODE_TV = 0,   /* shutter priority */
  MODE_AV = 1    /* aperture priority */
} MeterMode;

typedef enum {
  METER_OK = 0,
  METER_UNDER,
  METER_OVER,
  METER_SENSOR_ERR
} MeterHint;

typedef struct {
  GPIO_TypeDef *port;
  uint16_t pin;
  GPIO_PinState stable;
  GPIO_PinState last_read;
  uint32_t last_change_ms;
  uint32_t press_start_ms;
  uint8_t ever_pressed;       /* GPIO keys cannot be probed; becomes OK after first valid press. */
  uint8_t long_reported;
} ButtonState;

typedef enum {
  BUTTON_EVENT_NONE = 0,
  BUTTON_EVENT_SHORT,
  BUTTON_EVENT_LONG
} ButtonEvent;

typedef enum {
  SCREEN_SELF_TEST = 0,
  SCREEN_METER = 1
} AppScreen;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define ARRAY_LEN(a)              (sizeof(a) / sizeof((a)[0]))
#define ENCODER_STEP              4
#define KEY_PRESSED               GPIO_PIN_RESET
#define KEY_RELEASED              GPIO_PIN_SET
#define KEY_DEBOUNCE_MS           35U
#define SENSOR_PERIOD_MS          400U
#define UI_PERIOD_MS              180U
#define RANGE_RESTART_MS          1000U
#define RANGE_POLL_MS             120U
#define RANGE_FORCE_ALWAYS_ON     1U
#define DISPLAY_IIR_SHIFT         2U
#define DISPLAY_RANGE_IIR_SHIFT   3U
#define DISPLAY_LUX_STABLE_X10    30UL
#define DISPLAY_RANGE_STABLE_MM   20U
#define DISPLAY_RANGE_DEADBAND_MM 20U
#define DISPLAY_RANGE_STEP_MM     50U
#define RANGE_XSHUT_GPIO_Port     GPIOC
#define RANGE_XSHUT_Pin           GPIO_PIN_13
#define RANGE_INT_Pin             GPIO_PIN_14
#define SELF_TEST_HOLD_MS         5000U  /* Long press encoder button to enter self-test. */
#define SELF_TEST_SHOW_MS         6000U  /* Boot/long-press self-test screen display time. */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static VEML7700_HandleTypeDef hveml;
static VL53L1X_HandleTypeDef hvl53;
static ButtonState btn_mode;
static ButtonState btn_iso;

static const uint16_t shutter_den_tbl[] = {1, 2, 4, 8, 15, 30, 60, 125, 250, 500};
static const uint16_t aperture_x10_tbl[] = {28, 35, 40, 47, 56, 80, 110, 160, 220, 320, 450};
static const uint16_t iso_tbl[] = {100, 125, 160, 200, 400};

static MeterMode g_mode = MODE_TV;
static MeterHint g_hint = METER_SENSOR_ERR;
static uint8_t g_shutter_idx = 6;   /* 1/60 */
static uint8_t g_aperture_idx = 4;  /* f/5.6 */
static uint8_t g_iso_idx = 0;
static uint32_t g_lux_x10 = 0;
static int16_t g_ev_x10 = 0;
static uint16_t g_bat_mv = 0;
static uint8_t g_bat_pct = 0;
static uint8_t g_sensor_ok = 0;
static uint8_t g_lux_disp_valid = 0U;
static uint8_t g_lux_disp_stable = 0U;
static uint32_t g_lux_disp_x10 = 0U;
static uint8_t g_encoder_seen = 0U;
static uint8_t g_button_seen = 0U;
static uint32_t g_app_start_ms = 0U;
static AppScreen g_screen = SCREEN_SELF_TEST;
static uint32_t g_selftest_start_ms = 0U;
static uint8_t g_selftest_boot = 1U;
static uint8_t g_range_needed = 0U;
static uint8_t g_range_active = 0U;
static uint8_t g_range_ok = 0U;
static uint8_t g_range_read_seen = 0U;
static uint8_t g_range_disp_valid = 0U;
static uint8_t g_range_disp_stable = 0U;
static uint16_t g_range_mm = 0U;
static uint16_t g_range_disp_mm = 0U;
static uint8_t g_range_status = 0xFFU;
static uint32_t g_range_retry_ms = 0U;
static uint32_t g_range_last_poll_ms = 0U;
static volatile uint8_t g_vl53_int_flag = 0U;
static volatile uint8_t g_i2c1_busy = 0U;
static int16_t enc_last = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void App_Init(void);
static void App_Task(void);
static void Process_Input(void);
static void Update_Meter(void);
static void Range_Task(void);
static void Range_SetNeeded(uint8_t needed);
static uint8_t Range_IsNeeded(void);
static const char *Range_DebugText(void);
static void DisplayFilter_UpdateLux(uint32_t lux_x10);
static void DisplayFilter_UpdateRange(uint16_t range_mm);
static uint32_t IIR_UpdateU32(uint32_t current, uint32_t target);
static uint32_t IIR_UpdateU32Shift(uint32_t current, uint32_t target, uint8_t shift);
static uint32_t AbsDiffU32(uint32_t a, uint32_t b);
static uint8_t I2C1_TryLock(void);
static void I2C1_Unlock(void);
static void Meter_Recalculate(void);
static int8_t Encoder_ReadDetents(void);
static ButtonEvent Button_Update(ButtonState *btn, uint32_t long_ms);
static void SelfTest_Start(uint8_t boot_start);
static void SelfTest_Task(void);
static void UI_DrawSelfTest(void);
static void UI_DrawBottomStatusBar(void);
static uint16_t Battery_ReadMv(void);
static uint8_t Battery_Percent(uint16_t mv);
static uint8_t FindNearestShutter(float target_s);
static uint8_t FindNearestApertureByN2(float target_n2);
static int16_t EV10_FromLuxIso(uint32_t lux_x10, uint16_t iso);
static void Update_Display(void);
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

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_I2C1_Init();
  MX_TIM2_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  App_Init();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    App_Task();
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

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV2;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
static void App_Init(void)
{
  g_app_start_ms = HAL_GetTick();

  btn_mode.port = GPIOB;
  btn_mode.pin = GPIO_PIN_0;
  btn_mode.stable = KEY_RELEASED;
  btn_mode.last_read = KEY_RELEASED;
  btn_mode.last_change_ms = HAL_GetTick();
  btn_mode.press_start_ms = 0U;
  btn_mode.ever_pressed = 0U;
  btn_mode.long_reported = 0U;

  btn_iso.port = GPIOB;
  btn_iso.pin = GPIO_PIN_1;
  btn_iso.stable = KEY_RELEASED;
  btn_iso.last_read = KEY_RELEASED;
  btn_iso.last_change_ms = HAL_GetTick();
  btn_iso.press_start_ms = 0U;
  btn_iso.ever_pressed = 0U;
  btn_iso.long_reported = 0U;

  if (HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL) == HAL_OK) {
    enc_last = (int16_t)__HAL_TIM_GET_COUNTER(&htim2);
  } else {
    g_encoder_seen = 0U;
  }
  HAL_ADC_Start(&hadc1);

  /* XSHUT must already be configured as PC13 output-high by MX_GPIO_Init().
     Keep it high before the first VL53L1X I2C register access. */
  HAL_GPIO_WritePin(RANGE_XSHUT_GPIO_Port, RANGE_XSHUT_Pin, GPIO_PIN_SET);
  (void)VL53L1X_Attach(&hvl53, &hi2c1, RANGE_XSHUT_GPIO_Port, RANGE_XSHUT_Pin);

  if (I2C1_TryLock()) {
    if (SSD1306_Init(&hi2c1) == HAL_OK) {
      SSD1306_Clear();
      SSD1306_DrawString(0, 0, "Light Meter Top");
      SSD1306_DrawString(0, 2, "Init VEML7700...");
      SSD1306_Update();
    }
    I2C1_Unlock();
  }

  if (I2C1_TryLock()) {
    if (VEML7700_Init(&hveml, &hi2c1) != HAL_OK) {
      g_sensor_ok = 0;
      g_hint = METER_SENSOR_ERR;
    } else {
      g_sensor_ok = 1;
      g_hint = METER_OK;
    }
    I2C1_Unlock();
  }

  Range_SetNeeded(Range_IsNeeded());
  Range_Task();

  HAL_Delay(150);
  Update_Meter();
  SelfTest_Start(1U);
  Update_Display();
}

static void App_Task(void)
{
  static uint32_t last_sensor_ms = 0;
  static uint32_t last_ui_ms = 0;
  uint32_t now = HAL_GetTick();

  Process_Input();
  Range_Task();
  SelfTest_Task();

  if ((now - last_sensor_ms) >= SENSOR_PERIOD_MS) {
    last_sensor_ms = now;
    Update_Meter();
  }

  if ((now - last_ui_ms) >= UI_PERIOD_MS) {
    last_ui_ms = now;
    Update_Display();
  }
}

static void Process_Input(void)
{
  int8_t detents = Encoder_ReadDetents();
  ButtonEvent encoder_button_event = Button_Update(&btn_mode, SELF_TEST_HOLD_MS);
  ButtonEvent iso_button_event = Button_Update(&btn_iso, 0U);

  if (detents != 0) {
    g_encoder_seen = 1U;

    /* In self-test screen, rotation is only used to verify the encoder. */
    if (g_screen != SCREEN_SELF_TEST) {
      if (g_mode == MODE_TV) {
        int16_t idx = (int16_t)g_shutter_idx + detents;
        if (idx < 0) idx = 0;
        if (idx >= (int16_t)ARRAY_LEN(shutter_den_tbl)) idx = (int16_t)ARRAY_LEN(shutter_den_tbl) - 1;
        g_shutter_idx = (uint8_t)idx;
      } else {
        int16_t idx = (int16_t)g_aperture_idx + detents;
        if (idx < 0) idx = 0;
        if (idx >= (int16_t)ARRAY_LEN(aperture_x10_tbl)) idx = (int16_t)ARRAY_LEN(aperture_x10_tbl) - 1;
        g_aperture_idx = (uint8_t)idx;
      }
      Meter_Recalculate();
    }

    Update_Display();
  }

  if (encoder_button_event == BUTTON_EVENT_LONG) {
    /* Current IOC has no separate encoder-SW pin. PB0 is treated as encoder push.
       Short press keeps the original Mode action; 5s long press opens self-test. */
    SelfTest_Start(0U);
    Update_Display();
  } else if ((encoder_button_event == BUTTON_EVENT_SHORT) && (g_screen != SCREEN_SELF_TEST)) {
    g_mode = (g_mode == MODE_TV) ? MODE_AV : MODE_TV;
    Meter_Recalculate();
    Update_Display();
  }

  if ((iso_button_event == BUTTON_EVENT_SHORT) && (g_screen != SCREEN_SELF_TEST)) {
    g_iso_idx++;
    if (g_iso_idx >= ARRAY_LEN(iso_tbl)) g_iso_idx = 0;
    Meter_Recalculate();
    Update_Display();
  }
}

static void Update_Meter(void)
{
  uint32_t lux_x10;

  if (I2C1_TryLock()) {
    if (VEML7700_ReadLuxAutoX10(&hveml, &lux_x10) == HAL_OK) {
      g_lux_x10 = lux_x10;
      g_sensor_ok = 1;
      DisplayFilter_UpdateLux(lux_x10);
    } else {
      g_sensor_ok = 0;
      g_lux_disp_valid = 0U;
      g_hint = METER_SENSOR_ERR;
    }
    I2C1_Unlock();
  }

  g_bat_mv = Battery_ReadMv();
  g_bat_pct = Battery_Percent(g_bat_mv);
  Meter_Recalculate();
}

static void Range_Task(void)
{
  uint32_t now = HAL_GetTick();

  Range_SetNeeded(Range_IsNeeded());
  HAL_GPIO_WritePin(RANGE_XSHUT_GPIO_Port, RANGE_XSHUT_Pin, GPIO_PIN_SET);

  if (g_range_needed == 0U) {
    return;
  }

  if (g_range_active == 0U) {
    if ((g_range_retry_ms != 0U) && ((now - g_range_retry_ms) < RANGE_RESTART_MS)) {
      return;
    }

    if (I2C1_TryLock()) {
      HAL_StatusTypeDef init_ret = VL53L1X_PowerOnInit(&hvl53);
      HAL_StatusTypeDef start_ret = HAL_ERROR;
      if (init_ret == HAL_OK) {
        start_ret = VL53L1X_StartRanging(&hvl53);
      }
      if ((init_ret == HAL_OK) && (start_ret == HAL_OK)) {
        g_range_active = 1U;
        g_range_ok = 0U;
        g_range_read_seen = 0U;
        g_vl53_int_flag = 0U;
        g_range_last_poll_ms = now;
      } else {
        if (init_ret == HAL_OK) {
          hvl53.last_error_stage = VL53L1X_STAGE_START;
          hvl53.last_hal_status = start_ret;
        }
        g_range_active = 0U;
        g_range_ok = 0U;
        g_range_retry_ms = now;
#if (RANGE_FORCE_ALWAYS_ON != 0U)
        HAL_GPIO_WritePin(RANGE_XSHUT_GPIO_Port, RANGE_XSHUT_Pin, GPIO_PIN_SET);
#else
        VL53L1X_PowerOff(&hvl53);
#endif
      }
      I2C1_Unlock();
    }
    return;
  }

  if ((g_vl53_int_flag != 0U) ||
      ((now - g_range_last_poll_ms) >= RANGE_POLL_MS)) {
    uint16_t distance = 0U;
    uint8_t status = 0xFFU;

    if (I2C1_TryLock()) {
      g_vl53_int_flag = 0U;
      g_range_last_poll_ms = now;
      if (VL53L1X_ReadAndClear(&hvl53, &distance, &status) == HAL_OK) {
        g_range_mm = distance;
        g_range_status = status;
        g_range_read_seen = 1U;
        g_range_ok = (status == 0U) ? 1U : 0U;
        if (g_range_ok != 0U) {
          DisplayFilter_UpdateRange(distance);
        } else {
          g_range_disp_stable = 0U;
        }
      } else {
        hvl53.last_error_stage = VL53L1X_STAGE_READ;
        g_range_ok = 0U;
        g_range_active = 0U;
        g_range_retry_ms = now;
#if (RANGE_FORCE_ALWAYS_ON != 0U)
        HAL_GPIO_WritePin(RANGE_XSHUT_GPIO_Port, RANGE_XSHUT_Pin, GPIO_PIN_SET);
#else
        VL53L1X_PowerOff(&hvl53);
#endif
      }
      I2C1_Unlock();
    }
  }
}

static void Range_SetNeeded(uint8_t needed)
{
#if (RANGE_FORCE_ALWAYS_ON != 0U)
  needed = 1U;
  HAL_GPIO_WritePin(RANGE_XSHUT_GPIO_Port, RANGE_XSHUT_Pin, GPIO_PIN_SET);
#endif
  needed = (needed != 0U) ? 1U : 0U;

  if (needed == g_range_needed) {
    return;
  }

  g_range_needed = needed;

  if (g_range_needed == 0U) {
#if (RANGE_FORCE_ALWAYS_ON != 0U)
    HAL_GPIO_WritePin(RANGE_XSHUT_GPIO_Port, RANGE_XSHUT_Pin, GPIO_PIN_SET);
#else
    if (I2C1_TryLock()) {
      VL53L1X_PowerOff(&hvl53);
      I2C1_Unlock();
    } else {
      HAL_GPIO_WritePin(RANGE_XSHUT_GPIO_Port, RANGE_XSHUT_Pin, GPIO_PIN_RESET);
    }
#endif
    g_range_active = 0U;
    g_range_ok = 0U;
    g_range_read_seen = 0U;
    g_range_disp_valid = 0U;
    g_vl53_int_flag = 0U;
  }
}

static uint8_t Range_IsNeeded(void)
{
  /* Self-test needs range powered so it can verify VL53L1X presence.
     Replace the normal return value with the actual UI/standby condition later. */
  if (g_screen == SCREEN_SELF_TEST) {
    return 1U;
  }
  return 1U;
}

static const char *Range_DebugText(void)
{
  switch (hvl53.last_error_stage) {
    case VL53L1X_STAGE_BOOT:
      return "BOOT";
    case VL53L1X_STAGE_ID:
      return "ID";
    case VL53L1X_STAGE_CFG:
      return "CFG";
    case VL53L1X_STAGE_INT:
      return "INT";
    case VL53L1X_STAGE_CLR:
      return "CLR";
    case VL53L1X_STAGE_START:
      return "START";
    case VL53L1X_STAGE_READ:
      return "READ";
    default:
      return "TRY";
  }
}

static void DisplayFilter_UpdateLux(uint32_t lux_x10)
{
  if (g_lux_disp_valid == 0U) {
    g_lux_disp_x10 = lux_x10;
    g_lux_disp_valid = 1U;
  } else {
    g_lux_disp_x10 = IIR_UpdateU32(g_lux_disp_x10, lux_x10);
  }

  g_lux_disp_stable =
      (AbsDiffU32(g_lux_disp_x10, lux_x10) <= DISPLAY_LUX_STABLE_X10) ? 1U : 0U;
}

static void DisplayFilter_UpdateRange(uint16_t range_mm)
{
  uint32_t current;
  uint32_t target;
  uint32_t diff;

  if (g_range_disp_valid == 0U) {
    g_range_disp_mm = range_mm;
    g_range_disp_valid = 1U;
  } else {
    current = (uint32_t)g_range_disp_mm;
    target = (uint32_t)range_mm;
    diff = AbsDiffU32(current, target);

    if (diff > DISPLAY_RANGE_DEADBAND_MM) {
      if (diff > DISPLAY_RANGE_STEP_MM) {
        target = (target > current) ? (current + DISPLAY_RANGE_STEP_MM)
                                   : (current - DISPLAY_RANGE_STEP_MM);
      }
      g_range_disp_mm = (uint16_t)IIR_UpdateU32Shift(current, target, DISPLAY_RANGE_IIR_SHIFT);
    }
  }

  g_range_disp_stable =
      (AbsDiffU32((uint32_t)g_range_disp_mm, (uint32_t)range_mm) <= DISPLAY_RANGE_STABLE_MM) ? 1U : 0U;
}

static uint32_t IIR_UpdateU32(uint32_t current, uint32_t target)
{
  return IIR_UpdateU32Shift(current, target, DISPLAY_IIR_SHIFT);
}

static uint32_t IIR_UpdateU32Shift(uint32_t current, uint32_t target, uint8_t shift)
{
  uint32_t delta;
  uint32_t step;

  if (target == current) {
    return current;
  }

  if (target > current) {
    delta = target - current;
    step = delta >> shift;
    if (step == 0U) step = 1U;
    return current + step;
  }

  delta = current - target;
  step = delta >> shift;
  if (step == 0U) step = 1U;
  return current - step;
}

static uint32_t AbsDiffU32(uint32_t a, uint32_t b)
{
  return (a >= b) ? (a - b) : (b - a);
}

static uint8_t I2C1_TryLock(void)
{
  uint8_t ok = 0U;

  __disable_irq();
  if (g_i2c1_busy == 0U) {
    g_i2c1_busy = 1U;
    ok = 1U;
  }
  __enable_irq();

  return ok;
}

static void I2C1_Unlock(void)
{
  __disable_irq();
  g_i2c1_busy = 0U;
  __enable_irq();
}

static void Meter_Recalculate(void)
{
  uint16_t iso = iso_tbl[g_iso_idx];
  float lux = (float)g_lux_x10 / 10.0f;
  float light_power;

  g_ev_x10 = EV10_FromLuxIso(g_lux_x10, iso);

  if (g_sensor_ok == 0U) {
    g_hint = METER_SENSOR_ERR;
    return;
  }
  if (g_lux_x10 == 0U) {
    g_hint = METER_UNDER;
    return;
  }

  /* Incident meter approximation: EV100 = log2(lux / 2.5).
     Therefore N^2 / t = lux * ISO / 250. */
  light_power = lux * ((float)iso / 250.0f);

  if (g_mode == MODE_AV) {
    float n = (float)aperture_x10_tbl[g_aperture_idx] / 10.0f;
    float target_s = (n * n) / light_power;

    if (target_s > (1.0f / (float)shutter_den_tbl[0])) {
      g_hint = METER_UNDER;
    } else if (target_s < (1.0f / (float)shutter_den_tbl[ARRAY_LEN(shutter_den_tbl) - 1])) {
      g_hint = METER_OVER;
    } else {
      g_hint = METER_OK;
    }
    g_shutter_idx = FindNearestShutter(target_s);
  } else {
    float t = 1.0f / (float)shutter_den_tbl[g_shutter_idx];
    float target_n2 = t * light_power;
    float min_n = (float)aperture_x10_tbl[0] / 10.0f;
    float max_n = (float)aperture_x10_tbl[ARRAY_LEN(aperture_x10_tbl) - 1] / 10.0f;

    if (target_n2 < (min_n * min_n)) {
      g_hint = METER_UNDER;
    } else if (target_n2 > (max_n * max_n)) {
      g_hint = METER_OVER;
    } else {
      g_hint = METER_OK;
    }
    g_aperture_idx = FindNearestApertureByN2(target_n2);
  }
}

static int8_t Encoder_ReadDetents(void)
{
  int16_t now = (int16_t)__HAL_TIM_GET_COUNTER(&htim2);
  int16_t diff = (int16_t)(now - enc_last);
  int8_t detents = 0;

  if (diff >= ENCODER_STEP) {
    detents = (int8_t)(diff / ENCODER_STEP);
    enc_last = (int16_t)(enc_last + detents * ENCODER_STEP);
  } else if (diff <= -ENCODER_STEP) {
    detents = (int8_t)(diff / ENCODER_STEP);
    enc_last = (int16_t)(enc_last + detents * ENCODER_STEP);
  }
  return detents;
}

static ButtonEvent Button_Update(ButtonState *btn, uint32_t long_ms)
{
  GPIO_PinState read = HAL_GPIO_ReadPin(btn->port, btn->pin);
  uint32_t now = HAL_GetTick();

  if (read != btn->last_read) {
    btn->last_read = read;
    btn->last_change_ms = now;
  }

  if ((now - btn->last_change_ms) >= KEY_DEBOUNCE_MS) {
    if (read != btn->stable) {
      btn->stable = read;

      if (btn->stable == KEY_PRESSED) {
        btn->ever_pressed = 1U;
        g_button_seen = 1U;
        btn->press_start_ms = now;
        btn->long_reported = 0U;
      } else {
        if ((btn->long_reported == 0U) && (btn->press_start_ms != 0U)) {
          btn->press_start_ms = 0U;
          return BUTTON_EVENT_SHORT;
        }
        btn->press_start_ms = 0U;
      }
    }
  }

  if ((long_ms != 0U) &&
      (btn->stable == KEY_PRESSED) &&
      (btn->press_start_ms != 0U) &&
      (btn->long_reported == 0U) &&
      ((now - btn->press_start_ms) >= long_ms)) {
    btn->long_reported = 1U;
    return BUTTON_EVENT_LONG;
  }

  return BUTTON_EVENT_NONE;
}

static void SelfTest_Start(uint8_t boot_start)
{
  g_screen = SCREEN_SELF_TEST;
  g_selftest_start_ms = HAL_GetTick();
  g_selftest_boot = (boot_start != 0U) ? 1U : 0U;

  /* Start one non-blocking round for I2C devices. Passive GPIO inputs
     (encoder/buttons) can only become OK after the user operates them. */
  Range_SetNeeded(1U);
  Range_Task();
  Update_Meter();
}

static void SelfTest_Task(void)
{
  if (g_screen != SCREEN_SELF_TEST) {
    return;
  }

  /* Show one result page, then automatically return to the normal meter UI. */
  if ((HAL_GetTick() - g_selftest_start_ms) >= SELF_TEST_SHOW_MS) {
    g_screen = SCREEN_METER;
    g_selftest_boot = 0U;
    Update_Display();
  }
}

static void UI_DrawSelfTest(void)
{
  char line[32];

  SSD1306_Clear();
  SSD1306_DrawString(18U, 0U, g_selftest_boot ? "BOOT SELF TEST" : "SELF TEST");
  SSD1306_DrawHLine(0U, 11U, 128U, 1U);

  snprintf(line, sizeof(line), "ENC : %s", (g_encoder_seen != 0U) ? "OK" : "TURN");
  SSD1306_DrawString(0U, 2U, line);

  snprintf(line, sizeof(line), "MODE: %s", (btn_mode.ever_pressed != 0U) ? "OK" : "PRESS");
  SSD1306_DrawString(0U, 3U, line);

  snprintf(line, sizeof(line), "ISO : %s", (btn_iso.ever_pressed != 0U) ? "OK" : "PRESS");
  SSD1306_DrawString(0U, 4U, line);

  snprintf(line, sizeof(line), "LUX : %s", (g_sensor_ok != 0U) ? "OK" : "OFF");
  SSD1306_DrawString(0U, 5U, line);

  if (g_range_ok != 0U) {
    snprintf(line, sizeof(line), "DIST: OK %ucm", (unsigned int)(g_range_mm / 10U));
  } else if (g_range_read_seen != 0U) {
    snprintf(line, sizeof(line), "DIST:S%02X %ucm",
             (unsigned int)g_range_status,
             (unsigned int)(g_range_mm / 10U));
  } else if (g_range_active != 0U) {
    snprintf(line, sizeof(line), "DIST: WAIT");
  } else if (hvl53.last_error_stage == VL53L1X_STAGE_ID) {
    snprintf(line, sizeof(line), "DIST:ID %04X H%u",
             (unsigned int)hvl53.last_id,
             (unsigned int)hvl53.last_hal_status);
  } else if (g_range_needed != 0U) {
    snprintf(line, sizeof(line), "DIST: %s", Range_DebugText());
  } else {
    snprintf(line, sizeof(line), "DIST: OFF");
  }
  SSD1306_DrawString(0U, 6U, line);

  SSD1306_DrawString(0U, 7U, "Hold ENC 5s: test");
}

static void UI_DrawBottomStatusBar(void)
{
  char line[32];

  /* Left: light sensor */
  if ((g_sensor_ok != 0U) && (g_lux_disp_valid != 0U)) {
    snprintf(line, sizeof(line), "L:%s%lu",
             (g_lux_disp_stable != 0U) ? "" : "~",
             (unsigned long)(g_lux_disp_x10 / 10UL));
  } else {
    snprintf(line, sizeof(line), "L:--");
  }
  SSD1306_DrawString(0U, 7U, line);

  /* Middle: distance, unit is m */
  if (g_range_active != 0U) {
    if ((g_range_ok != 0U) && (g_range_disp_valid != 0U)) {
      snprintf(line, sizeof(line), "D:%s%u.%02um",
               (g_range_disp_stable != 0U) ? "" : "~",
               (unsigned int)(g_range_disp_mm / 1000U),
               (unsigned int)((g_range_disp_mm % 1000U) / 10U));
    } else if (g_range_read_seen != 0U) {
      snprintf(line, sizeof(line), "D:S%02X", (unsigned int)g_range_status);
    } else {
      snprintf(line, sizeof(line), "D:--");
    }
  } else {
    snprintf(line, sizeof(line), "D:--");
  }
  SSD1306_DrawString(42U, 7U, line);

  /* Right: exposure value */
  if (g_sensor_ok != 0U) {
    if (g_ev_x10 < 0) {
      int16_t ev_abs = (int16_t)(-g_ev_x10);
      snprintf(line, sizeof(line), "EV-%d.%d", ev_abs / 10, ev_abs % 10);
    } else {
      snprintf(line, sizeof(line), "EV%d.%d", g_ev_x10 / 10, g_ev_x10 % 10);
    }
  } else {
    snprintf(line, sizeof(line), "EV--");
  }
  {
    size_t ev_len = strlen(line);
    uint16_t ev_w = (uint16_t)(ev_len * 6U);
    uint8_t ev_x = (ev_w < SSD1306_WIDTH) ? (uint8_t)(SSD1306_WIDTH - ev_w) : 0U;
    SSD1306_DrawString(ev_x, 7U, line);
  }
}

static uint16_t Battery_ReadMv(void)
{
  uint32_t raw = 0;
  HAL_ADC_Start(&hadc1);
  if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
    raw = HAL_ADC_GetValue(&hadc1);
  }
  /* ADC reference 3.3V, 12-bit ADC, external divider is 1:2, so multiply by 2. */
  return (uint16_t)((raw * 3300UL * 2UL) / 4095UL);
}

static uint8_t Battery_Percent(uint16_t mv)
{
  if (mv <= 3200U) return 0U;
  if (mv >= 4200U) return 100U;
  return (uint8_t)(((uint32_t)(mv - 3200U) * 100UL) / 1000UL);
}

static uint8_t FindNearestShutter(float target_s)
{
  uint8_t best = 0;
  float best_score = 100000000.0f;
  for (uint8_t i = 0; i < ARRAY_LEN(shutter_den_tbl); i++) {
    float s = 1.0f / (float)shutter_den_tbl[i];
    float score = (s > target_s) ? (s / target_s) : (target_s / s);
    if (score < best_score) {
      best_score = score;
      best = i;
    }
  }
  return best;
}

static uint8_t FindNearestApertureByN2(float target_n2)
{
  uint8_t best = 0;
  float best_score = 100000000.0f;
  for (uint8_t i = 0; i < ARRAY_LEN(aperture_x10_tbl); i++) {
    float n = (float)aperture_x10_tbl[i] / 10.0f;
    float n2 = n * n;
    float score = (n2 > target_n2) ? (n2 / target_n2) : (target_n2 / n2);
    if (score < best_score) {
      best_score = score;
      best = i;
    }
  }
  return best;
}

static int16_t EV10_FromLuxIso(uint32_t lux_x10, uint16_t iso)
{
  float v;
  int16_t ev = 0;
  if (lux_x10 == 0U || iso == 0U) return -990;

  /* v = N^2/t = lux * ISO / 250.  lux_x10 is lux * 10. */
  v = ((float)lux_x10 * (float)iso) / 2500.0f;
  while (v >= 2.0f) { v *= 0.5f; ev += 10; }
  while (v < 1.0f)  { v *= 2.0f; ev -= 10; }

  /* Small approximation of 10*log2(v), v in [1, 2). Enough for UI display. */
  ev += (int16_t)((v - 1.0f) * 10.0f + 0.5f);
  return ev;
}

static void UI_FillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h)
{
  for (uint8_t yy = 0; yy < h; yy++) {
    for (uint8_t xx = 0; xx < w; xx++) {
      SSD1306_DrawPixel((uint8_t)(x + xx), (uint8_t)(y + yy), 1U);
    }
  }
}

static const char *UI_BigPattern(char c)
{
  switch (c) {
    case '0': return "111"
                     "101"
                     "101"
                     "101"
                     "111";
    case '1': return "010"
                     "110"
                     "010"
                     "010"
                     "111";
    case '2': return "111"
                     "001"
                     "111"
                     "100"
                     "111";
    case '3': return "111"
                     "001"
                     "111"
                     "001"
                     "111";
    case '4': return "101"
                     "101"
                     "111"
                     "001"
                     "001";
    case '5': return "111"
                     "100"
                     "111"
                     "001"
                     "111";
    case '6': return "111"
                     "100"
                     "111"
                     "101"
                     "111";
    case '7': return "111"
                     "001"
                     "010"
                     "010"
                     "010";
    case '8': return "111"
                     "101"
                     "111"
                     "101"
                     "111";
    case '9': return "111"
                     "101"
                     "111"
                     "001"
                     "111";
    case 'F': return "111"
                     "100"
                     "110"
                     "100"
                     "100";
    case '/': return "001"
                     "001"
                     "010"
                     "100"
                     "100";
    case '.': return "000"
                     "000"
                     "000"
                     "000"
                     "010";
    case '-': return "000"
                     "000"
                     "111"
                     "000"
                     "000";
    default:  return "000"
                     "000"
                     "000"
                     "000"
                     "000";
  }
}

static uint8_t UI_BigCharWidth(uint8_t scale)
{
  return (uint8_t)(3U * scale);
}

static void UI_DrawBigChar(uint8_t x, uint8_t y, char c, uint8_t scale)
{
  const char *p = UI_BigPattern(c);

  for (uint8_t row = 0; row < 5U; row++) {
    for (uint8_t col = 0; col < 3U; col++) {
      if (p[row * 3U + col] == '1') {
        UI_FillRect((uint8_t)(x + col * scale),
                    (uint8_t)(y + row * scale),
                    scale,
                    scale);
      }
    }
  }
}

static uint8_t UI_BigStringWidth(const char *str, uint8_t scale, uint8_t spacing)
{
  uint8_t width = 0U;

  while (*str != '\0') {
    width = (uint8_t)(width + UI_BigCharWidth(scale));
    str++;

    if (*str != '\0') {
      width = (uint8_t)(width + spacing);
    }
  }

  return width;
}

static void UI_DrawBigStringCentered(uint8_t y, const char *str, uint8_t scale, uint8_t spacing)
{
  uint8_t width = UI_BigStringWidth(str, scale, spacing);
  uint8_t x = 0U;

  if (width < 128U) {
    x = (uint8_t)((128U - width) / 2U);
  }

  while (*str != '\0') {
    UI_DrawBigChar(x, y, *str, scale);
    x = (uint8_t)(x + UI_BigCharWidth(scale) + spacing);
    str++;
  }
}

static void UI_DrawBatteryIcon(uint8_t x, uint8_t y, uint8_t percent)
{
  SSD1306_DrawRect(x, y, 14U, 7U, 1U);

  /* Battery terminal */
  SSD1306_DrawPixel((uint8_t)(x + 14U), (uint8_t)(y + 2U), 1U);
  SSD1306_DrawPixel((uint8_t)(x + 14U), (uint8_t)(y + 3U), 1U);
  SSD1306_DrawPixel((uint8_t)(x + 14U), (uint8_t)(y + 4U), 1U);

  uint8_t bars;
  if (percent > 75U) {
    bars = 4U;
  } else if (percent > 50U) {
    bars = 3U;
  } else if (percent > 25U) {
    bars = 2U;
  } else if (percent > 5U) {
    bars = 1U;
  } else {
    bars = 0U;
  }

  for (uint8_t i = 0; i < bars; i++) {
    UI_FillRect((uint8_t)(x + 2U + i * 3U), (uint8_t)(y + 2U), 2U, 3U);
  }
}

static void UI_FormatShutter(char *buf, uint8_t size)
{
  if (shutter_den_tbl[g_shutter_idx] == 1U) {
    snprintf(buf, size, "1");
  } else {
    snprintf(buf, size, "1/%u", shutter_den_tbl[g_shutter_idx]);
  }
}

static void UI_FormatAperture(char *buf, uint8_t size)
{
  snprintf(buf, size, "F%u.%u",
           aperture_x10_tbl[g_aperture_idx] / 10U,
           aperture_x10_tbl[g_aperture_idx] % 10U);
}

static void Update_Display(void)
{
  char line[32];
  char main_value[16];
  char sub_value[16];

  if (g_screen == SCREEN_SELF_TEST) {
    UI_DrawSelfTest();
    if (I2C1_TryLock()) {
      SSD1306_Update();
      I2C1_Unlock();
    }
    return;
  }

  SSD1306_Clear();

  /* Top status bar: mode / ISO / battery */
  SSD1306_DrawString(0U, 0U, (g_mode == MODE_TV) ? "Tv" : "Av");

  snprintf(line, sizeof(line), "ISO%u", iso_tbl[g_iso_idx]);
  SSD1306_DrawString(38U, 0U, line);

  snprintf(line, sizeof(line), "%u%%", g_bat_pct);
  SSD1306_DrawString(86U, 0U, line);
  UI_DrawBatteryIcon(112U, 0U, g_bat_pct);

  SSD1306_DrawHLine(0U, 11U, 128U, 1U);

  /* Main parameter follows current priority mode. */
  if (g_mode == MODE_TV) {
    UI_FormatShutter(main_value, sizeof(main_value));
    UI_FormatAperture(sub_value, sizeof(sub_value));
  } else {
    UI_FormatAperture(main_value, sizeof(main_value));
    UI_FormatShutter(sub_value, sizeof(sub_value));
  }

  /* Big main value */
  UI_DrawBigStringCentered(15U, main_value, 5U, 3U);

  SSD1306_DrawHLine(0U, 42U, 128U, 1U);

  /* Recommended/secondary value */
  UI_DrawBigStringCentered(45U, sub_value, 2U, 2U);

  SSD1306_DrawHLine(0U, 55U, 128U, 1U);

  /* Bottom status bar: Lux left, Dist center, EV right.
     Distance no longer uses the F-value area above this bar. */
  UI_DrawBottomStatusBar();

  if (I2C1_TryLock()) {
    SSD1306_Update();
    I2C1_Unlock();
  }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == RANGE_INT_Pin) {
    /* ISR only sets a flag. Never touch I2C here. */
    g_vl53_int_flag = 1U;
  }
}

/* USER CODE END 4 */

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
