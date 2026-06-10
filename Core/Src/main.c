//********************************************************************
// INCLUDE FILES
//====================================================================

#define STM32F051
#include "stm32f0xx.h"											   
#include "lcd_stm32f0.h"
#include "stdio.h"
#include "stdint.h"

//====================================================================
// GLOBAL CONSTANTS
//====================================================================

//====================================================================
// GLOBAL VARIABLES
//====================================================================

int32_t old_encoder_val = 0;   //initializing previous encoder value to zero
float speed = 0.0f;   //initializing speed variable to zero

//====================================================================
// FUNCTION DECLARATIONS
//====================================================================

void ResetClockTo48Mhz(void);
void initGPIOB(void);
void initTIM2(void);
void initTIM3(void);
void initTIM14(void);
void TIM14_IRQHandler(void);
void printToLCD(void);

//====================================================================
// MAIN FUNCTION
//====================================================================

int main (void)
{
    ResetClockTo48Mhz();
    init_LCD();
    initGPIOB();
    initTIM2();
    initTIM3();
    initTIM14();

    while(1)
    {
        printToLCD();
    }

}							
// End of main

//====================================================================
// FUNCTION DEFINITIONS
//====================================================================

void ResetClockTo48Mhz(void)
{
    if ((RCC->CFGR & RCC_CFGR_SWS) == RCC_CFGR_SWS_PLL)
    {
        RCC->CFGR &= (uint32_t) (~RCC_CFGR_SW);
        while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI);
    }
    RCC->CR &= (uint32_t)(~RCC_CR_PLLON);
    while ((RCC->CR & RCC_CR_PLLRDY) != 0);
    RCC->CFGR = ((RCC->CFGR & (~0x003C0000)) | 0x00280000);
    RCC->CR |= RCC_CR_PLLON;
    while ((RCC->CR & RCC_CR_PLLRDY) == 0);
    RCC->CFGR |= (uint32_t) (RCC_CFGR_SW_PLL);
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
}

void initGPIOB(void)
{
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;  //enabling clock
    GPIOB->MODER |= GPIO_MODER_MODER10_1 | GPIO_MODER_MODER4_1 | GPIO_MODER_MODER5_1; //setting pin 4,5,10 to alternate function mode
    GPIOB->PUPDR |= GPIO_PUPDR_PUPDR4_0;    //enable pull-up resistors for PB4 and PB5
    GPIOB->PUPDR |= GPIO_PUPDR_PUPDR5_0;
    GPIOB->AFR[1] |= 0x2 << 8; //setting alternate function to AF2 (TIM2_CH3, PB10)
    GPIOB->AFR[0] |= 0x1 << 16; //setting alternate function to AF1 (TIM3_CH1, PB4)
    GPIOB->AFR[0] |= 0x1 << 20; //setting alternate function to AF1 (TIM3_CH2, PB5)
}

void initTIM2(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN; //enabling clock
    TIM2->PSC = 23;
    TIM2->ARR = 199;
    TIM2->CCR3 = 100;    //setting duty cycle to 50% | duty cycle = (CCR3/(ARR+1))*100
    TIM2->CCMR2 |= TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC3M_1; //setting output compare mode to PWM mode 1 (channel 3)
    TIM2->CCER |= TIM_CCER_CC3E; //enabling channel 3 output to pin
    TIM2->CR1 |= TIM_CR1_CEN; //starting timer

}

void initTIM3(void) //setting up encoder mode for TIM3
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN; //enabling clock
    TIM3->ARR = 0xFFFF; //setting auto reload value to maximum
    TIM3->CCMR1 |= TIM_CCMR1_CC1S_0 | TIM_CCMR1_CC2S_0; //setting channel 1 and 2 to input mode (PB4 and PB5)
    TIM3->CCER &= ~(TIM_CCER_CC1P | TIM_CCER_CC2P); //setting polarity
    TIM3->SMCR |= TIM_SMCR_SMS_0 | TIM_SMCR_SMS_1; //setting both inputs to trigger rising and falling edges
    TIM3->CR1 |= TIM_CR1_CEN; //starting timer
}

void printToLCD(void)   //function to print speed to LCD
{
    char buffer[20];
    sprintf(buffer, "Speed: %d RPM", (int)speed);  //formatting encoder count into string
    lcd_command(CLEAR);
    delay(2000);
    lcd_putstring(buffer);
}

void initTIM14(void)    //setting up TIM14 for encoder conversion
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM14EN; //enable clock
    TIM14->PSC = 74;
    TIM14->ARR = 63999;
    TIM14->DIER |= TIM_DIER_UIE; //enable interupt on update event
    NVIC_EnableIRQ(TIM14_IRQn); //enable TIM14 interupt in NVIC
    TIM14->CR1 |= TIM_CR1_CEN;  //start timer
}

void TIM14_IRQHandler(void)   //TIM14 interupt handler for encoder conversion
{

    TIM14->SR &= ~TIM_SR_UIF;    //clear the update interrupt flag
    int32_t current_encoder_val = TIM3->CNT;   //recording the CNT value for comparison
    int32_t encoder_diff = current_encoder_val - old_encoder_val;   //calculating the difference in encoder counts
    speed = ((float)encoder_diff)*(75.0f/1048.0f);  //calculating speed in RPM
    old_encoder_val = current_encoder_val;

}
//********************************************************************
// END OF PROGRAM
//********************************************************************