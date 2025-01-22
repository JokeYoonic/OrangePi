#include <stdio.h>
#include <stdlib.h>
#include <wiringPi.h>
#include <stdint.h>

#define DHT_PIN        2      // 使用wPi编号2（物理引脚7）
#define RETRY_TIMES    5      // 最大重试次数
#define DATA_BITS      40     // 数据总位数（40位）

uint8_t data[5];              // 存储40位数据（湿度高8、低8，温度高8、低8，校验）

// 微秒级延时（Orange Pi需使用系统级延时）
void delay_us(uint32_t us) {
    delayMicroseconds(us);
}

// 毫秒级延时
void delay_ms(uint32_t ms) {
    delay(ms);
}

// 初始化GPIO
void DHT_Init() {
    pinMode(DHT_PIN, OUTPUT);
    digitalWrite(DHT_PIN, HIGH);
    delay_ms(2000);  // 传感器上电稳定
}

// 读取传感器数据（返回0成功，-1失败）
int DHT_Read() {
    uint8_t retry = 0;
    uint8_t i, j;

    // 发送起始信号：拉低至少18ms，再拉高20us
    digitalWrite(DHT_PIN, LOW);
    delay_ms(18);
    digitalWrite(DHT_PIN, HIGH);
    delay_us(20);

    // 切换为输入模式，等待传感器响应
    pinMode(DHT_PIN, INPUT);
    pullUpDnControl(DHT_PIN, PUD_UP);

    // 等待传感器拉低80us
    while (digitalRead(DHT_PIN) == HIGH) {
        if (retry++ > RETRY_TIMES) return -1;
        delay_us(1);
    }
    retry = 0;
    // 等待传感器释放总线（80us低电平 + 80us高电平）
    while (digitalRead(DHT_PIN) == LOW) {
        if (retry++ > RETRY_TIMES) return -1;
        delay_us(1);
    }
    retry = 0;
    while (digitalRead(DHT_PIN) == HIGH) {
        if (retry++ > RETRY_TIMES) return -1;
        delay_us(1);
    }

    // 读取40位数据
    for (i = 0; i < 5; i++) {
        data[i] = 0;
        for (j = 0; j < 8; j++) {
            // 等待数据位起始低电平（50us）
            while (digitalRead(DHT_PIN) == HIGH);
            while (digitalRead(DHT_PIN) == LOW);

            // 延时40us后检测高电平时间
            delay_us(40);
            if (digitalRead(DHT_PIN) == HIGH) {
                data[i] |= (1 << (7 - j));  // 高位在前
                // 等待剩余高电平时间（逻辑1为70us）
                while (digitalRead(DHT_PIN) == HIGH);
            }
        }
    }

    // 校验数据
    if (data[4] != (data[0] + data[1] + data[2] + data[3])) {
        return -1;
    }
    return 0;
}

// 打印温湿度数据
void DHT_Print() {
    float humidity = (data[0] * 256.0 + data[1]) / 10.0;
    float temperature = ((data[2] & 0x7F) * 256.0 + data[3]) / 10.0;
    if (data[2] & 0x80) temperature = -temperature;

    printf("------------------------\n");
    printf("湿度: %.1f%%RH\n", humidity);
    printf("温度: %.1f℃\n", temperature);
    printf("------------------------\n");
}

int main() {
    if (wiringPiSetup() == -1) {
        printf("GPIO初始化失败！\n");
        return -1;
    }

    DHT_Init();

    while (1) {
        if (DHT_Read() == 0) {
            DHT_Print();
        } else {
            printf("传感器读取失败，请检查连接！\n");
        }
        delay_ms(2000);  // 间隔至少2秒
    }
    return 0;
}
