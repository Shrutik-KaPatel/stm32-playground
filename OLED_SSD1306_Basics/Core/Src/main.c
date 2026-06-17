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
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define OLED_I2C_ADDR (0x3C << 1)
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void OLED_WriteCommand(uint8_t cmd)
{
    HAL_I2C_Mem_Write(&hi2c1, OLED_I2C_ADDR, 0x00, I2C_MEMADD_SIZE_8BIT, &cmd, 1, HAL_MAX_DELAY);
}

void OLED_WriteData(uint8_t data)
{
    HAL_I2C_Mem_Write(&hi2c1, OLED_I2C_ADDR, 0x40, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
}

void OLED_Init(void)
{
    OLED_WriteCommand(0x8D); OLED_WriteCommand(0x14); // enable charge pump
    HAL_Delay(10);
    OLED_WriteCommand(0xA8); OLED_WriteCommand(0x3F); // mux ratio = 64
    OLED_WriteCommand(0xD3); OLED_WriteCommand(0x00); // display offset = 0
    OLED_WriteCommand(0x40);                           // display start line = 0
    OLED_WriteCommand(0xA1);                           // segment re-map
    OLED_WriteCommand(0xC8);                           // COM scan direction remapped
    OLED_WriteCommand(0x81); OLED_WriteCommand(0x7F); // contrast mid-range
    OLED_WriteCommand(0xA4);                           // display follows RAM
    OLED_WriteCommand(0xA6);                           // normal (non-inverted)
    OLED_WriteCommand(0xD5); OLED_WriteCommand(0x80); // clock divide / osc freq
    OLED_WriteCommand(0xDA); OLED_WriteCommand(0x12); // COM pins hw config
    OLED_WriteCommand(0xAF);                           // display ON
    HAL_Delay(100);
    //OLED_WriteCommand(0xA5); // force entire display ON, bypass RAM/framebuffer

    HAL_StatusTypeDef status = HAL_I2C_IsDeviceReady(&hi2c1, OLED_I2C_ADDR, 2, 10);
    char dbg[64];
    sprintf(dbg, "Post-init bus check: %s\r\n", status == HAL_OK ? "OK" : "FAIL");
    HAL_UART_Transmit(&huart2, (uint8_t*)dbg, strlen(dbg), 100);
}



void OLED_Clear(void)
{
    for (uint8_t page = 0; page < 8; page++) {
        OLED_WriteCommand(0xB0 + page); // set page start address (0-7)
        OLED_WriteCommand(0x00);        // lower nibble of column start = 0
        OLED_WriteCommand(0x10);        // upper nibble of column start = 0
        for (uint8_t col = 0; col < 128; col++) {
            OLED_WriteData(0x00);
        }
    }
}

uint8_t OLED_Buffer[128 * 8];

void OLED_Buffer_Clear(void)
{
    memset(OLED_Buffer, 0x00, sizeof(OLED_Buffer));
}

void OLED_SetPixel(uint8_t x, uint8_t y, uint8_t color)
{
    if (x >= 128 || y >= 64) return;
    uint16_t index = x + (y / 8) * 128;
    uint8_t bit = y % 8;
    if (color) {
        OLED_Buffer[index] |= (1 << bit);
    } else {
        OLED_Buffer[index] &= ~(1 << bit);
    }
}

void OLED_UpdateScreen(void)
{
    for (uint8_t page = 0; page < 8; page++) {
        OLED_WriteCommand(0xB0 + page);
        OLED_WriteCommand(0x00);
        OLED_WriteCommand(0x10);
        for (uint8_t col = 0; col < 128; col++) {
            OLED_WriteData(OLED_Buffer[col + page * 128]);
        }
    }
}

const uint8_t Font5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, // space
    {0x3E,0x51,0x49,0x45,0x3E}, // 0
    {0x00,0x42,0x7F,0x40,0x00}, // 1
    {0x42,0x61,0x51,0x49,0x46}, // 2
    {0x21,0x41,0x45,0x4B,0x31}, // 3
    {0x18,0x14,0x12,0x7F,0x10}, // 4
    {0x27,0x45,0x45,0x45,0x39}, // 5
    {0x3C,0x4A,0x49,0x49,0x30}, // 6
    {0x01,0x71,0x09,0x05,0x03}, // 7
    {0x36,0x49,0x49,0x49,0x36}, // 8
    {0x06,0x49,0x49,0x29,0x1E}, // 9
    {0x7E,0x11,0x11,0x11,0x7E}, // A
    {0x7F,0x49,0x49,0x49,0x36}, // B
    {0x3E,0x41,0x41,0x41,0x22}, // C
    {0x7F,0x41,0x41,0x22,0x1C}, // D
    {0x7F,0x49,0x49,0x49,0x41}, // E
    {0x7F,0x09,0x09,0x09,0x01}, // F
    {0x3E,0x41,0x49,0x49,0x7A}, // G
    {0x7F,0x08,0x08,0x08,0x7F}, // H
    {0x00,0x41,0x7F,0x41,0x00}, // I
    {0x20,0x40,0x41,0x3F,0x01}, // J
    {0x7F,0x08,0x14,0x22,0x41}, // K
    {0x7F,0x40,0x40,0x40,0x40}, // L
    {0x7F,0x02,0x0C,0x02,0x7F}, // M
    {0x7F,0x04,0x08,0x10,0x7F}, // N
    {0x3E,0x41,0x41,0x41,0x3E}, // O
    {0x7F,0x09,0x09,0x09,0x06}, // P
    {0x3E,0x41,0x51,0x21,0x5E}, // Q
    {0x7F,0x09,0x19,0x29,0x46}, // R
    {0x46,0x49,0x49,0x49,0x31}, // S
    {0x01,0x01,0x7F,0x01,0x01}, // T
    {0x3F,0x40,0x40,0x40,0x3F}, // U
    {0x1F,0x20,0x40,0x20,0x1F}, // V
    {0x3F,0x40,0x38,0x40,0x3F}, // W
    {0x63,0x14,0x08,0x14,0x63}, // X
    {0x07,0x08,0x70,0x08,0x07}, // Y
    {0x61,0x51,0x49,0x45,0x43}, // Z
    {0x00,0x60,0x60,0x00,0x00}, // .
};

