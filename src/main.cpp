#include <Arduino.h>
#include <LiquidCrystal.h>
#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <time.h>
#include <DHT.h>
#include <wifipasswd.h>  // Contém as credenciais do Wi-Fi

// Pinos do LCD Keypad Shield no ESP8266
#define LCD_RS D2
#define LCD_EN D3
#define LCD_D4 D4
#define LCD_D5 D5
#define LCD_D6 D6
#define LCD_D7 D7

// Inicializa o LCD (sem I2C)
LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

// Pinos do sensor DHT22
#define DHTPIN D8
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// Fuso horário (UTC-3)
const long utcOffsetInSeconds = -10800;

// Servidores NTP
const char* ntpServers[] = {"c.ntp.br", "b.ntp.br", "a.ntp.br", "pool.ntp.org"};
const int numNtpServers = sizeof(ntpServers) / sizeof(ntpServers[0]);
int ntpServerIndex = 0;
int failedSyncCount = 0;

// Configuração do NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntpServers[ntpServerIndex], utcOffsetInSeconds, 60000);

// Dias da semana
const char* daysOfTheWeek[7] = {"Dom", "Seg", "Ter", "Qua", "Qui", "Sex", "Sáb"};

// Variáveis globais
int day, month, year, hour, minute, dayOfWeek;
int lastHour = -1, lastMinute = -1, lastDay = -1, lastMonth = -1, lastDayOfWeek = -1;
float lastTemp = -1000, lastHum = -1000;

void checkNtpFallback() {
    timeClient.update();
    if (timeClient.getEpochTime() == 0) {
        failedSyncCount++;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Falha NTP!");
        lcd.setCursor(0, 1);
        lcd.print(ntpServers[ntpServerIndex]);
        Serial.println("Falha na sincronização NTP!");
        if (failedSyncCount >= 5) {
            failedSyncCount = 0;
            ntpServerIndex = (ntpServerIndex + 1) % numNtpServers;
            timeClient.setPoolServerName(ntpServers[ntpServerIndex]);
            Serial.printf("Trocando para servidor NTP: %s\n", ntpServers[ntpServerIndex]);
        }
    } else {
        failedSyncCount = 0;
    }
}

void getDateFromEpoch(time_t epoch, int &day, int &month, int &year) {
    struct tm timeinfo;
    gmtime_r(&epoch, &timeinfo);

    day = timeinfo.tm_mday;
    month = timeinfo.tm_mon + 1;
    year = timeinfo.tm_year + 1900;
}

void setup() {
    Serial.begin(115200);
    lcd.begin(16, 2);
    lcd.setCursor(0, 0);
    lcd.print("Conectando...");

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    int i = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        lcd.setCursor(i, 1);
        lcd.print(".");
        if (i > 0) {
            lcd.setCursor(i - 1, 1);
            lcd.print(" ");
        }
        i++;
        if (i > 15) { i = 0; }
    }
    Serial.println("\nWi-Fi Conectado!");
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Wi-Fi OK!");

    timeClient.begin();
    dht.begin();
}

void loop() {
    if (lastHour == -1) {
        lcd.clear();
    }
    checkNtpFallback();

    hour = timeClient.getHours();
    minute = timeClient.getMinutes();
    getDateFromEpoch(timeClient.getEpochTime(), day, month, year);
    dayOfWeek = timeClient.getDay();

    float temp = dht.readTemperature();
    float hum = dht.readHumidity();

    if (isnan(temp) || isnan(hum)) {
        Serial.println("Erro ao ler DHT22");
    } else {
        if (temp != lastTemp || hum != lastHum) {
            lcd.setCursor(0, 1);
            lcd.printf("T:%5.1fC H:%5.1f%%  ", temp, hum);
            lastTemp = temp;
            lastHum = hum;
        }
    }

    if (hour != lastHour || minute != lastMinute || day != lastDay || month != lastMonth || dayOfWeek != lastDayOfWeek) {
        lcd.setCursor(0, 0);
        lcd.printf("%02d:%02d  %s %02d/%02d  ", hour, minute, daysOfTheWeek[dayOfWeek], day, month);

        lastHour = hour;
        lastMinute = minute;
        lastDay = day;
        lastMonth = month;
        lastDayOfWeek = dayOfWeek;
    }

    Serial.printf("Hora: %02d:%02d | Data: %02d/%02d | Temp: %.1f°C | Umid: %.1f%%\n", 
                  hour, minute, day, month, temp, hum);
    
    delay(5000);
}
