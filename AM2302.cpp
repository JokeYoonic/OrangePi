#include <stdio.h>
#include <stdlib.h>
#include <wiringPi.h>
#include <pthread.h>
#include <stdint.h>

#define AM2302_PIN        2      // wPi 2（物理引脚7）
#define RESPONSE_TIMEOUT  1000   // 微秒
#define DATA_BITS         40     

uint8_t data[5];
uint32_t blockFlag;
pthread_mutex_t flag_mutex = PTHREAD_MUTEX_INITIALIZER;

void gpioInit(int gpioPin) {
    pinMode(gpioPin, OUTPUT);
    digitalWrite(gpioPin, HIGH);
    delay(2000); // 上电稳定
    printf("GPIO初始化完成。\n");
}

void sendStartSignal() {
    pinMode(AM2302_PIN, OUTPUT);
    digitalWrite(AM2302_PIN, LOW);
    delay(2); // 2ms低电平
    digitalWrite(AM2302_PIN, HIGH);
    pinMode(AM2302_PIN, INPUT);
    pullUpDnControl(AM2302_PIN, PUD_UP);
    printf("启动信号已发送。\n");
}

int waitPinLevel(int expectedLevel, uint32_t timeout) {
    uint32_t start = micros();
    while (digitalRead(AM2302_PIN) != expectedLevel) {
        if (micros() - start > timeout) {
            printf("等待电平超时！预期电平：%d\n", expectedLevel);
            return -1;
        }
    }
    return 0;
}

int readData() {
    printf("等待传感器响应...\n");
    if (waitPinLevel(LOW, RESPONSE_TIMEOUT) == -1) return -1;
    printf("检测到响应低电平。\n");
    if (waitPinLevel(HIGH, RESPONSE_TIMEOUT) == -1) return -1;
    printf("检测到响应高电平，开始读取数据...\n");

    for (int i = 0; i < DATA_BITS; i++) {
        if (waitPinLevel(LOW, 60) == -1) return -1;
        uint32_t start = micros();
        if (waitPinLevel(HIGH, 100) == -1) return -1;
        uint32_t duration = micros() - start;

        data[i / 8] <<= 1;
        if (duration > 40) data[i / 8] |= 0x01;
        printf("位%d: 持续时间=%dµs，值=%d\n", i, duration, (duration > 40));
    }
    return 0;
}

uint8_t check() {
    uint8_t sum = data[0] + data[1] + data[2] + data[3];
    if (sum != data[4]) {
        printf("校验和错误！计算值=%d，实际值=%d\n", sum, data[4]);
        return 0;
    }
    return 1;
}

void *readAM2302Data(void *arg) {
    uint8_t attempt = 5;
    while (attempt--) {
        sendStartSignal();
        if (readData() == 0 && check()) {
            float humidity = (data[0] * 256.0 + data[1]) / 10.0;
            int16_t temp_raw = (data[2] & 0x7F) * 256 + data[3];
            float temperature = temp_raw / 10.0;
            if (data[2] & 0x80) temperature = -temperature;

            printf("湿度: %.1f%%RH\n", humidity);
            printf("温度: %.1f℃\n", temperature);

            pthread_mutex_lock(&flag_mutex);
            blockFlag = 0;
            pthread_mutex_unlock(&flag_mutex);
            return (void *)1;
        }
        delay(500);
    }

    pthread_mutex_lock(&flag_mutex);
    blockFlag = 0;
    pthread_mutex_unlock(&flag_mutex);
    printf("5次尝试均失败。\n");
    return (void *)2;
}

int main(void) {
    if (wiringPiSetup() == -1) {
        printf("wiringPi初始化失败！\n");
        exit(1);
    }

    gpioInit(AM2302_PIN);
    pthread_t tid;
    uint32_t waitTime;

    while (1) {
        pthread_mutex_lock(&flag_mutex);
        blockFlag = 1;
        pthread_mutex_unlock(&flag_mutex);
        waitTime = 5;

        if (pthread_create(&tid, NULL, readAM2302Data, NULL) != 0) {
            printf("线程创建失败！\n");
            return -1;
        }

        while (waitTime && blockFlag) {
            delay(1000);
            waitTime--;
        }

        if (blockFlag == 1) {
            pthread_cancel(tid);
            printf("线程超时！\n");
        }
        delay(2000);
    }

    return 0;
}
