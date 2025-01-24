#ifndef MYTIME_H
#define MYTIME_H

#include <stddef.h>

// 定义 time_t 类型（通常是一个整数类型，表示从某个固定时间点开始的秒数）
typedef long time_t;

// 定义 tm 结构体，用于存储日期和时间的各个部分
struct tm {
    int tm_sec;   // 秒 (0-59)
    int tm_min;   // 分 (0-59)
    int tm_hour;  // 时 (0-23)
    int tm_mday;  // 日 (1-31)
    int tm_mon;   // 月 (0-11)
    int tm_year;  // 年 (从 1900 年开始的年数)
    int tm_wday;  // 星期几 (0-6, 0 = 星期日)
    int tm_yday;  // 一年中的第几天 (0-365)
    int tm_isdst; // 是否启用夏令时
};

// 函数声明
time_t mytime(time_t *timer);
struct tm *mylocaltime(const time_t *timer);

#endif // MYTIME_H
