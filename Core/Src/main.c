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

#define LOG_SIZE 1000

//====================================================================
// GLOBAL VARIABLES
//====================================================================

int32_t old_encoder_val = 0;   //initializing previous encoder value to zero
float feedback_speed = 0.0f;   //initializing speed variable to zero

//PID constants
float T_s = 0.01f;   //initializing sampling time constant
float kp = 15.0f;   //initializing proportional gain constant
float ki = 1.0f;   //initializing integral gain constant
float kd = 0.01f;   //initializing derivative gain constant

//setting up command speed and control output limits for PID control algorithm
float command_speed = 41.0f;  //initializing command speed variable to 41 RPM (desired speed for motor)
float upper_limit = 200.0f ;   //initializing upper limit for control output to 200 (maximum duty cycle)
float lower_limit = 0.0f;   //initializing lower limit for control output to 0 (minimum duty cycle)

//setting up errors and control actions for PID control algorithm
float e_one_step = 0.0f;   //initializing previous error variable for one step back to zero
float e_two_steps = 0.0f;  //initializing previous error variable for two steps back to zero
float u_one_step = 0.0f;   //initializing previous control output variable for one step back to zero
float u_two_steps = 0.0f;   //initializing previous control output variable for two steps back to zero

//setting up log arrays
uint16_t actual_speed_log[LOG_SIZE];  //array to log feedback speeds for analysis
int log_index = 0;   //initializing log index variable to zero

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
void run_PID(float feedback_speed, float command_speed);

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

void initGPIOB(void)    //initializing GPIOB for TIM2_CH3 (PB10) and TIM3_CH1, TIM3_CH2 (PB4, PB5) for encoder input
{
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;  //enabling clock
    GPIOB->MODER |= GPIO_MODER_MODER10_1 | GPIO_MODER_MODER4_1 | GPIO_MODER_MODER5_1; //setting pin 4,5,10 to alternate function mode
    GPIOB->PUPDR |= GPIO_PUPDR_PUPDR4_0;    //enable pull-up resistors for PB4 and PB5
    GPIOB->PUPDR |= GPIO_PUPDR_PUPDR5_0;
    GPIOB->AFR[1] |= 0x2 << 8; //setting alternate function to AF2 (TIM2_CH3, PB10)
    GPIOB->AFR[0] |= 0x1 << 16; //setting alternate function to AF1 (TIM3_CH1, PB4)
    GPIOB->AFR[0] |= 0x1 << 20; //setting alternate function to AF1 (TIM3_CH2, PB5)
}

void initTIM2(void) //timer for PWM generation for motor control
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN; //enabling clock
    TIM2->PSC = 23;
    TIM2->ARR = 199;
    TIM2->CCR3 = 0;    //setting duty cycle to 0% initially | duty cycle = (CCR3/(ARR+1))*100
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
    sprintf(buffer, "Speed: %d RPM", (int)feedback_speed);  //formatting encoder count into string
    lcd_command(CLEAR);
    delay(2000);
    lcd_putstring(buffer);
}

void initTIM14(void)    //setting up TIM14 for encoder conversion every 0.01s
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM14EN; //enable clock
    TIM14->PSC = 74;
    TIM14->ARR = 6399;
    TIM14->DIER |= TIM_DIER_UIE; //enable interupt on update event
    NVIC_EnableIRQ(TIM14_IRQn); //enable TIM14 interupt in NVIC
    TIM14->CR1 |= TIM_CR1_CEN;  //start timer
}

void TIM14_IRQHandler(void)   //TIM14 interupt handler for encoder conversion
{

    TIM14->SR &= ~TIM_SR_UIF;    //clear the update interrupt flag
    int32_t current_encoder_val = TIM3->CNT;   //recording the CNT value for comparison
    int32_t encoder_diff = current_encoder_val - old_encoder_val;   //calculating the difference in encoder counts
    feedback_speed = ((float)encoder_diff)*(750.0f/1048.0f);  //calculating speed in RPM
    run_PID(feedback_speed, command_speed);   //running PID control algorithm to adjust motor speed based on feedback and command speeds
    old_encoder_val = current_encoder_val;

    //setting up logging for analysis
    if (log_index<LOG_SIZE)
    {
        actual_speed_log[log_index] = (uint16_t)feedback_speed;   //logging actual speed
        log_index++;    //incrementing log index for next entry
    }

}

void run_PID(float feedback_speed, float command_speed)    //function to run PID control algorithm
{
    float e = command_speed - feedback_speed;   //calculating error

    float a = kp + ((T_s*ki)/2) +((2*kd)/T_s);   //calculating coefficient a for PID control algorithm
    float b = (T_s*ki)-4*(kd/T_s);   //calculating coefficient b for PID control algorithm
    float c = -kp + ((T_s*ki)/2) + 2*(kd/T_s);   //calculating coefficient c for PID control algorithm

    //calculating control output using PID control algorithm
    float u = u_two_steps + a*e + b*e_one_step + c*e_two_steps;

    //saturating control output to upper and lower limits
    if (u > upper_limit)
    {
        u = upper_limit;
    }
    else if (u < lower_limit)
    {
        u = lower_limit;
    }

    TIM2->CCR3 = (uint32_t)u;   //setting duty cycle based on control output

    //updating previous control outputs and errors for next iteration
    u_two_steps = u_one_step;
    u_one_step = u;
    e_two_steps = e_one_step;
    e_one_step = e;
}
//********************************************************************
// END OF PROGRAM
//********************************************************************