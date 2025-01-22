#include <stdio.h>
#include <stdlib.h>
#include <wiringPi.h>
#include <pthread.h>
#include <stdint.h>

#define AM2302_PIN        2      // 使用 wPi 编号2（物理引脚7）
#define RESPONSE_TIMEOUT  1000   // 响应超时时间（微秒）
#define DATA_BITS         40     // 数据位数（40位）

uint8_t data[5];                 // 存储40位数据（湿度高、低，温度高、低，校验）
uint32_t blockFlag;

// 初始化 GPIO
void gpioInit(int gpioPin) {
    pinMode(gpioPin, OUTPUT);
    digitalWrite(gpioPin, HIGH);
    delay(2000); // AM2302 上电后需等待至少2秒稳定
}

// 发送启动信号
void sendStartSignal() {
    pinMode(AM2302_PIN, OUTPUT);
    digitalWrite(AM2302_PIN, LOW);
    delay(2);                    // 低电平至少1ms（手册要求1-20ms）
    digitalWrite(AM2302_PIN, HIGH);
    pinMode(AM2302_PIN, INPUT);
    pullUpDnControl(AM2302_PIN, PUD_UP); // 启用内部上拉电阻
    delayMicroseconds(30);       // 等待传感器响应
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
    // 等待传感器响应（80µs低电平）
    if (waitPinLevel(LOW, RESPONSE_TIMEOUT) == -1) return -1;
    if (waitPinLevel(HIGH, RESPONSE_TIMEOUT) == -1) return -1;

    // 读取40位数据
    for (int i = 0; i < DATA_BITS; i++) {
        // 等待50µs低电平（数据位起始）
        if (waitPinLevel(LOW, 60) == -1) return -1;

        // 测量高电平时间以区分0和1
        uint32_t start = micros();
        if (waitPinLevel(HIGH, 100) == -1) return -1;
        uint32_t duration = micros() - start;

        // 数据位赋值（高位在前）
        data[i / 8] <<= 1;
        if (duration > 40) data[i / 8] |= 0x01; // 高电平>40µs视为逻辑1
    }

    return 0;
}

// 校验数据
uint8_t check() {
    return (data[0] + data[1] + data[2] + data[3]) == data[4];
}

// 线程函数：读取数据
void *readAM2302Data(void *arg) {
    uint8_t attempt = 5;

    while (attempt--) {
        sendStartSignal();
        if (readData() == 0 && check()) {
            // 解析湿度（无符号）
            float humidity = (data[0] * 256.0 + data[1]) / 10.0;

            // 解析温度（带符号）
            int16_t temp_raw = (data[2] & 0x7F) * 256 + data[3];
            float temperature = temp_raw / 10.0;
            if (data[2] & 0x80) temperature *= -1;

            printf("Humidity: %.1f%%RH\n", humidity);
            printf("Temperature: %.1f℃\n", temperature);
            blockFlag = 0;
            return (void *)1;
        }
        delay(500); // 失败后等待500ms重试
    }

    blockFlag = 0;
    printf("Failed after 5 attempts.\n");
    return (void *)2;
}

int main(void) {
    pthread_t tid;
    uint32_t waitTime;

    // 初始化 wiringPi（使用 wPi 编号）
    if (wiringPiSetup() == -1) {
        printf("GPIO init failed!\n");
        exit(1);
    }

    gpioInit(AM2302_PIN);

    while (1) {
        blockFlag = 1;
        waitTime = 5;

        if (pthread_create(&tid, NULL, readAM2302Data, NULL) != 0) {
            printf("[%s|%s|%d]: Thread creation failed!\n", __FILE__, __func__, __LINE__);
            return -1;
        }

        // 等待线程完成或超时（5秒）
        while (waitTime && blockFlag) {
            delay(1000);
            waitTime--;
        }

        if (blockFlag == 1) {
            pthread_cancel(tid);
            printf("[%s|%s|%d]: Thread timeout!\n", __FILE__, __func__, __LINE__);
        }
        delay(2000); // 每次读取间隔至少2秒（手册要求）
    }

    return 0;
}
