#include <stdio.h>
#include <stdlib.h>
#include <wiringPi.h>
#include <stdint.h>

#define AM2302_PIN 7  // 假设 AM2302 连接到 GPIO7

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
    pinMode(AM2302_PIN, OUTPUT);  // 设置 GPIO 为输出模式
    printf("DEBUG: GPIO pin %d set to OUTPUT mode.\n", AM2302_PIN);

    printf("DEBUG: Setting GPIO pin %d to HIGH...\n", AM2302_PIN);
    digitalWrite(AM2302_PIN, HIGH);  // 拉高 GPIO
    printf("DEBUG: GPIO pin %d set to HIGH.\n", AM2302_PIN);
}

uint8_t Read_Byte(void) {
    uint8_t i, temp = 0;

    for (i = 0; i < 8; i++) {
        int timeout = 1000;  // 超时时间（单位：微秒）
        while (digitalRead(AM2302_PIN) == LOW) {
            if (--timeout == 0) {
                printf("DEBUG: Timeout while waiting for HIGH signal.\n");
                return 0xFF;  // 返回错误值
            }
            delayMicroseconds(1);
        }

        delayMicroseconds(30);

        if (digitalRead(AM2302_PIN) == HIGH) {
            timeout = 1000;
            while (digitalRead(AM2302_PIN) == HIGH) {
                if (--timeout == 0) {
                    printf("DEBUG: Timeout while waiting for LOW signal.\n");
                    return 0xFF;  // 返回错误值
                }
                delayMicroseconds(1);
            }
            temp |= (uint8_t)(0x01 << (7 - i));
            printf("DEBUG: Bit %d is '1'.\n", 7 - i);
        } else {
            temp &= (uint8_t) ~(0x01 << (7 - i));
            printf("DEBUG: Bit %d is '0'.\n", 7 - i);
        }
    }
    printf("DEBUG: Read byte: 0x%02X\n", temp);
    return temp;
}

uint8_t Read_AM2302(AM2302_Data_TypeDef *AM2302_Data) {
    printf("DEBUG: Starting AM2302 read process...\n");

    pinMode(AM2302_PIN, OUTPUT);
    digitalWrite(AM2302_PIN, LOW);
    delay(1);

    digitalWrite(AM2302_PIN, HIGH);
    delayMicroseconds(30);

    pinMode(AM2302_PIN, INPUT);

    int timeout = 1000;  // 超时时间（单位：微秒）
    while (digitalRead(AM2302_PIN) == HIGH) {
        if (--timeout == 0) {
            printf("DEBUG: Timeout while waiting for sensor response.\n");
            return 0;  // 返回失败
        }
        delayMicroseconds(1);
    }

    timeout = 1000;
    while (digitalRead(AM2302_PIN) == LOW) {
        if (--timeout == 0) {
            printf("DEBUG: Timeout while waiting for LOW response to end.\n");
            return 0;  // 返回失败
        }
        delayMicroseconds(1);
    }

    timeout = 1000;
    while (digitalRead(AM2302_PIN) == HIGH) {
        if (--timeout == 0) {
            printf("DEBUG: Timeout while waiting for HIGH response to end.\n");
            return 0;  // 返回失败
        }
        delayMicroseconds(1);
    }

    AM2302_Data->humi_int = Read_Byte();
    AM2302_Data->humi_deci = Read_Byte();
    AM2302_Data->temp_int = Read_Byte();
    AM2302_Data->temp_deci = Read_Byte();
    AM2302_Data->check_sum = Read_Byte();

    pinMode(AM2302_PIN, OUTPUT);
    digitalWrite(AM2302_PIN, HIGH);

    uint8_t calculated_checksum = AM2302_Data->humi_int + AM2302_Data->humi_deci + AM2302_Data->temp_int + AM2302_Data->temp_deci;
    printf("DEBUG: Calculated checksum: 0x%02X\n", calculated_checksum);

    if (AM2302_Data->check_sum == calculated_checksum) {
        printf("DEBUG: Checksum validation passed.\n");
        return 1;  // 成功
    } else {
        printf("DEBUG: Checksum validation failed.\n");
        return 0;  // 失败
    }
}

