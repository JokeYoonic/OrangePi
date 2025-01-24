#include <stdio.h>
#include <stdlib.h>
#include <wiringPi.h>
#include <stdint.h>
#include <time.h>  // Include time.h for time functions

#define AM2302_PIN 7  // 假设AM2302连接到GPIO7

typedef struct {
    uint8_t humi_int;      // 湿度的整数部分
    uint8_t humi_deci;     // 湿度的小数部分
    uint8_t temp_int;      // 温度的整数部分
    uint8_t temp_deci;     // 温度的小数部分
    uint8_t check_sum;     // 校验和
} AM2302_Data_TypeDef;

void AM2302_GPIO_Config(void) {
    wiringPiSetup();  // 初始化WiringPi
    pinMode(AM2302_PIN, OUTPUT);  // 设置GPIO为输出模式
    digitalWrite(AM2302_PIN, HIGH);  // 拉高GPIO
    printf("GPIO initialized and set to HIGH.\n");
}

uint8_t Read_Byte(void) {
    uint8_t i, temp = 0;

    for (i = 0; i < 8; i++) {
        while (digitalRead(AM2302_PIN) == LOW) {
            // Debug: Timeout for waiting for HIGH
            delayMicroseconds(1);
        }  // 等待高电平开始

        delayMicroseconds(30);  // 延时30us

        if (digitalRead(AM2302_PIN) == HIGH) {  // 60us后仍为高电平表示数据“1”
            while (digitalRead(AM2302_PIN) == HIGH) {
                // Debug: Timeout for waiting for LOW
                delayMicroseconds(1);
            }  // 等待高电平结束
            temp |= (uint8_t)(0x01 << (7 - i));  // 把第7-i位置1
        } else {  // 60us后为低电平表示数据“0”
            temp &= (uint8_t) ~(0x01 << (7 - i));  // 把第7-i位置0
        }
    }
    printf("Read byte: 0x%02X\n", temp);  // Debug: Print the byte read
    return temp;
}

uint8_t Read_AM2302(AM2302_Data_TypeDef *AM2302_Data) {
    pinMode(AM2302_PIN, OUTPUT);  // 设置GPIO为输出模式
    digitalWrite(AM2302_PIN, LOW);  // 主机拉低
    delay(1);  // 延时1ms，符合Tbe的最小要求
    printf("Start signal sent: Pin LOW for 1ms.\n");

    digitalWrite(AM2302_PIN, HIGH);  // 总线拉高
    delayMicroseconds(30);  // 延时30us，符合Tgo的典型值
    printf("Pin HIGH for 30us.\n");

    pinMode(AM2302_PIN, INPUT);  // 设置GPIO为输入模式
    printf("Switched to INPUT mode.\n");

    if (digitalRead(AM2302_PIN) == LOW) {  // 判断从机是否有低电平响应信号
        printf("Sensor responded with LOW.\n");

        while (digitalRead(AM2302_PIN) == LOW) {
            // Debug: Timeout for waiting for HIGH
            delayMicroseconds(1);
        }  // 等待从机发出的80us低电平响应信号结束
        printf("LOW response ended.\n");

        while (digitalRead(AM2302_PIN) == HIGH) {
            // Debug: Timeout for waiting for LOW
            delayMicroseconds(1);
        }  // 等待从机发出的80us高电平标置信号结束
        printf("HIGH response ended.\n");

        AM2302_Data->humi_int = Read_Byte();
        AM2302_Data->humi_deci = Read_Byte();
        AM2302_Data->temp_int = Read_Byte();
        AM2302_Data->temp_deci = Read_Byte();
        AM2302_Data->check_sum = Read_Byte();

        pinMode(AM2302_PIN, OUTPUT);  // 读取结束，引脚改为输出模式
        digitalWrite(AM2302_PIN, HIGH);  // 主机拉高
        printf("Data read complete. Pin set to HIGH.\n");

        // 检查读取的数据是否正确
        uint8_t calculated_checksum = AM2302_Data->humi_int + AM2302_Data->humi_deci + AM2302_Data->temp_int + AM2302_Data->temp_deci;
        printf("Checksum: Received = 0x%02X, Calculated = 0x%02X\n", AM2302_Data->check_sum, calculated_checksum);

        if (AM2302_Data->check_sum == calculated_checksum) {
            printf("Checksum OK.\n");
            return 1;  // 成功
        } else {
            printf("Checksum ERROR.\n");
            return 0;  // 失败
        }
    } else {
        printf("No response from sensor.\n");
        return 0;  // 失败
    }
}

void Print_Current_Time() {
    time_t rawtime;
    struct tm *timeinfo;

    // Get current time
    time(&rawtime);
    timeinfo = localtime(&rawtime);

    // Print formatted time
    printf("当前时间: %04d-%02d-%02d %02d:%02d:%02d\n",
           timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
           timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
}

int main(void) {
    AM2302_Data_TypeDef AM2302_Data;
    unsigned int RH_Value, TEMP_Value;
    unsigned char RH_H, RH_L, TP_H, TP_L;

    AM2302_GPIO_Config();  // AM2302管脚初始化

    while (1) {
        if (Read_AM2302(&AM2302_Data) == 1) {
            // 计算出实际湿度值的10倍
            RH_Value = AM2302_Data.humi_int * 256 + AM2302_Data.humi_deci;
            RH_H = RH_Value / 10;
            RH_L = RH_Value % 10;

            // 计算出实际温度值的10倍
            TEMP_Value = AM2302_Data.temp_int * 256 + AM2302_Data.temp_deci;
            TP_H = TEMP_Value / 10;
            TP_L = TEMP_Value % 10;

            // Print current time
            Print_Current_Time();

            // Print sensor data
            printf("读取AM2302成功!\r\n湿度为%d.%d ％RH，温度为 %d.%d℃ \r\n", RH_H, RH_L, TP_H, TP_L);
        } else {
            printf("Read AM2302 ERROR!\r\n");
        }

        delay(2000);  // 延时2秒，符合两次读取间隔时间最小为2S的要求
    }

    return 0;
}
