#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <time.h>
#include <DHT.h>
#include <wifipasswd.h>  // Contém as credenciais do Wi-Fi

// Pinos do LCD
#define DHTPIN D4      // Pino do DHT22
#define DHTTYPE DHT22  // Modelo do sensor

// Fuso horário (UTC-3) Brasília
const long utcOffsetInSeconds = -10800;

// Servidores NTP
const char* ntpServers[] = {"c.ntp.br", "b.ntp.br", "a.ntp.br", "pool.ntp.org"};
const int numNtpServers = sizeof(ntpServers) / sizeof(ntpServers[0]);
int ntpServerIndex = 0;
int failedSyncCount = 0;


// Inicializa LCD no endereço 0x27 (ou 0x3F, dependendo do módulo)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Inicializa sensor DHT
DHT dht(DHTPIN, DHTTYPE);

// Configuração do NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntpServers[ntpServerIndex], utcOffsetInSeconds, 60000); // UTC-3 (Brasil)

// Dias da semana
const char* daysOfTheWeek[7] = {"Dom", "Seg", "Ter", "Qua", "Qui", "Sex", "Sáb"};

// Variáveis globais
int day, month, year, hour, minute, dayOfWeek;
int lastHour = -1, lastMinute = -1, lastDay = -1, lastMonth = -1, lastDayOfWeek = -1;
float lastTemp = -1000, lastHum = -1000;

void checkNtpFallback() {
    timeClient.update();
    if (timeClient.getEpochTime() == 0) {  // Falha na sincronização
        failedSyncCount++;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Falha NTP!");
        lcd.setCursor(0, 1);
        lcd.print(ntpServers[ntpServerIndex]);
        Serial.println("Falha na sincronização NTP!");
        if (failedSyncCount >= 5) { // Após 5 falhas, trocar o servidor
            failedSyncCount = 0;
            ntpServerIndex = (ntpServerIndex + 1) % numNtpServers;  // Alterna entre servidores
            timeClient.setPoolServerName(ntpServers[ntpServerIndex]);
            Serial.printf("Trocando para servidor NTP: %s\n", ntpServers[ntpServerIndex]);
        }
    } else {
        failedSyncCount = 0; // Reset se a sincronização funcionar
    }
}

void getDateFromEpoch(time_t epoch, int &day, int &month, int &year) {
    struct tm timeinfo;
    gmtime_r(&epoch, &timeinfo);  // Converte epoch para estrutura de tempo UTC

    day = timeinfo.tm_mday;
    month = timeinfo.tm_mon + 1;  // tm_mon vai de 0 a 11, então somamos 1
    year = timeinfo.tm_year + 1900;  // tm_year é anos desde 1900
}

// Configuração inicial
void setup() {
    Serial.begin(115200);
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("Conectando...");

    // Conectar ao Wi-Fi
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    int i  = 0;
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

    // Iniciar NTP e DHT
    timeClient.begin();
    dht.begin();
}

void loop() {
    if (lastHour == -1) {
        lcd.clear();
    }
    checkNtpFallback(); // Verifica e troca o servidor se necessário

    hour = timeClient.getHours();
    minute = timeClient.getMinutes();
    getDateFromEpoch(timeClient.getEpochTime(), day, month, year);
    dayOfWeek = timeClient.getDay();  // 0=Domingo, 6=Sábado

    float temp = dht.readTemperature();
    float hum = dht.readHumidity();

    // Evita exibir valores inválidos do DHT
    if (isnan(temp) || isnan(hum)) {
        Serial.println("Erro ao ler DHT22");
    } else {
        // Atualiza temperatura e umidade apenas se mudar
        if (temp != lastTemp || hum != lastHum) {
            lcd.setCursor(0, 1);
            lcd.printf("T:%5.1fC H:%5.1f%%  ", temp, hum); // Espaços para evitar caracteres residuais

            lastTemp = temp;
            lastHum = hum;
        }
    }

    // Atualiza hora e data apenas se mudar
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
    
    delay(5000); // Atualiza a cada 5s
}
