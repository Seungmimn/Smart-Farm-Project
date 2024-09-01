/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : main program body
  ******************************************************************************
  * @attention
  
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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "i2c_lcd.h"

// DHT11 센서 포트와 핀 정의
#define DHT11_PORT GPIOA
#define DHT11_PIN GPIO_PIN_10
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
I2C_HandleTypeDef hi2c1;  // LCD용 I2C 핸들러
TIM_HandleTypeDef htim1;  // 지연을 위한 타이머 핸들러
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#ifdef __GNUC__
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif /* __GNUC__ */
#define ARR_CNT 5    // 명령어 파싱을 위한 최대 배열 크기
#define CMD_SIZE 50  // 블루투스 데이터의 최대 명령어 크기
/* USER CODE END PD */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;
TIM_HandleTypeDef htim1;
UART_HandleTypeDef huart2;  // PC 통신용 UART 핸들러
UART_HandleTypeDef huart6;  // 블루투스 통신용 UART 핸들러

/* USER CODE BEGIN PV */
uint8_t rx2char;             // UART2로부터 수신된 문자를 저장하는 변수
volatile unsigned char rx2Flag = 0;  // UART2에서 데이터가 수신되었음을 나타내는 플래그
volatile char rx2Data[50];   // UART2에서 수신된 데이터를 저장하는 버퍼
volatile unsigned char btFlag = 0;  // 블루투스에서 데이터가 수신되었음을 나타내는 플래그
uint8_t btchar;              // 블루투스로부터 수신된 문자를 저장하는 변수
char btData[50];             // 블루투스에서 수신된 데이터를 저장하는 버퍼
void bluetooth_Event();      // 블루투스 이벤트 처리 함수 프로토타입
/* USER CODE END PV */

/* USER CODE BEGIN 0 */
void delay_us(uint16_t time) {
	__HAL_TIM_SET_COUNTER(&htim1, 0);
	while((__HAL_TIM_GET_COUNTER(&htim1))<time);
}

int wait_pulse(int state) {
	__HAL_TIM_SET_COUNTER(&htim1, 0);
	while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) != state) {  
		if(__HAL_TIM_GET_COUNTER(&htim1) >= 100) {              
			return 0;
		}
	}
	return 1;
}

int Temperature = 0;  // 온도 데이터를 저장하는 변수
int Humidity = 0;     // 습도 데이터를 저장하는 변수

// DHT11 센서에서 데이터를 읽어오는 함수
int dht11_read (void) {
	//----- DHT11에 대한 시작 신호
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = DHT11_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);

	// 통신 시작을 위해 핀을 18ms 동안 Low로 유지한 후 20us 동안 High로 설정
	HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, 0);
	delay_us(18000);
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, 1);
    delay_us(20);

	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;  // 핀을 입력 모드로 설정
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);

	delay_us(40);  // 40us 동안 응답 대기
	if(!(HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN))) {  // 응답 신호 확인
		delay_us(80);
		if(!(HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN))) 
			return -1;  // 오류: DHT11에서 응답 없음
	}
	if(wait_pulse(GPIO_PIN_RESET) == 0) 
		return -1;  // 타임아웃

	uint8_t out[5], i, j;
	for(i = 0; i < 5; i++) {  // 센서에서 5바이트 데이터를 읽어옴
		for(j = 0; j < 8; j++) {  // 각 비트를 읽어옴
			if(!wait_pulse(GPIO_PIN_SET))  
				return -1;
			delay_us(40);  
			if(!(HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN)))  // Low이면
				out[i] &= ~(1<<(7-j));
			else  // High이면
				out[i] |= (1<<(7-j));

			if(!wait_pulse(GPIO_PIN_RESET)) 
				return -1;
		}
	}

	if(out[4] != (out[0] + out[1] + out[2] + out[3]))
		return -2;  // 체크섬 오류

	Temperature = out[2];  // 읽은 온도 값 저장
	Humidity = out[0];     // 읽은 습도 값 저장

	return 1;  // 성공
}
/* USER CODE END 0 */

