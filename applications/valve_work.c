/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-11-27     Rick       the first version
 */
#include "rtthread.h"
#include "rtdevice.h"
#include "pin_config.h"
#include "water_work.h"

#define DBG_TAG "valve"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

uint8_t lock_status = 0;
uint8_t valve_status = 0;
uint8_t valve_valid = 1;

#define VALVE_STATUS_CLOSE   0
#define VALVE_STATUS_OPEN    1

uint8_t valve_left_low_cnt,valve_right_low_cnt = 0;
uint8_t valve_left_check_start,valve_right_check_start = 0;
uint8_t valve_left_low_check_start,valve_right_low_check_start = 0;
uint8_t valve_left_detect_low_success,valve_right_detect_low_success = 0;
uint8_t valve_left_warning_result,valve_right_warning_result = 0;    //0:normal,1:warning

rt_timer_t valve_delay_close_timer = RT_NULL;
rt_timer_t valve_check_detect_low_timer;
rt_timer_t valve_check_detect_timeout_timer;
rt_timer_t valve_check_left_final_timer;
rt_timer_t valve_check_right_final_timer;
rt_timer_t valve_detect_once_timer = RT_NULL;

extern enum Device_Status DeviceStatus;
extern WariningEvent ValveLeftFailEvent;
extern WariningEvent ValveRightFailEvent;
extern WariningEvent SlaverSensorLeakEvent;

void valve_turn_control(int dir)
{
    if(dir < 0)
    {
        valve_status = VALVE_STATUS_CLOSE;
        rt_pin_write(MOTO_LEFT_CONTROL_PIN,PIN_LOW);
        rt_pin_write(MOTO_RIGHT_CONTROL_PIN,PIN_LOW);
    }
    else
    {
        valve_status = VALVE_STATUS_OPEN;
        rt_pin_write(MOTO_LEFT_CONTROL_PIN,PIN_HIGH);
        rt_pin_write(MOTO_RIGHT_CONTROL_PIN,PIN_HIGH);
    }
}

rt_err_t valve_open(void)
{
    if(lock_status == 1 || valve_valid == 0)
    {
        led_valve_fail();
        return RT_ERROR;
    }

    DeviceStatus = ValveOpen;
    led_valve_on();
    beep_once();
    valve_check_stop();
    valve_turn_control(1);
    rt_timer_start(valve_detect_once_timer);

    return RT_EOK;
}

rt_err_t valve_close(void)
{
    if(valve_valid == 0)
    {
        led_valve_fail();
        valve_turn_control(-1);
        return RT_ERROR;
    }

    DeviceStatus = ValveClose;
    led_valve_off();
    beep_key_down();
    valve_check_stop();
    valve_turn_control(-1);
    rt_timer_stop(valve_detect_once_timer);

    return RT_EOK;
}

void valve_lock(void)
{
    if(lock_status == 0)
    {
        lock_status = 1;
        flash_set_key("valve_lock",1);
    }
}

void valve_unlock(void)
{
    if(lock_status == 1)
    {
        lock_status = 0;
        flash_set_key("valve_lock",0);
    }
}

uint8_t get_valve_lock(void)
{
    return lock_status;
}

uint8_t get_valve_status(void)
{
    return valve_status;
}

void valve_delay_close_timer_callback(void *parameter)
{
    if(DeviceStatus == ValveClose || DeviceStatus == ValveOpen)
    {
        valve_lock();
        valve_close();
        gateway_control_master_control(0);
    }
}

void valve_delay_control(uint8_t value)
{
    if(value)
    {
        rt_timer_start(valve_delay_close_timer);
    }
    else
    {
        rt_timer_stop(valve_delay_close_timer);
    }
}

