/* Board bring-up that is common to every stage: enable GPIO clocks and put
 * every panel pin into its safe, correct mode. Peripheral drivers (codec,
 * ADC, OLED, MIDI) configure their own pins when they start. */
#include "board_algoradix.h"
#include "gpio.h"
#include "board_init.h"

void board_init_gpio(void)
{
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN
               |  RCC_AHB1ENR_GPIODEN | RCC_AHB1ENR_GPIOEEN | RCC_AHB1ENR_GPIOFEN
               |  RCC_AHB1ENR_GPIOGEN | RCC_AHB1ENR_GPIOHEN;
  (void)RCC->AHB1ENR;

  /* Core LEDs */
  gpio_output(LED_GREEN_PORT, LED_GREEN_PIN); gpio_clr(LED_GREEN_PORT, LED_GREEN_PIN);
  gpio_output(LED_RED_PORT,   LED_RED_PIN);   gpio_clr(LED_RED_PORT,   LED_RED_PIN);

  /* Panel LEDs, off */
  gpio_output(LED1_PORT, LED1_PIN); gpio_clr(LED1_PORT, LED1_PIN);
  gpio_output(LED2_PORT, LED2_PIN); gpio_clr(LED2_PORT, LED2_PIN);
  gpio_output(LED3_PORT, LED3_PIN); gpio_clr(LED3_PORT, LED3_PIN);
  gpio_output(LED4_PORT, LED4_PIN); gpio_clr(LED4_PORT, LED4_PIN);

  /* Pots and CV inputs: analog mode (highest impedance, no digital buffer). */
  gpio_analog(POT_F1_PORT, POT_F1_PIN);
  gpio_analog(POT_F2_PORT, POT_F2_PIN);
  gpio_analog(POT_TUNE_PORT, POT_TUNE_PIN);
  gpio_analog(POT_OCT_PORT, POT_OCT_PIN);
  gpio_analog(POT_A_PORT, POT_A_PIN);
  gpio_analog(POT_D_PORT, POT_D_PIN);
  gpio_analog(POT_FILT_PORT, POT_FILT_PIN);
  gpio_analog(POT_LEVELS_PORT, POT_LEVELS_PIN);
  gpio_analog(CV_ASSIGN_PORT, CV_ASSIGN_PIN);

  /* Buttons: active high → pull-down. Encoder A/B: to GND → pull-up. */
  gpio_input(BTN_GATE_PORT, BTN_GATE_PIN, BUTTON_PULL);
  gpio_input(ENC_PUSH_PORT, ENC_PUSH_PIN, BUTTON_PULL);
  gpio_input(ENC_A_PORT, ENC_A_PIN, ENCODER_PULL);
  gpio_input(ENC_B_PORT, ENC_B_PIN, ENCODER_PULL);

  /* Core S2: to VDD, so pull-down. */
  gpio_input(SW2_PORT, SW2_PIN, GPIO_PULLDOWN);

  /* USB host power switch: enable is active low, so this powers the port (as every release since 01 did). SD detect: input pull-up. */
  gpio_output(USBH_PWR_EN_PORT, USBH_PWR_EN_PIN); gpio_clr(USBH_PWR_EN_PORT, USBH_PWR_EN_PIN);
  gpio_input(SD_DETECT_PORT, SD_DETECT_PIN, GPIO_PULLUP);
}
