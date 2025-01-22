#include <stdio.h>
#include <stdlib.h>
#include <wiringPi.h>
#include <stdint.h>

#define AM2302_PIN        0      // 使用 wPi 0（物理引脚3）
#define START_LOW_TIME    18     // 起始信号低电平时间（ms，手册要求至少1ms，实际建议18ms）
#define START_HIGH_TIME   20     // 起始信号高电平时间（µs）
#define RESPONSE_LOW_TIME 80     // 传感器响应低电平时间（µs）
#define RESPONSE_HIGH_TIME 80    // 传感器响应高电平时间（µs）
#define BIT_LOW_TIME      50     // 数据位低电平时间（µs）
#define JUDGE_DELAY       40     // 数据位判断延时（µs）
#define DATA_BITS         40     // 数据总位数（40位）

uint8_t data[5];                 // 存储40位数据（湿度高8、低8，温度高8、低8，校验）

// 初始化 GPIO
void gpioInit(int gpioPin) {
    pinMode(gpioPin, OUTPUT);
    digitalWrite(gpioPin, HIGH);
    delay(2000); // 传感器上电稳定时间
}

// 发送起始信号（主机拉低至少1ms，实际建议18ms）
void sendStartSignal() {
    pinMode(AM2302_PIN, OUTPUT);
    digitalWrite(AM2302_PIN, LOW);
    delay(START_LOW_TIME);        // 拉低18ms
    digitalWrite(AM2302_PIN, HIGH);
    delayMicroseconds(START_HIGH_TIME); // 主机拉高20µs
    pinMode(AM2302_PIN, INPUT);
    pullUpDnControl(AM2302_PIN, PUD_UP); // 启用内部上拉电阻
}

// 等待引脚电平变化（超时返回-1）
int waitPinLevel(int expectedLevel, uint32_t timeout) {
    uint32_t start = micros();
    while (digitalRead(AM2302_PIN) != expectedLevel) {
        if (micros() - start > timeout) return -1;
    }
    return 0;
}

// 读取40位数据
int readData() {
    // 1. 等待传感器响应（80µs低电平 + 80µs高电平）
    if (waitPinLevel(LOW, RESPONSE_LOW_TIME * 2) == -1) return -1;  // 等待低电平（允许超时到160µs）
    if (waitPinLevel(HIGH, RESPONSE_HIGH_TIME * 2) == -1) return -1; // 等待高电平（允许超时到160µs）

    // 2. 读取40位数据（高位在前）
    for (int i = 0; i < DATA_BITS; i++) {
        // 等待数据位起始低电平（50µs）
        if (waitPinLevel(LOW, BIT_LOW_TIME * 2) == -1) return -1;

        // 延时40µs后检测电平状态
        delayMicroseconds(JUDGE_DELAY);
        uint8_t bitValue = digitalRead(AM2302_PIN);

        // 数据位赋值（高位先出）
        data[i / 8] <<= 1;
        data[i / 8] |= (bitValue & 0x01);

        // 等待剩余高电平时间（逻辑0：26µs剩余，逻辑1：70µs剩余）
        uint32_t start = micros();
        while (digitalRead(AM2302_PIN) == HIGH) {
            if (micros() - start > 100) break; // 防止死循环
        }
    }
    return 0;
}

// 校验数据（校验和 = 湿度高8 + 湿度低8 + 温度高8 + 温度低8）
uint8_t check() {
    return (data[0] + data[1] + data[2] + data[3]) == data[4];
}

// 解析并打印数据
void parseData() {
    // 解析湿度（实际值 = 传感器值 / 10）
    uint16_t humidity_raw = (data[0] << 8) | data[1];
    float humidity = humidity_raw / 10.0;

    // 解析温度（最高位为符号位，实际值 = 传感器值 / 10）
    int16_t temp_raw = (data[2] & 0x7F) << 8 | data[3]; // 屏蔽符号位
    float temperature = temp_raw / 10.0;
    if (data[2] & 0x80) temperature = -temperature;     // 最高位为1表示负数

    printf("湿度: %.1f%%RH\n", humidity);
    printf("温度: %.1f℃\n", temperature);
}

int main(void) {
    if (wiringPiSetup() == -1) {
        printf("GPIO初始化失败！\n");
        exit(1);
    }
    gpioInit(AM2302_PIN);

    while (1) {
        // 连续读取两次，取第二次的值
        for (int i = 0; i < 2; i++) {
            sendStartSignal();
            if (readData() == 0 && check()) {
                if (i == 1) parseData(); // 仅打印第二次数据
            } else {
                printf("数据读取失败！\n");
            }
            delay(2000); // 两次读取间隔至少2秒
        }
    }
    return 0;
}
