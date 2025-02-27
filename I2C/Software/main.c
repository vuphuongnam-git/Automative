#include "stm32f10x.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_i2c.h"              // Keil::Device:StdPeriph Drivers:I2C

#define I2C_SCL 	GPIO_Pin_6
#define I2C_SDA		GPIO_Pin_7

#define I2C_GPIO 	GPIOB

#define WRITE_SDA_0 	GPIO_ResetBits(I2C_GPIO, I2C_SDA)
#define WRITE_SDA_1 	GPIO_SetBits(I2C_GPIO, I2C_SDA)
#define WRITE_SCL_0 	GPIO_ResetBits(I2C_GPIO, I2C_SCL)
#define WRITE_SCL_1 	GPIO_SetBits(I2C_GPIO, I2C_SCL)
#define READ_SDA_VAL 	GPIO_ReadInputDataBit(I2C_GPIO, I2C_SDA)

//T(SCL) = 10us, 5us HIGH, 5us LOW

typedef enum{
	NOT_OK = 0,
	OK = 1
} status;

typedef enum{
	NACK = 0,
	ACK = 1
} ACK_Bit;

void Delay1Ms(void)
{
	
	TIM_SetCounter(TIM2, 0);
	while (TIM_GetCounter(TIM2) < 1000) {
	}
}

void delay_us(uint32_t delay)
{
	TIM_SetCounter(TIM2, 0);
 	while (TIM_GetCounter(TIM2) < delay) {
	}
}

void Delay_Ms(uint32_t u32DelayInMs)
{
	
	while (u32DelayInMs) {
		Delay1Ms();
		--u32DelayInMs;
	}
}

void RCC_Config(){
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
}

void TIM_Config(){
	
	TIM_TimeBaseInitTypeDef TIM_TimBaseInitStructure;
	TIM_TimBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimBaseInitStructure.TIM_Period = 0xFFFF;
	TIM_TimBaseInitStructure.TIM_Prescaler = 72 - 1;
	TIM_TimBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	
	TIM_TimeBaseInit(TIM2, &TIM_TimBaseInitStructure);
	
	TIM_Cmd(TIM2, ENABLE);
}

void GPIO_Config(){
	GPIO_InitTypeDef GPIO_InitStructure;
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
	GPIO_InitStructure.GPIO_Pin = I2C_SCL | I2C_SDA;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	
	GPIO_Init(I2C_GPIO, &GPIO_InitStructure);
}

void I2C_Config(){
	WRITE_SDA_1;
	delay_us(1);
	WRITE_SCL_1;
	delay_us(1);
}

void I2C_Start(){
	WRITE_SDA_0;	//SDA reset to 0 before SCL.
	delay_us(3);
	WRITE_SCL_0;
	delay_us(3);
}

void I2C_Stop(){
	
	WRITE_SDA_0;
	delay_us(3);
	WRITE_SCL_1; 	//SCL set to 1 before SDA.
	delay_us(3);
	WRITE_SDA_1;
	delay_us(3);
}

status I2C_Write(uint8_t pData){	//Wrrite a byte data, return the status of writing. 
	status stRet;
	for(int i = 0; i< 8; i++){		//Write byte data.
		if (pData & 0x80) {					//0b1000 0000
			WRITE_SDA_1;
		} else {
			WRITE_SDA_0;
		}
		delay_us(3);		//Make sure SDA HIGH complete
		
		//SCL Clock
		WRITE_SCL_1;
		delay_us(5);
		WRITE_SCL_0;
		delay_us(5);
		
		//Shift 1 bit to the left
		pData <<= 1;
	}
	
	WRITE_SDA_1;					//Set SDA to read ACK from slave.
	delay_us(3);
	WRITE_SCL_1;					//
	delay_us(3);
	
	if (READ_SDA_VAL) {				//Read ACK bit at the 9th clock.
		stRet = NOT_OK;					//If SDA still at High >> NACK>> NOT_OK
	} else {
		stRet = OK;					//else
	}
	
	delay_us(2);
	WRITE_SCL_0;
	delay_us(5);
	
	return stRet;
}

