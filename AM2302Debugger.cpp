#include <stdio.h>
#include <stdlib.h>
#include <wiringPi.h>
#include <stdint.h>

#define AM2302_PIN        2      // wPi 2（物理引脚7）
#define RESPONSE_TIMEOUT  2000   // 响应超时时间（微秒）
#define DATA_BITS         40     // 数据位数（40位）

uint8_t data[5];

void gpioInit(int gpioPin) {
    pinMode(gpioPin, OUTPUT);
    digitalWrite(gpioPin, HIGH);
    delay(2000); // 上电稳定
    printf("GPIO初始化完成。\n");
}

void sendStartSignal() {
    pinMode(AM2302_PIN, OUTPUT);
    digitalWrite(AM2302_PIN, LOW);
    printf("[DEBUG] 启动信号：拉低数据线\n");
    delay(5); // 延长至5ms
    digitalWrite(AM2302_PIN, HIGH);
    pinMode(AM2302_PIN, INPUT);
    pullUpDnControl(AM2302_PIN, PUD_UP);
    printf("[DEBUG] 启动信号：释放总线，等待响应\n");
}

int waitPinLevel(int expectedLevel, uint32_t timeout) {
    uint32_t start = micros();
    while (digitalRead(AM2302_PIN) != expectedLevel) {
        if (micros() - start > timeout) {
            printf("[ERROR] 等待电平超时！预期电平：%d\n", expectedLevel);
            return -1;
        }
    }
    printf("[DEBUG] 检测到预期电平：%d\n", expectedLevel);
    return 0;
}

int readData() {
    printf("[DEBUG] 等待传感器响应...\n");
    if (waitPinLevel(LOW, RESPONSE_TIMEOUT) == -1) return -1;
    printf("[DEBUG] 检测到响应低电平。\n");
    if (waitPinLevel(HIGH, RESPONSE_TIMEOUT) == -1) return -1;
    printf("[DEBUG] 检测到响应高电平，开始读取数据...\n");

    for (int i = 0; i < DATA_BITS; i++) {
        if (waitPinLevel(LOW, 60) == -1) return -1;
        uint32_t start = micros();
        if (waitPinLevel(HIGH, 100) == -1) return -1;
        uint32_t duration = micros() - start;

        data[i / 8] <<= 1;
        if (duration > 40) data[i / 8] |= 0x01;
        printf("[DEBUG] 位%d: 持续时间=%dµs，值=%d\n", i, duration, (duration > 40));
    }
    return 0;
}

uint8_t check() {
    uint8_t sum = data[0] + data[1] + data[2] + data[3];
    if (sum != data[4]) {
        printf("[ERROR] 校验和错误！计算值=%d，实际值=%d\n", sum, data[4]);
        return 0;
    }
    return 1;
}

int main(void) {
    if (wiringPiSetup() == -1) {
        printf("wiringPi初始化失败！\n");
        exit(1);
    }
    gpioInit(AM2302_PIN);

    while (1) {
        sendStartSignal();
        if (readData() == 0 && check()) {
            float humidity = (data[0] * 256.0 + data[1]) / 10.0;
            int16_t temp_raw = (data[2] & 0x7F) * 256 + data[3];
            float temperature = temp_raw / 10.0;
            if (data[2] & 0x80) temperature = -temperature;
            printf("湿度: %.1f%%RH\n", humidity);
            printf("温度: %.1f℃\n", temperature);
        }
        delay(2000);
    }
    return 0;
}