uint8_t OLED_GetFontIndex(char c)
{
    if (c == ' ') return 0;
    if (c >= '0' && c <= '9') return 1 + (c - '0');
    if (c >= 'A' && c <= 'Z') return 11 + (c - 'A');
    if (c == '.') return 37;
    return 0;
}

void OLED_DrawChar(uint8_t x, uint8_t y, char c)
{
    uint8_t idx = OLED_GetFontIndex(c);
    for (uint8_t col = 0; col < 5; col++) {
        uint8_t colData = Font5x7[idx][col];
        for (uint8_t row = 0; row < 7; row++) {
            OLED_SetPixel(x + col, y + row, (colData >> row) & 0x01);
        }
    }
}

void OLED_DrawString(uint8_t x, uint8_t y, const char *str)
{
    while (*str) {
        OLED_DrawChar(x, y, *str);
        x += 6;
        str++;
    }
}

void OLED_DrawCircle(int x0, int y0, int radius, uint8_t color)
{
    int x = radius, y = 0;
    int err = 0;
    while (x >= y) {
        OLED_SetPixel(x0 + x, y0 + y, color);
        OLED_SetPixel(x0 + y, y0 + x, color);
        OLED_SetPixel(x0 - y, y0 + x, color);
        OLED_SetPixel(x0 - x, y0 + y, color);
        OLED_SetPixel(x0 - x, y0 - y, color);
        OLED_SetPixel(x0 - y, y0 - x, color);
        OLED_SetPixel(x0 + y, y0 - x, color);
        OLED_SetPixel(x0 + x, y0 - y, color);
        y += 1;
        err += 1 + 2*y;
        if (2*err - 2*x + 1 > 0) {
            x -= 1;
            err += 1 - 2*x;
        }
    }
}