uint8_t I2C_Read(ACK_Bit _ACK){		//Read a byte data frome slave and write a ACK bit at 9th clock;
	uint8_t u8Ret = 0x00;
	
	WRITE_SDA_1;
	delay_us(1);	
	
	for (int i = 0; i < 8; i++) {
		u8Ret <<= 1;
		
		//SCL Clock
		WRITE_SCL_1;
		delay_us(3);
		if (READ_SDA_VAL) {
			u8Ret |= 0x01;
		}
		delay_us(2);
		WRITE_SCL_0;
		delay_us(5);
	}
	
	if (_ACK) {						//Check the ACK to write back.
		WRITE_SDA_0;
	} else {
		WRITE_SDA_1;
	}
	delay_us(1);
	
	//SCL Clock
	WRITE_SCL_1;
	delay_us(5);
	WRITE_SCL_0;
	delay_us(5);
	
	return u8Ret;
}

//MemAddr = 0x1234 = 0b0011 0100 
status EPROM_Read(uint16_t MemAddr, uint8_t SlaveAddress, uint8_t NumByte, uint8_t *pData)
{
	uint8_t i;
	
	I2C_Start();
	if (I2C_Write(SlaveAddress << 1) == NOT_OK) {		//Write the address of eprom. with Write bit
		I2C_Stop();
		return NOT_OK;
	}
	
	if (I2C_Write(MemAddr >> 8) == NOT_OK) {		//Write the 8 high bits of the adddress to write data
		I2C_Stop();
		return NOT_OK;
	}
	
	if (I2C_Write(MemAddr) == NOT_OK) {			//Write the 8 low bits of the adddress to write data
		I2C_Stop();
		return NOT_OK;
	}
	
	I2C_Start();
	
	if (I2C_Write((SlaveAddress<<1) | 1) == NOT_OK) {//Wtite the address of eprom. with Read bit
		I2C_Stop();
		return NOT_OK;
	}
	
	for (i = 0; i < NumByte - 1; ++i) { 				//Write data to 16bit MemAddr.
		pData[i] = I2C_Read(ACK);
	}
	
	pData[i] = I2C_Read(NACK);
	
	I2C_Stop();
	
	return OK;
}

status EPROM_Write(uint16_t MemAddr,uint8_t SlaveAddress, uint8_t NumByte, uint8_t *pData)
{
	uint8_t i;
	
	for (i = 0; i < NumByte; ++i) {
		I2C_Start();
		if (I2C_Write(SlaveAddress << 1) == NOT_OK) {
			I2C_Stop();
			return NOT_OK;
		}
		if (I2C_Write((MemAddr + i) >> 8) == NOT_OK) {
			I2C_Stop();
			return NOT_OK;
		}
		
		if (I2C_Write(MemAddr + i) == NOT_OK) {
			I2C_Stop();
			return NOT_OK;
		}
		if (I2C_Write(pData[i]) == NOT_OK) {
			I2C_Stop();
			return NOT_OK;
		}
		I2C_Stop();
		Delay_Ms(10);
	}
	return OK;
}

uint8_t Data1[10] = {0x03, 0xFF, 0x0E, 0xDA, 0xA6, 0x6F, 0x50, 0x00, 0x00, 0xF0};
uint8_t Data2[10] = {0x01, 0x0A, 0x19, 0x24, 0xFA, 0x10, 0x3C, 0x48, 0xFF, 0x77};
uint8_t Rcv[10] = {0x00, 0x00, 0x00, 0x00, 0x00,0x00, 0x00, 0x00, 0x00, 0x00};

void I2C_Initial(){
	I2C_InitTypeDef I2C_InitStruct;
	
	I2C_InitStruct.I2C_Mode = I2C_Mode_I2C;
	I2C_InitStruct.I2C_ClockSpeed = 100000;
	I2C_InitStruct.I2C_DutyCycle = I2C_DutyCycle_16_9;
	I2C_InitStruct.I2C_OwnAddress1 = 0x00;
	I2C_InitStruct.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
	I2C_InitStruct.I2C_Ack = I2C_Ack_Enable;
	
	I2C_Init(I2C1, &I2C_InitStruct);
	I2C_Cmd(I2C1, ENABLE);
	
}

int main(){
	RCC_Config();
	TIM_Config();
	GPIO_Config();
	I2C_Config();
	
	while(EPROM_Write(0x0030, 0x50 ,10, Data1) == NOT_OK){}	//Default address of eprom is 0x50
	
	while(EPROM_Write(0x0080, 0x50 ,10, Data2) == NOT_OK){}
	
	while(1){
		while(EPROM_Read(0x0030, 0x50 ,10, Rcv) == NOT_OK) {}
		while(EPROM_Read(0x0080, 0x50 ,10, Rcv) == NOT_OK) {}
	}
}