/**
  * @brief  애플리케이션 진입점.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
	  HAL_Init();  // HAL 라이브러리 초기화
	  SystemClock_Config();  // 시스템 클록 설정
	  MX_GPIO_Init();  // GPIO 초기화
	  MX_I2C1_Init();  // I2C 초기화
	  MX_TIM1_Init();  // 타이머 초기화

	  // LCD 초기화
	  lcd_init();

	  lcd_clear(); 
	  lcd_put_cur(0, 0);
	  lcd_send_string("Hello, STM32!");
	  HAL_Delay(2000); 
	  lcd_clear(); 

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* 모든 주변 장치 리셋, 플래시 인터페이스 및 SysTick 초기화 */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* 시스템 클록 구성 */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* 초기화된 모든 주변 장치 구성 */
  MX_GPIO_Init();
  MX_USART2_UART_Init();  // UART2 초기화
  MX_USART6_UART_Init();  // UART6 초기화
  MX_TIM1_Init();  // 타이머1 초기화
  MX_I2C1_Init();  // I2C1 초기화
  /* USER CODE BEGIN 2 */
  HAL_UART_Receive_IT(&huart2, &rx2char,1);  // UART2에서 인터럽트 기반으로 수신 시작
  HAL_UART_Receive_IT(&huart6, &btchar,1);  // UART6에서 인터럽트 기반으로 수신 시작
  printf("start main2()\r\n");

  HAL_TIM_Base_Start(&htim1);  // 타이머1 시작

  /* USER CODE END 2 */

  /* 무한 루프 */
  /* USER CODE BEGIN WHILE */
  while (1)
  {

	if(dht11_read() == 1) {  // DHT11 데이터 읽기 성공 시
		static char buffer[50];
		snprintf(buffer, sizeof(buffer), "[MJB_SQL]SENSOR@%d@%d\n", Temperature, Humidity);
		HAL_UART_Transmit_IT(&huart6, (uint8_t *)buffer, strlen(buffer));  // 블루투스로 데이터 전송
		lcd_put_cur(0, 0);
		lcd_send_string("Temp: ");
		char temp_str[5];
		sprintf(temp_str, "%d", Temperature);  // 온도 문자열로 변환
		lcd_send_string(temp_str);
		lcd_send_string("C");
		HAL_UART_Transmit(&huart2, buffer, strlen(buffer), 100);  // UART2로 데이터 전송

		lcd_put_cur(1, 0);
		lcd_send_string("Humidity: ");
		char hum_str[5];
		sprintf(hum_str, "%d", Humidity);  // 습도 문자열로 변환
		lcd_send_string(hum_str);
		lcd_send_string("%");

		HAL_Delay(2000);  // 2초 대기
	}

	if(rx2Flag)  // UART2에서 데이터 수신 플래그 확인
	{
		printf("recv2 : %s\r\n",rx2Data);
		rx2Flag =0;  // 플래그 리셋
	}
	if(btFlag)  // 블루투스에서 데이터 수신 플래그 확인
	{
		btFlag =0;  // 플래그 리셋
		bluetooth_Event();  // 블루투스 이벤트 처리 함수 호출
	}
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief 시스템 클록 구성
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** 메인 내부 레귤레이터 출력 전압 구성
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** RCC 오실레이터 초기화
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** CPU, AHB 및 APB 버스 클록 초기화
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 초기화 함수
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{
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
}

/**
  * @brief TIM1 초기화 함수
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 72-1;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 0xffff-1;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART2 초기화 함수
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{
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
}

/**
  * @brief USART6 초기화 함수
  * @param None
  * @retval None
  */
static void MX_USART6_UART_Init(void)
{
  huart6.Instance = USART6;
  huart6.Init.BaudRate = 9600;
  huart6.Init.WordLength = UART_WORDLENGTH_8B;
  huart6.Init.StopBits = UART_STOPBITS_1;
  huart6.Init.Parity = UART_PARITY_NONE;
  huart6.Init.Mode = UART_MODE_TX_RX;
  huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart6.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart6) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief GPIO 초기화 함수
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO 포트 클록 활성화 */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* GPIO 핀 초기화: 출력 레벨 설정 */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);

  /* B1 핀 초기화: 인터럽트 모드 */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /* LD2 및 PA10 핀 초기화: 출력 모드 */
  GPIO_InitStruct.Pin = LD2_Pin|GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

