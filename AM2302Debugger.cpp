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
    printf("DEBUG: Initializing WiringPi...\n");
    if (wiringPiSetup() == -1) {
        printf("DEBUG: WiringPi initialization failed!\n");
        exit(1);
    }
    printf("DEBUG: WiringPi initialized successfully.\n");

    printf("DEBUG: Setting GPIO pin %d to OUTPUT mode...\n", AM2302_PIN);
    pinMode(AM2302_PIN, OUTPUT);  // 设置GPIO为输出模式
    printf("DEBUG: GPIO pin %d set to OUTPUT mode.\n", AM2302_PIN);

    printf("DEBUG: Setting GPIO pin %d to HIGH...\n", AM2302_PIN);
    digitalWrite(AM2302_PIN, HIGH);  // 拉高GPIO
    printf("DEBUG: GPIO pin %d set to HIGH.\n", AM2302_PIN);
}

uint8_t Read_Byte(void) {
    uint8_t i, temp = 0;

    for (i = 0; i < 8; i++) {
        printf("DEBUG: Waiting for HIGH signal on pin %d...\n", AM2302_PIN);
        while (digitalRead(AM2302_PIN) == LOW);  // 等待高电平开始

        printf("DEBUG: HIGH signal detected. Delaying 30us...\n");
        delayMicroseconds(30);  // 延时30us

        if (digitalRead(AM2302_PIN) == HIGH) {  // 60us后仍为高电平表示数据“1”
            printf("DEBUG: Bit %d is '1'.\n", 7 - i);
            while (digitalRead(AM2302_PIN) == HIGH);  // 等待高电平结束
            temp |= (uint8_t)(0x01 << (7 - i));  // 把第7-i位置1
        } else {  // 60us后为低电平表示数据“0”
            printf("DEBUG: Bit %d is '0'.\n", 7 - i);
            temp &= (uint8_t) ~(0x01 << (7 - i));  // 把第7-i位置0
        }
    }
    printf("DEBUG: Read byte: 0x%02X\n", temp);
    return temp;
}

uint8_t Read_AM2302(AM2302_Data_TypeDef *AM2302_Data) {
    printf("DEBUG: Starting AM2302 read process...\n");

    printf("DEBUG: Setting GPIO pin %d to OUTPUT mode...\n", AM2302_PIN);
    pinMode(AM2302_PIN, OUTPUT);  // 设置GPIO为输出模式
    printf("DEBUG: GPIO pin %d set to OUTPUT mode.\n", AM2302_PIN);

    printf("DEBUG: Setting GPIO pin %d to LOW...\n", AM2302_PIN);
    digitalWrite(AM2302_PIN, LOW);  // 主机拉低
    printf("DEBUG: GPIO pin %d set to LOW.\n", AM2302_PIN);

    printf("DEBUG: Delaying 1ms...\n");
    delay(1);  // 延时1ms，符合Tbe的最小要求
    printf("DEBUG: 1ms delay complete.\n");

    printf("DEBUG: Setting GPIO pin %d to HIGH...\n", AM2302_PIN);
    digitalWrite(AM2302_PIN, HIGH);  // 总线拉高
    printf("DEBUG: GPIO pin %d set to HIGH.\n", AM2302_PIN);

    printf("DEBUG: Delaying 30us...\n");
    delayMicroseconds(30);  // 延时30us，符合Tgo的典型值
    printf("DEBUG: 30us delay complete.\n");

    printf("DEBUG: Setting GPIO pin %d to INPUT mode...\n", AM2302_PIN);
    pinMode(AM2302_PIN, INPUT);  // 设置GPIO为输入模式
    printf("DEBUG: GPIO pin %d set to INPUT mode.\n", AM2302_PIN);

    if (digitalRead(AM2302_PIN) == LOW) {  // 判断从机是否有低电平响应信号
        printf("DEBUG: Sensor responded with LOW signal.\n");

        printf("DEBUG: Waiting for LOW response to end...\n");
        while (digitalRead(AM2302_PIN) == LOW);  // 等待从机发出的80us低电平响应信号结束
        printf("DEBUG: LOW response ended.\n");

        printf("DEBUG: Waiting for HIGH response to end...\n");
        while (digitalRead(AM2302_PIN) == HIGH);  // 等待从机发出的80us高电平标置信号结束
        printf("DEBUG: HIGH response ended.\n");

        printf("DEBUG: Reading humidity integer part...\n");
        AM2302_Data->humi_int = Read_Byte();
        printf("DEBUG: Humidity integer part: 0x%02X\n", AM2302_Data->humi_int);

        printf("DEBUG: Reading humidity decimal part...\n");
        AM2302_Data->humi_deci = Read_Byte();
        printf("DEBUG: Humidity decimal part: 0x%02X\n", AM2302_Data->humi_deci);

        printf("DEBUG: Reading temperature integer part...\n");
        AM2302_Data->temp_int = Read_Byte();
        printf("DEBUG: Temperature integer part: 0x%02X\n", AM2302_Data->temp_int);

        printf("DEBUG: Reading temperature decimal part...\n");
        AM2302_Data->temp_deci = Read_Byte();
        printf("DEBUG: Temperature decimal part: 0x%02X\n", AM2302_Data->temp_deci);

        printf("DEBUG: Reading checksum...\n");
        AM2302_Data->check_sum = Read_Byte();
        printf("DEBUG: Checksum: 0x%02X\n", AM2302_Data->check_sum);

        printf("DEBUG: Setting GPIO pin %d to OUTPUT mode...\n", AM2302_PIN);
        pinMode(AM2302_PIN, OUTPUT);  // 读取结束，引脚改为输出模式
        printf("DEBUG: GPIO pin %d set to OUTPUT mode.\n", AM2302_PIN);

        printf("DEBUG: Setting GPIO pin %d to HIGH...\n", AM2302_PIN);
        digitalWrite(AM2302_PIN, HIGH);  // 主机拉高
        printf("DEBUG: GPIO pin %d set to HIGH.\n", AM2302_PIN);

        // 检查读取的数据是否正确
        uint8_t calculated_checksum = AM2302_Data->humi_int + AM2302_Data->humi_deci + AM2302_Data->temp_int + AM2302_Data->temp_deci;
        printf("DEBUG: Calculated checksum: 0x%02X\n", calculated_checksum);

        if (AM2302_Data->check_sum == calculated_checksum) {
            printf("DEBUG: Checksum validation passed.\n");
            return 1;  // 成功
        } else {
            printf("DEBUG: Checksum validation failed.\n");
            return 0;  // 失败
        }
    } else {
        printf("DEBUG: No response from sensor.\n");
        return 0;  // 失败
    }
}

int main(void) {
    AM2302_Data_TypeDef AM2302_Data;
    unsigned int RH_Value, TEMP_Value;
    unsigned char RH_H, RH_L, TP_H, TP_L;

    printf("DEBUG: Initializing AM2302 GPIO configuration...\n");
    AM2302_GPIO_Config();  // AM2302管脚初始化
    printf("DEBUG: AM2302 GPIO configuration complete.\n");

    while (1) {
        printf("DEBUG: Starting AM2302 read cycle...\n");
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

        printf("DEBUG: Delaying 2 seconds before next read...\n");
        delay(2000);  // 延时2秒，符合两次读取间隔时间最小为2S的要求
        printf("DEBUG: 2-second delay complete.\n");
    }

    return 0;
}
