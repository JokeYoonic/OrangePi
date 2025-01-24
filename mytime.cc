#include "mytime.h"
#include <stddef.h>
#include <stdio.h>

// 起始时间2023年1月1日 00:00:00
#define START_YEAR 2023
#define START_MON 0
#define START_DAY 1
#define START_HOUR 0
#define START_MIN 0
#define START_SEC 0

// 每个月的天数（非闰年）
static const int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

// 判断是否为闰年
static int is_leap_year(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

// 计算从 START_YEAR 年到指定年份的总天数
static int total_days(int year) {
    int days = 0;
    for (int y = START_YEAR; y < year; y++) {
        days += is_leap_year(y) ? 366 : 365;
    }
    return days;
}

// 计算从 START_MON 月到指定月份的总天数
static int month_days(int year, int mon) {
    int days = 0;
    for (int m = START_MON; m < mon; m++) {
        days += days_in_month[m];
        if (m == 1 && is_leap_year(year)) {
            days += 1;
        }
    }
    return days;
}

time_t mytime(time_t *timer) {
    time_t t = 0;
    if (timer) {
        *timer = t;
    }
    printf("DEBUG: mytime() called, returning time_t = %ld\n", t);
    return t;
}

struct tm *mylocaltime(const time_t *timer) {
    static struct tm tm_result;
    time_t t = *timer;

    
    tm_result.tm_year = START_YEAR - 1900; 
    tm_result.tm_mon = START_MON;     
    tm_result.tm_mday = START_DAY;         
    tm_result.tm_hour = START_HOUR;       
    tm_result.tm_min = START_MIN;         
    tm_result.tm_sec = START_SEC;         

    
    tm_result.tm_wday = 0; 
    tm_result.tm_yday = 0; 

    // 夏令时标志（未实现）
    tm_result.tm_isdst = -1;

    printf("DEBUG: mylocaltime() called, returning tm = {year=%d, mon=%d, day=%d, hour=%d, min=%d, sec=%d}\n",
           tm_result.tm_year + 1900, tm_result.tm_mon + 1, tm_result.tm_mday,
           tm_result.tm_hour, tm_result.tm_min, tm_result.tm_sec);

    return &tm_result;
}