/* USER CODE BEGIN 4 */
// LED를 켜는 함수
void MX_GPIO_LED_ON(int pin)
{
	HAL_GPIO_WritePin(LD2_GPIO_Port, pin, GPIO_PIN_SET);
}

// LED를 끄는 함수
void MX_GPIO_LED_OFF(int pin)
{
	HAL_GPIO_WritePin(LD2_GPIO_Port, pin, GPIO_PIN_RESET);
}

// 블루투스 이벤트 처리 함수
void bluetooth_Event()
{
  int i=0;
  char * pToken;
  char * pArray[ARR_CNT]={0};
  char recvBuf[CMD_SIZE]={0};
  char sendBuf[CMD_SIZE]={0};
  strcpy(recvBuf,btData);

  printf("btData : %s\r\n",btData);

  pToken = strtok(recvBuf,"[@]");  // '@' 기준으로 문자열 분리
  while(pToken != NULL)
  {
    pArray[i] =  pToken;
    if(++i >= ARR_CNT)
      break;
    pToken = strtok(NULL,"[@]");
  }

  if(!strcmp(pArray[1],"LED"))  // 'LED' 명령어 확인
  {
		if(!strcmp(pArray[2],"ON"))  // 'ON' 명령어 확인
		{
			MX_GPIO_LED_ON(LD2_Pin);  // LED 켜기
		}
		else if(!strcmp(pArray[2],"OFF"))  // 'OFF' 명령어 확인
		{
			MX_GPIO_LED_OFF(LD2_Pin);  // LED 끄기
		}
  }
  else if(!strncmp(pArray[1]," New conn",sizeof(" New conn")))
  {
      return;
  }
  else if(!strncmp(pArray[1]," Already log",sizeof(" Already log")))
  {
      return;
  }
  else
      return;

  sprintf(sendBuf,"[%s]%s@%s\n",pArray[0],pArray[1],pArray[2]);  // 응답 데이터 포맷
  HAL_UART_Transmit(&huart6, (uint8_t *)sendBuf, strlen(sendBuf), 0xFFFF);  // 블루투스로 응답 전송
}

/**
  * @brief  printf 함수를 USART에 리다이렉트
  * @param  None
  * @retval None
  */
PUTCHAR_PROTOTYPE
{
  HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, 0xFFFF);  // UART2로 문자 전송
  return ch;
}

// UART 수신 완료 콜백 함수
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if(huart->Instance == USART2)  // UART2에서 수신된 경우
    {
    	static int i=0;
    	rx2Data[i] = rx2char;
    	if((rx2Data[i] == '\r')||(btData[i] == '\n'))
    	{
    		rx2Data[i] = '\0';
    		rx2Flag = 1;  // 수신 완료 플래그 설정
    		i = 0;
    	}
    	else
    	{
    		i++;
    	}
    	HAL_UART_Receive_IT(&huart2, &rx2char,1);  // UART2 수신 재시작
    }
    if(huart->Instance == USART6)  // UART6에서 수신된 경우
    {
    	static int i=0;
    	btData[i] = btchar;
    	if((btData[i] == '\n') || btData[i] == '\r')
    	{
    		btData[i] = '\0';
    		btFlag = 1;  // 블루투스 수신 완료 플래그 설정
    		i = 0;
    	}
    	else
    	{
    		i++;
    	}
    	HAL_UART_Receive_IT(&huart6, &btchar,1);  // UART6 수신 재시작
    }
}
/* USER CODE END 4 */

/**
  * @brief  오류 발생 시 실행되는 함수
  * @retval None
  */
void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
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