// 获取当前时间戳
void get_timestamp(char *timestamp, size_t size) {
    FILE *fp;
    char buffer[64];

    // 使用系统命令获取时间戳
    fp = popen("date '+%Y-%m-%d %H:%M:%S'", "r");
    if (fp == NULL) {
        printf("DEBUG: Failed to get timestamp.\n");
        return;
    }

    // 读取命令输出
    if (fgets(buffer, sizeof(buffer), fp) != NULL) {
        // 去掉换行符
        buffer[strcspn(buffer, "\n")] = '\0';
        snprintf(timestamp, size, "%s", buffer);
    }

    pclose(fp);
}

// 保存数据到 CSV 文件
void save_to_csv(const char *timestamp, float humidity, float temperature) {
    FILE *file = fopen("sensor_data.csv", "a");  // 以追加模式打开文件
    if (file == NULL) {
        printf("DEBUG: Failed to open file for writing.\n");
        return;
    }

    // 如果是第一次写入，添加 CSV 文件头
    if (ftell(file) == 0) {
        fprintf(file, "Timestamp,Humidity (%%RH),Temperature (℃)\n");
    }

    // 写入时间戳和传感器数据
    fprintf(file, "%s,%.1f,%.1f\n", timestamp, humidity, temperature);
    fclose(file);

    printf("DEBUG: Data saved to CSV file.\n");
}

int main(void) {
    AM2302_Data_TypeDef AM2302_Data;
    unsigned int RH_Value, TEMP_Value;
    unsigned char RH_H, RH_L, TP_H, TP_L;
    int retry_count = 0;
    const int max_retries = 5;  // 最大重试次数
    char timestamp[20];  // 用于存储时间戳

    printf("DEBUG: Initializing AM2302 GPIO configuration...\n");
    AM2302_GPIO_Config();  // AM2302 管脚初始化
    printf("DEBUG: AM2302 GPIO configuration complete.\n");

    while (1) {
        printf("DEBUG: Starting AM2302 read cycle...\n");
        if (Read_AM2302(&AM2302_Data) == 1) {
            retry_count = 0;  // 重置重试计数器

            // 计算出实际湿度值的 10 倍
            RH_Value = AM2302_Data.humi_int * 256 + AM2302_Data.humi_deci;
            RH_H = RH_Value / 10;
            RH_L = RH_Value % 10;
            float humidity = RH_H + (float)RH_L / 10.0;

            // 计算出实际温度值的 10 倍
            TEMP_Value = AM2302_Data.temp_int * 256 + AM2302_Data.temp_deci;
            TP_H = TEMP_Value / 10;
            TP_L = TEMP_Value % 10;
            float temperature = TP_H + (float)TP_L / 10.0;

            // 获取当前时间戳
            get_timestamp(timestamp, sizeof(timestamp));

            // 打印传感器数据
            printf("\r\n读取AM2302成功!\r\n\r\n湿度为%d.%d ％RH，温度为 %d.%d℃ \r\n", RH_H, RH_L, TP_H, TP_L);

            // 保存数据到 CSV 文件
            save_to_csv(timestamp, humidity, temperature);
        } else {
            retry_count++;
            printf("Read AM2302 ERROR! Retry count: %d\n", retry_count);

            if (retry_count >= max_retries) {
                printf("DEBUG: Maximum retries reached. Resetting GPIO...\n");
                AM2302_GPIO_Config();  // 重置 GPIO 配置
                retry_count = 0;
            }
        }

        printf("DEBUG: Delaying 2 seconds before next read...\n");
        delay(2000);  // 延时 2 秒，符合两次读取间隔时间最小为 2S 的要求
        printf("DEBUG: 2-second delay complete.\n");
    }

    return 0;
}