void valve_check_detect_low_timer_callback(void *parameter)
{
    if(valve_left_low_check_start == 1 && valve_left_detect_low_success == 0)
    {
        if(rt_pin_read(MOTO_LEFT_HALL_PIN) == 0)
        {
            valve_left_low_cnt++;
        }
        else
        {
            valve_left_low_cnt = 0;
        }

        if(valve_left_low_cnt > 3)
        {
            valve_left_low_check_start = 0;
            valve_left_detect_low_success = 1;
            rt_pin_write(MOTO_LEFT_CONTROL_PIN,PIN_HIGH);
            rt_timer_start(valve_check_left_final_timer);
        }
        LOG_D("valve_left_low_cnt %d",valve_left_low_cnt);
    }

    if(valve_right_low_check_start == 1 && valve_right_detect_low_success == 0)
    {
        if(rt_pin_read(MOTO_RIGHT_HALL_PIN) == 0)
        {
            valve_right_low_cnt++;
        }
        else
        {
            valve_right_low_cnt = 0;
        }

        if(valve_right_low_cnt > 3)
        {
            valve_right_low_check_start = 0;
            valve_right_detect_low_success = 1;
            rt_pin_write(MOTO_RIGHT_CONTROL_PIN,PIN_HIGH);
            rt_timer_start(valve_check_right_final_timer);
        }
        LOG_D("valve_right_low_cnt %d",valve_right_low_cnt);
    }

    if(valve_left_low_check_start == 0 && valve_right_low_check_start == 0)
    {
        LOG_I("valve_check_detect_low_timer stop");
        rt_timer_stop(valve_check_detect_low_timer);
    }
}

void valve_check_detect_timeout_timer_callback(void *parameter)
{
    if(valve_left_low_check_start == 1 && valve_left_detect_low_success == 0)
    {
        valve_valid = 0;
        valve_left_check_start = 0;
        valve_left_low_check_start = 0;
        valve_left_warning_result = 1;
        warning_enable(ValveLeftFailEvent);
        rt_pin_write(MOTO_LEFT_CONTROL_PIN,PIN_HIGH);
        LOG_D("valve_left_turn_check fail");
    }

    if(valve_right_low_check_start == 1 && valve_right_detect_low_success == 0)
    {
        valve_valid = 0;
        valve_right_check_start = 0;
        valve_right_low_check_start = 0;
        valve_right_warning_result = 1;
        warning_enable(ValveRightFailEvent);
        rt_pin_write(MOTO_RIGHT_CONTROL_PIN,PIN_HIGH);
        LOG_D("valve_right_turn_check fail");
    }
}

void valve_check_left_final_timer_callback(void *parameter)
{
    valve_left_check_start = 0;
    if(rt_pin_read(MOTO_LEFT_HALL_PIN) == 1)
    {
        valve_left_warning_result = 0;
        if(valve_valid == 0 && valve_right_warning_result == 0)
        {
            valve_valid = 1;
            valvefail_warning_disable();
        }
        gateway_warning_master_valve_check(1);
        LOG_D("valve_left_check success");
    }
    else
    {
        valve_valid = 0;
        valve_left_warning_result = 1;
        warning_enable(ValveLeftFailEvent);
        LOG_D("valve_left_check fail");
    }
}

void valve_check_right_final_timer_callback(void *parameter)
{
    valve_right_check_start = 0;
    if(rt_pin_read(MOTO_RIGHT_HALL_PIN) == 1)
    {
        valve_right_warning_result = 0;
        if(valve_valid == 0 && valve_left_warning_result == 0)
        {
            valve_valid = 1;
            valvefail_warning_disable();
        }
        gateway_warning_master_valve_check(2);
        LOG_D("valve_right_check success");
    }
    else
    {
        valve_valid = 0;
        valve_right_warning_result = 1;
        warning_enable(ValveRightFailEvent);
        LOG_D("valve_right_check fail");
    }
}

