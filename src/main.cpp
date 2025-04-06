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
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <wifipasswd.h>  // Wi-Fi credentials
#include <apikeys.h>     // OpenWeatherMap API key


// I²C Addresses
// 0x27 → LCD I2C (PCF8574 Controller)
// 0x50 → Tiny RTC EEPROM
// 0x68 → RTC DS1307 (Real Time Clock)
// 0x76 → BME280 Sensor (Temperature, Humidity and Pressure)

// BME280
Adafruit_BME280 bme;
#define SEALEVELPRESSURE_HPA (1013.25)
#define BME_READ_INTERVAL 60000

// LiquidCrystal_I2C Controller
LiquidCrystal_I2C lcd(0x27,20,4);
#include <digits.h>

// Rotary encoder pins
#define CLK D6
#define DT  D5
#define SW  D7

// RTC
RTC_DS1307 rtc;

// Fuso horário (UTC-3)
const long utcOffsetInSeconds = -10800;

// NTP Server List. Change to your preferred servers
const char* ntpServers[] = {
    "scarlett",                         // Local NTP Server
    "a.ntp.br","b.ntp.br","c.ntp.br",   // Official Brazilian NTP Server
    "time.nist.gov",                    // USA NTP Server
    "pool.ntp.org"                      // NTP Pool
};
const int numNTPServers = sizeof(ntpServers) / sizeof(ntpServers[0]);
int ntpSrvIndex = 0;
int numRedes = sizeof(ssids) / sizeof(ssids[0]);  // Number of Wi-Fi networks in wifipasswd.h

// OpenWeatherMap API
const char* apiKey = OWM_APIKEY; // Change for your API key
const char* lon = "-49.2908"; // Change coordinates for your city
const char* lat = "-25.504";
#define MAX_REQUEST_SIZE 512
#define MAX_RESPONSE_SIZE 1024
#define MAX_JSON_SIZE 768
#define FETCH_INTERVAL 900 // Fetch weather data every 15 minutes


// NTP Configuration
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntpServers[ntpSrvIndex], utcOffsetInSeconds, 60000);
WiFiClientSecure client;

// Dias da semana
const char* daysOfTheWeek[7] = {"Dom", "Seg", "Ter", "Qua", "Qui", "Sex", "Sab"};

/* 
 * GLOBAL VARIABLES 
 */


// General variables
int day, month, year, hour, minute, second, dayOfWeek;
unsigned long  lastRTCsync = 0;
float tmp, hum, pres, alt;
float lastTemp = -1000, lastHum = -1000;
bool wifiConnected = false, ntpConnected = false;
int lastUIScreen = 0;
unsigned long lastUIMillis = 0;
unsigned long lastBMERead = 0;

// Rotary encoder variables
int counter = 0;
int lastCounter = 0;
int lastStateCLK;
bool btnToggle = false;
bool lastButtonState = false;
bool btnPressed = false;
unsigned long lastMillis = 0;
int UIMax = 1; 
int UIMin = -1; 


// Weather variables
float current_temp = 0.0;
float current_feels_like = 0.0;
float current_temp_min = 0.0;
float current_temp_max = 0.0;
int current_pressure = 0.0;
int current_humidity = 0.0;
char current_weatherDescription[21]; // 20 chars + '\0'
char location_name[21]; // 20 chars + '\0'
long current_sunset = 0;
long current_sunrise = 0;
long current_dt = 0;




