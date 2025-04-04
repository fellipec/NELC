#include <Arduino.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <time.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <RTClib.h>
#include <wifipasswd.h>  // Contém as credenciais do Wi-Fi

Adafruit_BME280 bme; // I2C
LiquidCrystal_I2C lcd(0x27,20,4);  

// Rotary encoder pins
#define CLK D6
#define DT  D5
#define SW  D7
#define SEALEVELPRESSURE_HPA (1013.25)

// RTC
RTC_DS1307 rtc;

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
const char* daysOfTheWeek[7] = {"Dom", "Seg", "Ter", "Qua", "Qui", "Sex", "Sab"};

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


int counter = 0;
int lastStateCLK;
bool btnToggle = false;
bool lastButtonState = false;
bool btnPressed = false;
unsigned long lastMillis = 0;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50; // 50ms para debounce
bool status;


void setup() {
    Wire.begin();
    Serial.begin(115200);
    lcd.begin(20, 4);
    lcd.setCursor(0, 0);
    lcd.backlight();
    lcd.print("Conectando...");
    bme.begin(0x76); // I2C address 0x76
    rtc.begin();
    Serial.println(rtc.now().timestamp());

    pinMode(CLK, INPUT);
    pinMode(DT, INPUT);
    pinMode(SW, INPUT);

    lastStateCLK = digitalRead(CLK);

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
    lcd.setCursor(0, 0);
    lcd.print("Linha 0");
    lcd.setCursor(0, 1);
    lcd.print("Linha 1");
    lcd.setCursor(0, 2);
    lcd.print("Linha 2");
    lcd.setCursor(0, 3);
    lcd.print("Linha 3");
    delay(2000);

}



void loop() {
    
    // Rotary encoder read
    

    int currentStateCLK = digitalRead(CLK);
    // Detecta mudança no CLK
    if (currentStateCLK != lastStateCLK) {
        if (digitalRead(DT) != currentStateCLK) {
            counter++; // Gira para a direita
        } else {
            counter--; // Gira para a esquerda
        }
    }
    lastStateCLK = currentStateCLK;

    // Encoder button read

    bool currentButtonState = digitalRead(SW);
    if (currentButtonState == LOW && lastButtonState == HIGH) {
        btnPressed = true;
        btnToggle = !btnToggle;
        Serial.println("Botão pressionado!");
    }
    lastButtonState = currentButtonState;

    // Main tasks every second
    if (millis() - lastMillis > 1000) {
        lastMillis = millis();

        lcd.setCursor(0, 1);
        lcd.printf("Pos: %d Btn: %s    ", counter, btnToggle ? "ON" : "OFF");


        if (lastHour == -1) {
            lcd.clear();
        }
        checkNtpFallback();
    
        hour = timeClient.getHours();
        minute = timeClient.getMinutes();
        getDateFromEpoch(timeClient.getEpochTime(), day, month, year);
        dayOfWeek = timeClient.getDay();
    
    
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
                      hour, minute, day, month);
    
        Serial.print("Temperature = ");
        Serial.print(bme.readTemperature());
        Serial.println(" °C");
    
        Serial.print("Pressure = ");
    
        Serial.print(bme.readPressure() / 100.0F);
        Serial.println(" hPa");
    
        Serial.print("Approx. Altitude = ");
        Serial.print(bme.readAltitude(SEALEVELPRESSURE_HPA));
        Serial.println(" m");
    
        Serial.print("Humidity = ");
        Serial.print(bme.readHumidity());
        Serial.println(" %");
    
        Serial.println();

        lcd.setCursor(0, 2);
        lcd.printf("T: %.1fC  ", bme.readTemperature());
        lcd.setCursor(9, 2);
        lcd.printf("U: %.1f%%  ", bme.readHumidity());
        lcd.setCursor(0, 3);
        lcd.printf("P: %.1fhPa  ", bme.readPressure() / 100.0F);

        btnPressed = false; // Reset button state

        
    }
}