void OLED_FillCircle(int x0, int y0, int radius, uint8_t color)
{
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            if (x*x + y*y <= radius*radius) {
                OLED_SetPixel(x0 + x, y0 + y, color);
            }
        }
    }
}

void OLED_DrawRect(int x0, int y0, int width, int height, uint8_t color)
{
    for (int x = x0; x < x0 + width; x++) {
        OLED_SetPixel(x, y0, color);
        OLED_SetPixel(x, y0 + height - 1, color);
    }
    for (int y = y0; y < y0 + height; y++) {
        OLED_SetPixel(x0, y, color);
        OLED_SetPixel(x0 + width - 1, y, color);
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
  MX_I2C1_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  char msg[64];
  uint8_t found = 0;

  for (uint16_t addr = 1; addr < 128; addr++) {
      if (HAL_I2C_IsDeviceReady(&hi2c1, (addr << 1), 2, 10) == HAL_OK) {
          sprintf(msg, "Found device at 0x%02X\r\n", addr);
          HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 100);
          found = 1;
      }
  }

  if (!found) {
      sprintf(msg, "No I2C devices found\r\n");
      HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 100);
  }

  OLED_Init();
  HAL_Delay(100);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  /* OPEN EYES */
	    OLED_Buffer_Clear();

	    OLED_DrawRect(20, 6, 88, 52, 1);

	    /* Left eyebrow */
	    for(int x = -10; x <= 10; x++)
	    {
	        int y = (x * x) / 25;
	        OLED_SetPixel(42 + x, 14 + y, 1);
	    }

	    /* Right eyebrow */
	    for(int x = -10; x <= 10; x++)
	    {
	        int y = (x * x) / 25;
	        OLED_SetPixel(86 + x, 14 + y, 1);
	    }

	    /* Left eye */
	    OLED_DrawCircle(42, 28, 10, 1);
	    OLED_DrawCircle(42, 28, 7, 1);
	    OLED_DrawCircle(42, 28, 4, 1);
	    OLED_FillCircle(42, 28, 1, 1);

	    /* Right eye */
	    OLED_DrawCircle(86, 28, 10, 1);
	    OLED_DrawCircle(86, 28, 7, 1);
	    OLED_DrawCircle(86, 28, 4, 1);
	    OLED_FillCircle(86, 28, 1, 1);

	    /* Flirty smile */
	    for(int x = -12; x <= 12; x++)
	    {
	        int y = (x * x) / 35;
	        OLED_SetPixel(64 + x, 44 - y, 1);
	    }

	    OLED_UpdateScreen();
	    HAL_Delay(500);

	    /* WINK */
	    OLED_Buffer_Clear();

	    OLED_DrawRect(20, 6, 88, 52, 1);

	    /* Left eyebrow */
	    for(int x = -10; x <= 10; x++)
	    {
	        int y = (x * x) / 25;
	        OLED_SetPixel(42 + x, 14 + y, 1);
	    }

	    /* Right eyebrow raised */
	    for(int x = -10; x <= 10; x++)
	    {
	        int y = (x * x) / 25;
	        OLED_SetPixel(86 + x, 12 + y, 1);
	    }

	    /* Left eye open */
	    OLED_DrawCircle(42, 28, 10, 1);
	    OLED_DrawCircle(42, 28, 7, 1);
	    OLED_DrawCircle(42, 28, 4, 1);
	    OLED_FillCircle(42, 28, 1, 1);

	    /* Right eye wink */
	    for(int x = -10; x <= 10; x++)
	    {
	        OLED_SetPixel(86 + x, 28, 1);
	        OLED_SetPixel(86 + x, 29, 1);
	    }

	    /* Flirty smile */
	    for(int x = -12; x <= 12; x++)
	    {
	        int y = (x * x) / 35;
	        OLED_SetPixel(64 + x, 44 - y, 1);
	    }

	    OLED_UpdateScreen();
	    HAL_Delay(250);
	}
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */


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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

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
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
int _write(int file, char *ptr, int len)
{
    HAL_UART_Transmit(&huart2, (uint8_t*)ptr, len, HAL_MAX_DELAY);
    return len;
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