bool tryWIFI() {
    bool connOK = false;
    const char* gizmo[] = {"|", ">", "=", "<"}; //Wi-Fi loading animation
    for (int i = 0; i < numRedes; i++) {        
        Serial.print("Tentando conectar em ");
        Serial.print(ssids[i]);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Conectando em:");
        lcd.setCursor(0, 1);
        lcd.print("               ");
        lcd.setCursor(0, 1);
        lcd.print(ssids[i]);
        WiFi.begin(ssids[i], passwords[i]);

        int tentativa = 0;
        int j = 0;  // Index for gizmo array
        // Retry connection up to 10 seconds (10 attempts)
        while (WiFi.status() != WL_CONNECTED && tentativa < 20) {
            delay(250);
            Serial.print(".");
            lcd.setCursor(19, 1);
            lcd.print(gizmo[j]);  // Display some progress information
            j = (j + 1) % 4;  // Cycle through the gizmo array
            tentativa++;
        }
        
        // If connected successfully to Wi-Fi
        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("\nConectado em %s\n", ssids[i]);
            lcd.clear();
            lcd.print("Conectado ao ");
            lcd.setCursor(0, 1);
            lcd.print("Wi-Fi: ");
            lcd.print(ssids[i]);
            connOK = true;
            break;  // Exit loop if connection is successful
        } else {
            Serial.printf("\nFalha ao conectar no Wi-Fi: %s\n", ssids[i]);
        }
    }
    lcd.clear();
    return connOK;
}

/*
 * tryNTPServer() - Tries to connect to a list of NTP servers
 * 
 * This function attempts to establish a connection with a list of NTP (Network Time Protocol)
 * servers to synchronize the time. It loops through the predefined list of NTP server addresses
 * and tries to update the time using each server. If a successful connection is made, it returns
 * the index of the server that was successfully connected to. If all servers fail, it returns -1.
 */
int tryNTPServer() {
    for (int i = 0; i < numNTPServers; i++) {
        timeClient.setPoolServerName(ntpServers[i]);
        timeClient.begin();
        if (timeClient.update()) {
            Serial.printf("Conexão com NTP bem-sucedida: %s\n ", ntpServers[i]);
            return i;
        } else {
            Serial.printf("Erro ao conectar no NTP: %s\n", ntpServers[i]);
        }
    }
    return -1;
}

void getDateFromEpoch(time_t epoch, int &day, int &month, int &year) {
    struct tm timeinfo;
    gmtime_r(&epoch, &timeinfo);

    day = timeinfo.tm_mday;
    month = timeinfo.tm_mon + 1;
    year = timeinfo.tm_year + 1900;
}

void getDayOfWeekFromEpoch(time_t epoch, int &dayOfWeek) {
    struct tm timeinfo;
    gmtime_r(&epoch, &timeinfo); 

    dayOfWeek = timeinfo.tm_wday;
}


void getTimeFromEpoch(time_t epoch, int &hour, int &minute, int &second) {
    struct tm timeinfo;
    gmtime_r(&epoch, &timeinfo); 

    hour = timeinfo.tm_hour;
    minute = timeinfo.tm_min;
    second = timeinfo.tm_sec;
}

void upperFirstLetter(char* str) {
    if (str && str[0] != '\0') {  // Checks for empty string        
      str[0] = toupper(str[0]);  // Convert the first character to uppercase
    }
  }


void readBME() {
    if (millis() - lastBMERead > BME_READ_INTERVAL || lastBMERead == 0) {
        lastBMERead = millis();
        bme.takeForcedMeasurement();
        tmp = bme.readTemperature();
        hum = bme.readHumidity();
        pres = bme.readPressure() / 100.0F;
        alt = bme.readAltitude(SEALEVELPRESSURE_HPA);    
    }
}

void readNTP() {
    timeClient.update();
    hour = timeClient.getHours();
    minute = timeClient.getMinutes();
    second = timeClient.getSeconds();
    getDateFromEpoch(timeClient.getEpochTime(), day, month, year);
    dayOfWeek = timeClient.getDay();
}

void readRTC() {
    DateTime now = rtc.now();
    hour = now.hour();
    minute = now.minute();
    second = now.second();
    day = now.day();
    month = now.month();
    year = now.year();    
    dayOfWeek = now.dayOfTheWeek();
}

