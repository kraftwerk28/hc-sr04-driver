#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/timer.h>

#include "itm_debug.h"

static float distance = 0;

#define GPIO_SET(port, pin, flag)                                              \
	do {                                                                       \
		if ((flag))                                                            \
			gpio_set(port, pin);                                               \
		else                                                                   \
			gpio_clear(port, pin);                                             \
	} while (0)

void tim3_isr() {
	if (timer_get_flag(TIM3, TIM_SR_CC1IF)) {
		// Caught rising edge of echo signal
		timer_clear_flag(TIM3, TIM_SR_CC1IF);
		return;
	}
	if (timer_get_flag(TIM3, TIM_SR_CC2IF)) {
		// Caught falling edge of echo signal, time to measure distance
		timer_clear_flag(TIM3, TIM_SR_CC2IF);

		distance = (float)(TIM_CCR2(TIM3) - TIM_CCR1(TIM3)) / 58.0;
		printf("Distance: d=%fcm\n", distance);

		// NOTE: By the spec, min_pwm=1000, max_pwm=2000
		// Tune individually for every servo motor to get accurate angle
		uint16_t min_pwm = 700;
		uint16_t max_pwm = 2100;

		float min_distance = 5.0;
		float max_distance = 20.0;

		uint16_t oc_value = min_pwm + ((distance - min_distance) /
									   (max_distance - min_distance)) *
										  (max_pwm - min_pwm);
		oc_value = (oc_value > max_pwm) ? max_pwm : oc_value;
		oc_value = (oc_value < min_pwm) ? min_pwm : oc_value;
		timer_set_oc_value(TIM2, TIM_OC1, oc_value);

		GPIO_SET(GPIOA, GPIO12, distance > 5);
		GPIO_SET(GPIOA, GPIO11, distance > 10);
		GPIO_SET(GPIOA, GPIO9, distance > 20);
		GPIO_SET(GPIOB, GPIO15, distance > 40);
		GPIO_SET(GPIOB, GPIO13, distance > 60);

		return;
	}
}

int main(void) {
	rcc_clock_setup_pll(&rcc_hse_configs[RCC_CLOCK_HSE8_72MHZ]);

	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_GPIOB);
	rcc_periph_clock_enable(RCC_TIM2);
	rcc_periph_clock_enable(RCC_TIM3);

	// Servo pin
	gpio_set_mode(
		GPIOA, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO0);

	// Trigger pin
	gpio_set_mode(
		GPIOB, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO0);

	// Echo pin
	gpio_set_mode(GPIOA, GPIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN, GPIO6);

	// Red led
	gpio_set_mode(
		GPIOB, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO11);

	// Level leds
	gpio_set_mode(
		GPIOA, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL,
		GPIO9 | GPIO11 | GPIO12);
	gpio_set_mode(
		GPIOB, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL,
		GPIO13 | GPIO15);

	{
		uint32_t tim = TIM2;
		uint32_t prescaler = rcc_ahb_frequency / 1000000;
		uint32_t period = 20000;

		timer_set_mode(
			tim, TIM_CR1_CKD_CK_INT, TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);
		timer_set_prescaler(tim, prescaler - 1);
		timer_set_period(tim, period - 1);

		timer_set_oc_mode(tim, TIM_OC1, TIM_OCM_PWM1);
		timer_set_oc_value(tim, TIM_OC1, 2100);
		timer_set_oc_polarity_high(tim, TIM_OC1);
		timer_enable_oc_output(tim, TIM_OC1);

		timer_enable_counter(tim);
	}

	// Trigger + Echo timer
	{
		//    output compare                input capture
		//    on channel 3                 on channels 1,2
		//   ___ 10 μs          ______________ 0..38ms
		//   | |                |            |
		// __| |________________|            |__________

		// NOTE: 10 us for trigger + approx. 50 ms is for echo
		// NOTE: HC SR04 sensor times out after 38ms, which is approx. 650 cm
		uint32_t period = 50000; // 50 ms
		uint32_t prescaler = rcc_ahb_frequency / 1000000;
		uint32_t tim = TIM3;
		uint32_t tim_nvic = NVIC_TIM3_IRQ;

		timer_set_mode(
			tim, TIM_CR1_CKD_CK_INT, TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);
		timer_set_prescaler(tim, prescaler - 1);
		timer_set_period(tim, period - 1);
		timer_continuous_mode(tim);

		timer_ic_set_input(tim, TIM_IC1, TIM_IC_IN_TI1);
		timer_ic_set_filter(tim, TIM_IC1, TIM_IC_CK_INT_N_8);
		timer_ic_set_polarity(tim, TIM_IC1, TIM_IC_RISING);
		timer_ic_enable(tim, TIM_IC1);

		timer_ic_set_input(tim, TIM_IC2, TIM_IC_IN_TI1);
		timer_ic_set_filter(tim, TIM_IC2, TIM_IC_CK_INT_N_8);
		timer_ic_set_polarity(tim, TIM_IC2, TIM_IC_FALLING);
		timer_ic_enable(tim, TIM_IC2);

		timer_set_oc_mode(tim, TIM_OC3, TIM_OCM_PWM1);
		timer_set_oc_value(tim, TIM_OC3, 10 - 1); // 10 us
		timer_enable_oc_output(tim, TIM_OC3);

		nvic_enable_irq(tim_nvic);
		timer_enable_irq(tim, TIM_SR_CC1IF | TIM_SR_CC2IF);

		timer_enable_counter(tim);
	}

	gpio_set(GPIOB, GPIO11);

	for (;;) {
	}

	return 0;
}
