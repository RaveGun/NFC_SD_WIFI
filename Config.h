/*
 * Config.h
 *
 *  Created on: Aug 7, 2017
 *      Author: COX
 */

#ifndef CONFIG_H_
#define CONFIG_H_

//#define STD_PRINT_EN
//#define NO_BUZZER

#define SSD1306_OLED_TYPE		(1u)
#define SH1106_OLED_TYPE		(2u)

#define OLED_SMALL	SSD1306_OLED_TYPE
#define OLED_BIG	SH1106_OLED_TYPE

/* To select one of the supported displays please choose between: OLED_SMALL or OLED_BIG */
#define OLED_SIZE	OLED_BIG

#endif /* CONFIG_H_ */