void setRTC() {
    readNTP();
    rtc.adjust(DateTime(year, month, day, hour, minute, second));
    Serial.printf("RTC set to: %02d:%02d:%02d %02d/%02d/%04d\n", hour, minute, second, day, month, year);
}

bool isRTCSync() {
    readNTP();
    DateTime ntpnow = DateTime(year, month, day, hour, minute, second);
    DateTime rtcnow = rtc.now();
    Serial.printf("RTC: %02d:%02d:%02d %02d/%02d/%04d\n", rtcnow.hour(), rtcnow.minute(), rtcnow.second(), rtcnow.day(), rtcnow.month(), rtcnow.year());
    Serial.printf("NTP: %02d:%02d:%02d %02d/%02d/%04d\n", ntpnow.hour(), ntpnow.minute(), ntpnow.second(), ntpnow.day(), ntpnow.month(), ntpnow.year());
    if (rtcnow.year() != ntpnow.year() || rtcnow.month() != ntpnow.month() || rtcnow.day() != ntpnow.day() ||
        rtcnow.hour() != ntpnow.hour() || rtcnow.minute() != ntpnow.minute() || rtcnow.second() != ntpnow.second()) {
        return false;
    } else {
        return true;
    }
}

bool checkConnections() {
    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
    }
    else {
        wifiConnected = tryWIFI();
        ntpConnected = false;
    }
    if (wifiConnected) {
        if (timeClient.update()) {
            ntpConnected = true;
        }
        else {
            ntpConnected = tryNTPServer();
        }
    }
    return wifiConnected && ntpConnected;
}

void buildOWMRequest(char* request, const char* lat, const char* lon, const char* apiKey) {
    snprintf(request, MAX_REQUEST_SIZE, 
             "GET /data/2.5/weather?lat=%s&lon=%s&appid=%s&units=metric&lang=pt_br HTTP/1.1\r\n"
             "Host: api.openweathermap.org\r\n"
             "Connection: close\r\n\r\n", 
             lat, lon, apiKey);
}


