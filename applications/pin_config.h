/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-08-06     Rick       the first version
 */

#ifndef APPLICATIONS_PIN_CONFIG_H_
#define APPLICATIONS_PIN_CONFIG_H_
/*
 * RF
 */
#define RF_SW1_PIN                      6
#define RF_SW2_PIN                      7
#define TCXO_PWR_PIN                    16
/*
 * ANT
 */
#define ANT_INT_PIN                     8
#define ANT_EXT_PIN                     9
#define ANT_SW_PIN                      0
/*
 * SENSOR
 */
#define SENSOR_LEAK_PIN                 5
#define SENSOR_LOST_PIN                 4
/*
 * MOTO
 */
#define MOTO_LEFT_HALL_PIN              11
#define MOTO_LEFT_CONTROL_PIN           12
#define MOTO_RIGHT_HALL_PIN             28
#define MOTO_RIGHT_CONTROL_PIN          10
/*
 * KEY
 */
#define KEY_ON_PIN                      15
#define KEY_OFF_PIN                     45
/*
 * BUZZER
 */
#define BEEP_PIN                        23

#endif /* APPLICATIONS_PIN_CONFIG_H_ */
