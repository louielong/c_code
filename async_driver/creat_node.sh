#!/bin/bash

if [ ! -c /dev/swctrl_psu ];then
    PSU_MAJOR=`cat /proc/devices | grep swctrl_psu | awk '{print $1}'`
    if [ $PSU_MAJOR ]; then
        mknod /dev/swctrl_psu c $PSU_MAJOR 0
    fi
fi

if [ ! -c /dev/swctrl_fan ];then
    FAN_MAJOR=`cat /proc/devices | grep swctrl_fan | awk '{print $1}'`
    if [ $FAN_MAJOR ]; then
        mknod /dev/swctrl_fan c $FAN_MAJOR 0
    fi
fi

if [ ! -c /dev/swctrl_port ];then
    PORT_MAJOR=`cat /proc/devices | grep swctrl_port | awk '{print $1}'`
    if [ $PORT_MAJOR ]; then
        mknod /dev/swctrl_port c $PORT_MAJOR 0
    fi
fi