void getWeather() {
    if (rtc.now().unixtime() - current_dt > FETCH_INTERVAL) {
        if (!client.connect("api.openweathermap.org", 443)) { 
        Serial.println("Falha ao conectar ao servidor.");
        return;
        }
        char request[MAX_REQUEST_SIZE];
        buildOWMRequest(request, lat, lon, apiKey);
        
        client.print(request); 
    
        unsigned long timeout = millis();
        while (client.available() == 0) { 
        if (millis() - timeout > 5000) { // 5 seconds timeout
            Serial.println("Erro: Timeout.");
            client.stop();
            return;
        }
        }
    
        // Payload buffer
        // User this while loop to avoid the Strings object
        // to avoid memory fragmentation
        char weatherJson[MAX_RESPONSE_SIZE]; 
        unsigned int index = 0;

        while (client.available()) {
            if (index < sizeof(weatherJson) - 1) {  // Buffer limit check
                weatherJson[index++] = (char)client.read();  // Add the next character to the buffer
            } else {
                break;  // Buffer is full, stop reading
            }
        }
        weatherJson[index] = '\0';  // Add null terminator to the string

        // Find the JSON start position
        char* jsonStart = strchr(weatherJson, '{');  // First { character
        if (jsonStart) {
            // Copy the JSON part to the payload
            strcpy(weatherJson, jsonStart);
        } else {
            Serial.println("Erro: JSON não encontrado na resposta.");
            return;
        }
    
        // Print the payload for debugging
         Serial.println(weatherJson);
    
        // JSON parsing   
        JsonDocument doc;

        DeserializationError error = deserializeJson(doc, weatherJson, MAX_RESPONSE_SIZE);

        if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
        }
        
        Serial.println("JSON parsed");
        JsonObject weather_0 = doc["weather"][0];
        const char* desc = weather_0["description"] | ""; 
        strncpy(current_weatherDescription, desc, sizeof(current_weatherDescription)); // Copy string to avoid null pointer
        current_weatherDescription[sizeof(current_weatherDescription) - 1] = '\0'; // add null terminator
        upperFirstLetter(current_weatherDescription); // Capitalize first letter
        const char* name = doc["name"] | "";
        strncpy(location_name, name, sizeof(location_name)); // Copy string to avoid null pointer
        location_name[sizeof(location_name) - 1] = '\0'; // add null terminator
        upperFirstLetter(location_name); // Capitalize first letter

        JsonObject main = doc["main"];
        current_temp= main["temp"]; 
        current_feels_like = main["feels_like"]; 
        current_temp_min = main["temp_min"]; 
        current_temp_max = main["temp_max"]; 
        current_pressure = main["pressure"]; 
        current_humidity = main["humidity"]; 
        current_dt = doc["dt"];
        current_dt += utcOffsetInSeconds;

        JsonObject sys = doc["sys"];
        current_sunset = sys["sunset"];
        current_sunrise = sys["sunrise"];

        

        Serial.printf("Clima: %s\n", current_weatherDescription);
        Serial.printf("Temp: %.1f C\n", current_temp);
        Serial.printf("Min: %.1f C\n", current_temp_min);
        Serial.printf("Max: %.1f C\n", current_temp_max);
        Serial.printf("Sensação: %.1f C\n", current_feels_like);
        Serial.printf("Umidade: %d%%\n", current_humidity);
        Serial.printf("Pressão: %d hPa\n", current_pressure);
        Serial.printf("Localização: %s\n", location_name);
        Serial.printf("Data: %ld\n", current_dt);
        Serial.printf("Nascer do sol: %ld\n", current_sunrise);
        Serial.printf("Pôr do sol: %ld\n", current_sunset);
        Serial.printf("Latitude: %s\n", lat);
        Serial.printf("Longitude: %s\n", lon);
        

    }
 
  }

  #define EEPROM_ADDRESS 0x50  // Endereço da EEPROM externa (0x50)

  // Função para escrever na EEPROM externa
  void writeEEPROM(uint16_t addr, byte data) {
    Wire.beginTransmission(EEPROM_ADDRESS);
    Wire.write((uint8_t)(addr >> 8));  // Endereço alto (MSB)
    Wire.write((uint8_t)(addr & 0xFF));  // Endereço baixo (LSB)
    Wire.write(data);  // Dados a serem escritos
    Wire.endTransmission();
    delay(5);  // Aguardar o tempo necessário para a escrita (depende do modelo da EEPROM)
  }
  
  // Função para ler da EEPROM externa
  byte readEEPROM(uint16_t addr) {
    Wire.beginTransmission(EEPROM_ADDRESS);
    Wire.write((uint8_t)(addr >> 8));  // Endereço alto (MSB)
    Wire.write((uint8_t)(addr & 0xFF));  // Endereço baixo (LSB)
    Wire.endTransmission();
    
    Wire.requestFrom(EEPROM_ADDRESS, 1);  // Solicitar 1 byte de dados
    return Wire.read();  // Retorna o byte lido
  }

