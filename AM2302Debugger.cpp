#include <stdio.h>
#include <stdlib.h>
#include <wiringPi.h>
#include <stdint.h>

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
}

uint8_t Read_Byte(void) {
    uint8_t i, temp = 0;

    for (i = 0; i < 8; i++) {
        while (digitalRead(AM2302_PIN) == LOW);  // 等待高电平开始

        delayMicroseconds(30);  // 延时30us

        if (digitalRead(AM2302_PIN) == HIGH) {  // 60us后仍为高电平表示数据“1”
            while (digitalRead(AM2302_PIN) == HIGH);  // 等待高电平结束
            temp |= (uint8_t)(0x01 << (7 - i));  // 把第7-i位置1
        } else {  // 60us后为低电平表示数据“0”
            temp &= (uint8_t) ~(0x01 << (7 - i));  // 把第7-i位置0
        }
    }
    return temp;
}

uint8_t Read_AM2302(AM2302_Data_TypeDef *AM2302_Data) {
    pinMode(AM2302_PIN, OUTPUT);  // 设置GPIO为输出模式
    digitalWrite(AM2302_PIN, LOW);  // 主机拉低
    delay(2);  // 延时2ms

    digitalWrite(AM2302_PIN, HIGH);  // 总线拉高
    delayMicroseconds(30);  // 延时30us

    pinMode(AM2302_PIN, INPUT);  // 设置GPIO为输入模式

    if (digitalRead(AM2302_PIN) == LOW) {  // 判断从机是否有低电平响应信号
        while (digitalRead(AM2302_PIN) == LOW);  // 等待从机发出的80us低电平响应信号结束
        while (digitalRead(AM2302_PIN) == HIGH);  // 等待从机发出的80us高电平标置信号结束

        AM2302_Data->humi_int = Read_Byte();
        AM2302_Data->humi_deci = Read_Byte();
        AM2302_Data->temp_int = Read_Byte();
        AM2302_Data->temp_deci = Read_Byte();
        AM2302_Data->check_sum = Read_Byte();

        pinMode(AM2302_PIN, OUTPUT);  // 读取结束，引脚改为输出模式
        digitalWrite(AM2302_PIN, HIGH);  // 主机拉高

        // 检查读取的数据是否正确
        if (AM2302_Data->check_sum == AM2302_Data->humi_int + AM2302_Data->humi_deci + AM2302_Data->temp_int + AM2302_Data->temp_deci) {
            return 1;  // 成功
        } else {
            return 0;  // 失败
        }
    } else {
        return 0;  // 失败
    }
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

            printf("\r\n读取AM2302成功!\r\n\r\n湿度为%d.%d ％RH，温度为 %d.%d℃ \r\n", RH_H, RH_L, TP_H, TP_L);
        } else {
            printf("Read AM2302 ERROR!\r\n");
        }

        delay(2000);  // 延时2秒
    }

    return 0;
}