void valve_check_start(void)
{
    if(valve_status == VALVE_STATUS_CLOSE)
    {
        LOG_E("valve_check failed,valve is close");
        return;
    }

    if(rt_pin_read(MOTO_LEFT_HALL_PIN) == 1 && valve_left_check_start == 0)
    {
        valve_left_low_cnt = 0;
        valve_left_check_start = 1;
        valve_left_low_check_start = 1;
        valve_left_detect_low_success = 0;
        rt_pin_write(MOTO_LEFT_CONTROL_PIN,PIN_LOW);
        rt_timer_stop(valve_check_left_final_timer);
        rt_timer_start(valve_check_detect_low_timer);
        rt_timer_start(valve_check_detect_timeout_timer);
        LOG_D("valve_left_turn_check start");
    }
    if(rt_pin_read(MOTO_RIGHT_HALL_PIN) == 1 && valve_right_check_start == 0)
    {
        valve_right_low_cnt = 0;
        valve_right_check_start = 1;
        valve_right_low_check_start = 1;
        valve_right_detect_low_success = 0;
        rt_pin_write(MOTO_RIGHT_CONTROL_PIN,PIN_LOW);
        rt_timer_stop(valve_check_right_final_timer);
        rt_timer_start(valve_check_detect_low_timer);
        rt_timer_start(valve_check_detect_timeout_timer);
        LOG_D("valve_right_turn_check start");
    }
}

void valve_check_stop(void)
{
    valve_left_check_start = 0;
    valve_right_check_start = 0;
    rt_timer_stop(valve_check_detect_low_timer);
    rt_timer_stop(valve_check_detect_timeout_timer);
    rt_timer_stop(valve_check_left_final_timer);
    rt_timer_stop(valve_check_right_final_timer);
}

void valve_detect_once_timer_callback(void *parameter)
{
    valve_check_start();
}

void valve_init(void)
{
    lock_status = flash_get_key("valve_lock");

    rt_pin_mode(MOTO_LEFT_CONTROL_PIN,PIN_MODE_OUTPUT);
    rt_pin_mode(MOTO_RIGHT_CONTROL_PIN,PIN_MODE_OUTPUT);
    rt_pin_mode(MOTO_LEFT_HALL_PIN,PIN_MODE_INPUT);
    rt_pin_mode(MOTO_RIGHT_HALL_PIN,PIN_MODE_INPUT);

    valve_delay_close_timer = rt_timer_create("valve_delay_close_timer", valve_delay_close_timer_callback, RT_NULL, 4*60*60*1000, RT_TIMER_FLAG_ONE_SHOT|RT_TIMER_FLAG_SOFT_TIMER);
    valve_detect_once_timer  = rt_timer_create("valve_detect", valve_detect_once_timer_callback, RT_NULL, 60*1000*5, RT_TIMER_FLAG_ONE_SHOT|RT_TIMER_FLAG_SOFT_TIMER);
    valve_check_detect_low_timer = rt_timer_create("valve_check_detect_low", valve_check_detect_low_timer_callback, RT_NULL, 500, RT_TIMER_FLAG_PERIODIC|RT_TIMER_FLAG_SOFT_TIMER);
    valve_check_detect_timeout_timer = rt_timer_create("valve_check_detect_timeout", valve_check_detect_timeout_timer_callback, RT_NULL, 10000, RT_TIMER_FLAG_ONE_SHOT|RT_TIMER_FLAG_SOFT_TIMER);
    valve_check_left_final_timer = rt_timer_create("valve_check_left_final", valve_check_left_final_timer_callback, RT_NULL, 9000, RT_TIMER_FLAG_ONE_SHOT|RT_TIMER_FLAG_SOFT_TIMER);
    valve_check_right_final_timer = rt_timer_create("valve_check_right_final", valve_check_right_final_timer_callback, RT_NULL, 10000, RT_TIMER_FLAG_ONE_SHOT|RT_TIMER_FLAG_SOFT_TIMER);

    if(aq_device_waterleak_find())
    {
        warning_enable(SlaverSensorLeakEvent);
    }
    else
    {
        valve_open();
    }
}