void setup() {
    Wire.begin();
    Serial.begin(115200);
    lcd.begin(20, 4);
    lcd.setCursor(0, 0);
    lcd.backlight();
    bme.begin(0x76); // I2C address 0x76
    bme.setSampling(Adafruit_BME280::MODE_FORCED,
                    Adafruit_BME280::SAMPLING_X1,  // temperature
                    Adafruit_BME280::SAMPLING_X1,  // pressure
                    Adafruit_BME280::SAMPLING_X1,  // humidity
                    Adafruit_BME280::FILTER_OFF );
    bme.setTemperatureCompensation(-2); 
    rtc.begin();
    client.setInsecure(); // Usa a verificação de certificado SSL sem precisar armazená-lo
    Serial.println(rtc.now().timestamp());


    pinMode(CLK, INPUT);
    pinMode(DT, INPUT);
    pinMode(SW, INPUT);

    lastStateCLK = digitalRead(CLK);

    wifiConnected = tryWIFI();

    if (wifiConnected) {        
        ntpSrvIndex = tryNTPServer();
        lcd.setCursor(0, 0);
        lcd.printf("Wi-Fi: %s", WiFi.SSID().c_str());
        lcd.setCursor(0, 1);
        lcd.printf("IP: %s", WiFi.localIP().toString().c_str());
    } 
    else {
        ntpSrvIndex = -1;
        lcd.setCursor(0, 0);
        lcd.print("Sem Wi-Fi");
        lcd.setCursor(0, 1);
        lcd.print("Usando RTC");
    }

    // If connected to NTP server, display success and synchronize RTC
    if (ntpSrvIndex >= 0) {
        ntpConnected = true;
        lcd.setCursor(0, 2);
        lcd.print("Conectado ao NTP");
        lcd.setCursor(0, 3);
        lcd.print(ntpServers[ntpSrvIndex]);
        if (! rtc.isrunning()) {
            Serial.println("RTC is NOT running, let's set the time!");
            setRTC();
          } else {
            Serial.println("RTC is running, comparing to NTP time...");
            if (!isRTCSync()) {
                Serial.println("RTC time is different from NTP time, setting RTC...");
                setRTC();
            } else {
                Serial.println("RTC time is synchronized with NTP time.");
            }
          }
        delay(2000);
    } else {
        ntpConnected = false;
        lcd.setCursor(0, 2);
        lcd.print("Erro ao conectar NTP");
        delay(10000);
    }

    // Makes the Weather update in 10 seconds from startup
    current_dt = rtc.now().unixtime() - FETCH_INTERVAL - 10; 

    // Create custom LCD characters
    lcd.createChar(0, LT);
    lcd.createChar(1, UB);
    lcd.createChar(2, RT);
    lcd.createChar(3, LL);
    lcd.createChar(4, LB);
    lcd.createChar(5, LR);
    lcd.createChar(6, MB);
    lcd.createChar(7, block);

    lcd.clear();
    Serial.println("RTC Epoch: " + String(rtc.now().unixtime()));
    Serial.println("NTP Epoch: " + String(timeClient.getEpochTime()));

    // writeEEPROM(0, 0x41);
    byte data = readEEPROM(0);
    Serial.printf("EEPROM data: %c\n", data);




}

/*
 * printMainScreen() - Display the main screen of the clock
 */

void printMainScreen(int h, int m, int s, int day, int month, int year, int dayOfWeek,
                     float tmp, float hum, float pres, float alt) {

    // Print the big clock
    char separator = char(165);
    if (!ntpConnected) {
        separator = char(176);
    }
    if (s % 2 == 0) {
        separator = ' ';
    }    
    printDigits(h / 10, 0);
    printDigits(h % 10, 4);
    lcd.setCursor(7, 0);
    lcd.print(separator);
    lcd.setCursor(7, 1);
    lcd.print(separator);
    printDigits(m / 10, 8);
    printDigits(m % 10, 12);

    // Print the date
    lcd.setCursor(0, 2);
    lcd.printf("%s %02d/%02d/%04d", daysOfTheWeek[dayOfWeek], day, month, year);

    // Print weather data
    lcd.setCursor(16, 0);
    lcd.print("Temp");
    lcd.setCursor(16, 1);
    lcd.printf("%.1f", tmp);
    lcd.setCursor(0, 3);
    lcd.printf("U: %.1f%%  ", hum);
    lcd.setCursor(10, 3);
    lcd.printf("%.1fhPa  ", pres);
}

void printCurrentWeather() {
    lcd.setCursor(0, 0);
    if (rtc.now().second() / 4 % 2 == 0) {
        lcd.printf("%20s", current_weatherDescription);
    } else {
        lcd.printf("Medido em:  %s", DateTime(current_dt).timestamp(DateTime::TIMESTAMP_TIME).c_str());
    }    
    lcd.setCursor(0, 1);
    lcd.printf("Tem: %.1f", current_temp);
    lcd.setCursor(0, 2);
    lcd.printf("Sem: %.1f", current_feels_like);
    lcd.setCursor(11, 1);
    lcd.printf("Max: %.1f", current_temp_max);
    lcd.setCursor(11, 2);    
    lcd.printf("Min: %.1f", current_temp_min);
    lcd.setCursor(0, 3);
    lcd.printf("Umi: %d%%", current_humidity);
    lcd.setCursor(10, 3);
    lcd.printf("%dhPa", current_pressure);

}

void printNetworkStatus() {
    lcd.setCursor(0, 0);
    lcd.printf("Wi-Fi: %s", WiFi.SSID().c_str());
    lcd.setCursor(0, 1);
    lcd.printf("IP: %s", WiFi.localIP().toString().c_str());
    lcd.setCursor(0, 2);
    lcd.printf("NTP: %s", ntpConnected ? "Conectado" : "Desconectado");
    lcd.setCursor(0, 3);
    lcd.printf("RTC: %s", rtc.isrunning() ? "Ativo" : "Inativo");
}

void readRE() {
    int currentStateCLK = digitalRead(CLK);
    // Detects change in CLK state
    if (currentStateCLK != lastStateCLK) {
        if (digitalRead(DT) != currentStateCLK) {
            counter++; // Clockwise rotation
        } else {
            counter--; // Counter-clockwise rotation
        }
    }
    lastStateCLK = currentStateCLK;

    // Encoder button read
    bool currentButtonState = digitalRead(SW);
    if (currentButtonState == LOW && lastButtonState == HIGH) {
        btnPressed = true;
        btnToggle = !btnToggle;
    }
    lastButtonState = currentButtonState;
}



void loop() {
    
    // Rotary encoder read
    readRE();
    
    // Check if the counter is within the range
    // If not, reset to the maximum or minimum value
    if (counter < UIMin) {
        counter = UIMax; // Reset to maximum
    }
    else if (counter > UIMax) {
        counter = UIMin; // Reset to minimum
    }
    if (lastCounter != counter) {
        lastUIMillis = millis(); // Reset last User interaction time
    }

    // Main tasks every second or if user interacts with the rotary encoder
    if (millis() - lastMillis > 1000 || lastCounter != counter || btnPressed) {
        lastMillis = millis();
        lastCounter = counter;

        if (btnToggle) {
            lcd.noBacklight();
        }
        else {
            lcd.backlight();
        }

        // Synchronize time with NTP server every hour = 3600000 ms
        if (millis() - lastRTCsync > 90000) {
            lastRTCsync = millis();
            if (checkConnections()) {
                if (!isRTCSync()) {
                    Serial.println("RTC time is different from NTP time, setting RTC...");
                    setRTC();
                } else {
                    Serial.println("RTC time is synchronized with NTP time.");
                }
            }
        }

        // Update 
        readRTC();
        readBME();
        getWeather();

        if (millis() - lastUIMillis > 60000) {
            lastUIMillis = millis();
            counter = 0; // Reset counter to show main screen
        }
        if (lastUIScreen != counter) {
            lastUIScreen = counter;
            lcd.clear();
        }

        if (counter == 0) {
            //Serial.printf("Temp: %.1f C, Hum: %.1f%%, Pres: %.1fhPa, Alt: %.1fm NTP: %s\n", tmp, hum, pres, alt, ntpConnected ? "True" : "False");
            printMainScreen(hour, minute, second, day, month, year, dayOfWeek, tmp, hum, pres, alt);
        }
        else if (counter == 1) {
            printCurrentWeather();
        }
        else if (counter == -1) {
            printNetworkStatus();
        }
        
        btnPressed = false; // Reset button state
    }
}

