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
#include <PubSubClient.h>
#include <wifipasswd.h>  // Wi-Fi credentials
#include <apikeys.h>     // OpenWeatherMap API key
#include <mqttconfig.h>  // MQTT configuration

// Uncomment to enable the debug serial print
// #define SERIALPRINT

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
const int alt = 935; // Altitude in meters
#define MAX_REQUEST_SIZE 512
#define MAX_RESPONSE_SIZE 4096
#define FETCH_INTERVAL 900 // Fetch weather data every 15 minutes
char weatherJson[MAX_RESPONSE_SIZE];

// Network Configuration
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntpServers[ntpSrvIndex], utcOffsetInSeconds, 60000);
WiFiClientSecure client;
PubSubClient mqtt(client);

// Dias da semana
const char* daysOfTheWeek[7] = {"Domingo", "Segunda", "Terca", "Quarta", "Quinta", "Sexta", "Sabado"};

/* 
 * GLOBAL VARIABLES 
 */


// General variables
int day, month, year, hour, minute, second, dayOfWeek;
unsigned long  lastRTCsync = 0;
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
int UIMax = 9; 
int UIMin = -1; 


// Weather variables
float tmp, hum, pres, calc_alt, qnh;
float lastTemp = -1000, lastHum = -1000;
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
#define FORECAST_HOURS 8
long forecast_dt = 0;
struct Forecast {
  long dt;
  float temp;
  float feels_like;
  float temp_min;
  float temp_max;
  int pressure;
  int humidity;
  float pop;
  float rain_3h;
  char description[32];
};

Forecast forecast[FORECAST_HOURS];




bool tryWIFI() {
    bool connOK = false;
    const char* gizmo[] = {"|", ">", "=", "<"}; //Wi-Fi loading animation

    WiFi.mode(WIFI_STA);
    WiFi.disconnect(); // Limpa conexões anteriores
    delay(100);
    #ifdef SERIALPRINT
    Serial.println("Escaneando redes...");
    #endif
    int n = WiFi.scanNetworks();
    if (n == 0) {
      #ifdef SERIALPRINT
      Serial.println("Nenhuma rede encontrada.");
      #endif
      return connOK;
    }

    for (int i = 0; i < numRedes; i++) {
        #ifdef SERIALPRINT
        Serial.print("Tentando conectar em ");
        Serial.print(ssids[i]);
        #endif
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Conectando em:");
        lcd.setCursor(0, 1);
        lcd.print("               ");
        lcd.setCursor(0, 1);
        lcd.print(ssids[i]);
        bool found = false;
        for (int j = 0; j < n; j++) {
            if (WiFi.SSID(j) == ssids[i]) {
                found = true;
                break;
            }
        }
        if (!found) {
            #ifdef SERIALPRINT
            Serial.println(" - Rede não encontrada.");
            #endif
            continue;  // Skip to the next SSID if not found
        }
        WiFi.begin(ssids[i], passwords[i]);

        int tentativa = 0;
        int j = 0;  // Index for gizmo array
        // Retry connection up to 10 seconds (10 attempts)
        while (WiFi.status() != WL_CONNECTED && tentativa < 20) {
            delay(250);
            #ifdef SERIALPRINT            
            Serial.print(".");
            #endif
            lcd.setCursor(19, 1);
            lcd.print(gizmo[j]);  // Display some progress information
            j = (j + 1) % 4;  // Cycle through the gizmo array
            tentativa++;
        }
        
        // If connected successfully to Wi-Fi
        if (WiFi.status() == WL_CONNECTED) {
            #ifdef SERIALPRINT
            Serial.printf("\nConectado em %s\n", ssids[i]);
            #endif
            lcd.clear();
            lcd.print("Conectado ao ");
            lcd.setCursor(0, 1);
            lcd.print("Wi-Fi: ");
            lcd.print(ssids[i]);
            connOK = true;
            break;  // Exit loop if connection is successful
        } else {
            #ifdef SERIALPRINT
            Serial.printf("\nFalha ao conectar no Wi-Fi: %s\n", ssids[i]);
            #endif
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
            #ifdef SERIALPRINT
            Serial.printf("Conexão com NTP bem-sucedida: %s\n ", ntpServers[i]);
            #endif
            return i;
        } else {
            #ifdef SERIALPRINT
            Serial.printf("Erro ao conectar no NTP: %s\n", ntpServers[i]);
            #endif
        }
    }
    return -1;
}

/*
*  removeAccents() - Removes accents from a string
*
*  This function takes an UTF-8 string as input and removes any accents from the characters.
*  It is necessary for the correct display of characters on the LCD.
*/
void removeAccents(char* str) {
    char* src = str;
    char* dst = str;
  
    while (*src) {
      // Se encontrar caractere UTF-8 multibyte (início com 0xC3)
      if ((uint8_t)*src == 0xC3) {
        src++;  // Avança para o próximo byte
        switch ((uint8_t)*src) {
          case 0xA0: case 0xA1: case 0xA2: case 0xA3: case 0xA4:  *dst = 'a'; break; // àáâãä
          case 0x80: case 0x81: case 0x82: case 0x83: case 0x84:  *dst = 'A'; break; // ÀÁÂÃÄ
          case 0xA7: *dst = 'c'; break; // ç
          case 0x87: *dst = 'C'; break; // Ç
          case 0xA8: case 0xA9: case 0xAA: case 0xAB: *dst = 'e'; break; // èéêë
          case 0x88: case 0x89: case 0x8A: case 0x8B: *dst = 'E'; break; // ÈÉÊË
          case 0xAC: case 0xAD: case 0xAE: case 0xAF: *dst = 'i'; break; // ìíîï
          case 0x8C: case 0x8D: case 0x8E: case 0x8F: *dst = 'I'; break; // ÌÍÎÏ
          case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6: *dst = 'o'; break; // òóôõö
          case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: *dst = 'O'; break; // ÒÓÔÕÖ
          case 0xB9: case 0xBA: case 0xBB: case 0xBC: *dst = 'u'; break; // ùúûü
          case 0x99: case 0x9A: case 0x9B: case 0x9C: *dst = 'U'; break; // ÙÚÛÜ
          case 0xB1: *dst = 'n'; break; // ñ
          case 0x91: *dst = 'N'; break; // Ñ
          default: *dst = '?'; break; // desconhecido
        }
        dst++;
        src++; // Pula o segundo byte do caractere especial
      } else {
        *dst++ = *src++; // Copia byte normal
      }
    }
    *dst = '\0'; // Termina a nova string
  }

  void upperFirstLetter(char* str) {
    if (str && str[0] != '\0') {  // Checks for empty string        
      str[0] = toupper(str[0]);  // Convert the first character to uppercase
    }
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


float calculateQNH(float pressure, float temperature, float altitude) {
    const float L = 0.0065;        // Gradiente térmico padrão (°C/m)
    const float T0 = temperature + 273.15;  // Temperatura absoluta (K)
    const float g = 9.80665;
    const float M = 0.0289644;
    const float R = 8.3144598;

    float exponent = (g * M) / (R * L);
    float seaLevelPressure = pressure * pow(1 - (L * altitude) / T0, -exponent);
    return seaLevelPressure;
}

void readBME() {
    if (millis() - lastBMERead > BME_READ_INTERVAL || lastBMERead == 0) {
        lastBMERead = millis();
        bme.takeForcedMeasurement();
        tmp = bme.readTemperature();
        hum = bme.readHumidity();
        pres = bme.readPressure() / 100.0F;        
        qnh = calculateQNH(pres, tmp, alt);
        calc_alt = bme.readAltitude(current_pressure);    
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
    #ifdef SERIALPRINT
    Serial.printf("RTC set to: %02d:%02d:%02d %02d/%02d/%04d\n", hour, minute, second, day, month, year);
    #endif
}

bool isRTCSync() {
    readNTP();
    DateTime ntpnow = DateTime(year, month, day, hour, minute, second);
    DateTime rtcnow = rtc.now();
    #ifdef SERIALPRINT
    Serial.printf("RTC: %02d:%02d:%02d %02d/%02d/%04d\n", rtcnow.hour(), rtcnow.minute(), rtcnow.second(), rtcnow.day(), rtcnow.month(), rtcnow.year());
    Serial.printf("NTP: %02d:%02d:%02d %02d/%02d/%04d\n", ntpnow.hour(), ntpnow.minute(), ntpnow.second(), ntpnow.day(), ntpnow.month(), ntpnow.year());
    #endif
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

void buildWeatherRequest(char* request, const char* lat, const char* lon, const char* apiKey) {
    snprintf(request, MAX_REQUEST_SIZE, 
             "GET /data/2.5/weather?lat=%s&lon=%s&appid=%s&units=metric&lang=pt_br HTTP/1.1\r\n"
             "Host: api.openweathermap.org\r\n"
             "Connection: close\r\n\r\n", 
             lat, lon, apiKey);
}

void buildForecastRequest(char* request, const char* lat, const char* lon, const char* apiKey) {
    snprintf(request, MAX_REQUEST_SIZE, 
             "GET /data/2.5/forecast?lat=%s&lon=%s&cnt=8&appid=%s&units=metric&lang=pt_br HTTP/1.1\r\n"
             "Host: api.openweathermap.org\r\n"
             "Connection: close\r\n\r\n", 
             lat, lon, apiKey);
}

// char weatherJson[MAX_RESPONSE_SIZE];
void getWeatherJSON(bool forecast = false) {
    if (!client.connect("api.openweathermap.org", 443)) { 
        #ifdef SERIALPRINT
        Serial.println("Falha ao conectar ao servidor.");
        #endif
        return;
    }
    char req[MAX_REQUEST_SIZE];
    if (forecast) {
        buildForecastRequest(req, lat, lon, apiKey);
    } else {
        buildWeatherRequest(req, lat, lon, apiKey);
    }    
    
    #ifdef SERIALPRINT
    Serial.println("Requisição:");
    Serial.println(req);
    #endif
    client.print(req); 

    unsigned long timeout = millis();
    while (client.available() == 0) { 
        if (millis() - timeout > 5000) { // 5 seconds timeout
            #ifdef SERIALPRINT
            Serial.println("Erro: Timeout.");
            #endif
            client.stop();
            return;
        }
    }
    // Payload buffer
    // User this while loop to avoid the Strings object
    // to avoid memory fragmentation
    unsigned int index = 0;
    unsigned long lastRead = millis();
    while (millis() - lastRead < 2000) { // 2 seconds timeout for reading network
        while (client.available()) {            
            if (index < MAX_RESPONSE_SIZE - 1) {  // Buffer limit check
                weatherJson[index++] = (char)client.read();  // Add the next character to the buffer
                lastRead = millis();  // Update last read time
            } else {
                break;  // Buffer is full, stop reading
            }
        }
        yield(); // try to play nice with the esp8266
    }
    weatherJson[index] = '\0';  // Add null terminator to the string
    #ifdef SERIALPRINT
    Serial.println("Resposta do servidor:");
    Serial.print(weatherJson);
    Serial.print("\n\n");
    #endif

    // Find the JSON start position
    char* jsonStart = strchr(weatherJson, '{');  // First { character
    if (jsonStart) {
        // Copy the JSON part to the payload
        strcpy(weatherJson, jsonStart);
    } else {
        #ifdef SERIALPRINT
        Serial.println("Erro: JSON não encontrado na resposta.");
        #endif
        return;
    }

}


void getForecast() {
    if ((rtc.now().unixtime() - forecast_dt > FETCH_INTERVAL*4)) {
        forecast_dt = rtc.now().unixtime();
        getWeatherJSON(true);
        
        JsonDocument doc;

        DeserializationError error = deserializeJson(doc, weatherJson, MAX_RESPONSE_SIZE);
        
        if (error) {
            #ifdef SERIALPRINT
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.f_str());
            #endif
            return;
        }
        
        JsonArray list = doc["list"];
        
        for (int i = 0; i < FORECAST_HOURS; i++) {
            JsonObject entry = list[i];
            JsonObject main = entry["main"];
            JsonObject weather0 = entry["weather"][0];
            JsonObject rain = entry["rain"];
            
            forecast[i].dt = entry["dt"];
            forecast[i].dt += utcOffsetInSeconds;
            forecast[i].temp = main["temp"];
            forecast[i].feels_like = main["feels_like"];
            forecast[i].temp_min = main["temp_min"];
            forecast[i].temp_max = main["temp_max"];
            forecast[i].pressure = main["pressure"];
            forecast[i].humidity = main["humidity"];
            forecast[i].pop = entry["pop"];
            forecast[i].rain_3h = rain["3h"] | 0.0;
        
            const char* desc = weather0["description"] | "";
            strncpy(forecast[i].description, desc, sizeof(forecast[i].description));
            forecast[i].description[sizeof(forecast[i].description) - 1] = '\0';
            upperFirstLetter(forecast[i].description);
            removeAccents(forecast[i].description);
        }
        

    }

}


void getWeather() {
    if (rtc.now().unixtime() - current_dt > FETCH_INTERVAL) {


        getWeatherJSON(false);
    
        // JSON parsing   
        JsonDocument doc;

        DeserializationError error = deserializeJson(doc, weatherJson, MAX_RESPONSE_SIZE);

        if (error) {
            #ifdef SERIALPRINT
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.f_str());
            #endif
            return;
        }
        
        #ifdef SERIALPRINT
        Serial.println("JSON parsed");
        #endif
        JsonObject weather_0 = doc["weather"][0];
        const char* desc = weather_0["description"] | ""; 
        strncpy(current_weatherDescription, desc, sizeof(current_weatherDescription)); // Copy string to avoid null pointer
        current_weatherDescription[sizeof(current_weatherDescription) - 1] = '\0'; // add null terminator
        upperFirstLetter(current_weatherDescription); // Capitalize first letter
        removeAccents(current_weatherDescription); // Remove accents
        const char* name = doc["name"] | "";
        strncpy(location_name, name, sizeof(location_name)); // Copy string to avoid null pointer
        location_name[sizeof(location_name) - 1] = '\0'; // add null terminator
        upperFirstLetter(location_name); // Capitalize first letter
        removeAccents(location_name); // Remove accents

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

        
        #ifdef SERIALPRINT
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
        #endif
        

    }
 
  }

long lastMQTTMillis = 0;
void sendSensorData(float tmp, float hum, float pres) {
    if (millis() - lastMQTTMillis > 30000) {
        lastMQTTMillis = millis();
        lcd.setCursor(19,3);
        lcd.write(255); // MQTT Activity icon

        // Check if connected to MQTT
        if (!mqtt.connected()) {
            #ifdef SERIALPRINT
            Serial.println("Conectando ao MQTT...");
            #endif
            if (mqtt.connect("esp1", mqtt_user, mqtt_pass)) {
                #ifdef SERIALPRINT
                Serial.println("Conectado ao MQTT");
                #endif
            } else {
                #ifdef SERIALPRINT
                Serial.print("Falha ao conectar ao MQTT, rc=");
                Serial.print(mqtt.state());
                #endif
                return;
            }
        }

        // Messages variables
        char tmp_str[8];
        char hum_str[8];
        char pres_str[8];


        // Convert to string
        dtostrf(tmp, 6, 2, tmp_str);
        dtostrf(hum, 6, 2, hum_str);
        dtostrf(pres, 6, 2, pres_str);

        // Publish on MQTT
        mqtt.publish("esp1/sensor/temperature", tmp_str);
        mqtt.publish("esp1/sensor/humidity", hum_str);
        mqtt.publish("esp1/sensor/pressure", pres_str);

        #ifdef SERIALPRINT
        Serial.println("Dados enviados ao broker:");
        Serial.print("Temperatura: "); Serial.println(tmp_str);
        Serial.print("Umidade: "); Serial.println(hum_str);
        Serial.print("Pressão: "); Serial.println(pres_str);
        #endif
        lcd.setCursor(19,3);
        lcd.write(32); // Clear MQTT Activity icon
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
    Serial.println("");
    Serial.println("Iniciando...");
    lcd.begin(20, 4);
    lcd.backlight();
    lcd.setCursor(0, 0);lcd.print("+------------------+");
    lcd.setCursor(0, 1);lcd.print("|    Iniciando     |");
    lcd.setCursor(0, 2);lcd.print("|    Aguarde...    |");
    lcd.setCursor(0, 3);lcd.print("+------------------+");
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
            #ifdef SERIALPRINT
            Serial.println("RTC is NOT running, let's set the time!");
            #endif
            setRTC();
          } else {
            #ifdef SERIALPRINT
            Serial.println("RTC is running, comparing to NTP time...");
            #endif
            if (!isRTCSync()) {
                #ifdef SERIALPRINT
                Serial.println("RTC time is different from NTP time, setting RTC...");
                #endif
                setRTC();
            } else {
                #ifdef SERIALPRINT
                Serial.println("RTC time is synchronized with NTP time.");
                #endif
            }
          }
        delay(2000);
    } else {
        ntpConnected = false;
        lcd.setCursor(0, 2);
        lcd.print("Erro ao conectar NTP");
        delay(10000);
    }

    // Configure the MQTT server
    mqtt.setServer(mqtt_server, mqtt_port);
    mqtt.setKeepAlive(60);


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

    // Update 
    readRTC();
    readBME();
    getWeather();
    getForecast();

    lcd.clear();
    #ifdef SERIALPRINT
    Serial.println("RTC Epoch: " + String(rtc.now().unixtime()));
    Serial.println("NTP Epoch: " + String(timeClient.getEpochTime()));
    #endif

    // writeEEPROM(0, 0x41);
    byte data = readEEPROM(0);
    Serial.printf("EEPROM data: %c\n", data);

    #ifdef SERIALPRINT
    Serial.println("Inicialização completa - Debug Serial ligado");
    #else
    Serial.println("Inicialização completa - Debug Serial desligado");
    #endif

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
    lcd.printf("%.1fhPa ", qnh);
}

void printCurrentWeather() {
    lcd.setCursor(0, 0);
    if (rtc.now().second() / 4 % 2 == 0) {
        lcd.printf("%-20s", current_weatherDescription);
    } else {
        lcd.printf("Tempo as:  %s", DateTime(current_dt).timestamp(DateTime::TIMESTAMP_TIME).c_str());
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

void printForecast(unsigned int index) {
    if (index >= (sizeof(forecast)/sizeof(forecast[0])) ) {
        #ifdef SERIALPRINT
        Serial.printf("Index out of range %d\n", index);
        #endif
        return;
    }
  
    Forecast fc = forecast[index];
    
    // Converter timestamp para hora
    time_t t = fc.dt;
    struct tm* timeinfo = localtime(&t); // precisa de RTC ou NTP funcionando
    char timeStr[12]; // dd/mm hh:mm
    strftime(timeStr, sizeof(timeStr), "%d/%m %H:%M", timeinfo);
  
    // Linha 0: hora da previsão e início da descrição
    lcd.setCursor(0, 0);
    lcd.print("Prev: ");
    lcd.print(timeStr);
  
    // Linha 1: descrição do clima (até 20 chars)
    lcd.setCursor(0, 1);
    if (strlen(fc.description) > 20) {
      for (int i = 0; i < 20; i++) lcd.print(fc.description[i]);
    } else {
      lcd.print(fc.description);
    }
  
    // Linha 2: temperatura máxima e mínima
    lcd.setCursor(0, 2);
    lcd.print("Max:");
    lcd.print(fc.temp_max, 1);
    lcd.print((char)223); // símbolo do grau
    lcd.print(" Min:");
    lcd.print(fc.temp_min, 1);
    lcd.print((char)223);
  
    // Linha 3: chuva e probabilidade de chuva
    lcd.setCursor(0, 3);
    lcd.print((int)(fc.pop * 100));
    lcd.print("% Chuva: ");
    lcd.print(fc.rain_3h, 1);
    lcd.print("mm");
  }
  

void printNetworkStatus() {
    lcd.setCursor(0, 0);
    lcd.printf("Wi-Fi: %s", WiFi.SSID().c_str());
    lcd.setCursor(0, 1);
    lcd.printf("IP: %s", WiFi.localIP().toString().c_str());
    lcd.setCursor(0, 2);
    lcd.printf("NTP: %s", ntpConnected ? "OK   " : "Nao  ");
    lcd.print(timeClient.getFormattedTime());
    lcd.setCursor(0, 3);
    lcd.printf("RTC: %s", rtc.isrunning() ? "OK   " : "Nao  ");
    lcd.printf("%02d:%02d:%02d", rtc.now().hour(), rtc.now().minute(), rtc.now().second());
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
                    #ifdef SERIALPRINT
                    Serial.println("RTC time is different from NTP time, setting RTC...");
                    #endif
                    setRTC();
                } else {
                    #ifdef SERIALPRINT
                    Serial.println("RTC time is synchronized with NTP time.");
                    #endif
                }
            }
        }



        if (millis() - lastUIMillis > 60000) {
            lastUIMillis = millis();
            counter = 0; // Reset counter to show main screen
        }
        if (lastUIScreen != counter) {
            lastUIScreen = counter;
            lcd.clear();
        }

        switch (counter) {
            case 0:
                #ifdef SERIALPRINT
                Serial.printf("Temp: %.1f C, Hum: %.1f%%, Pres: %.1fhPa, Alt: %dm NTP: %s\n", tmp, hum, pres, alt, ntpConnected ? "True" : "False");
                #endif
                printMainScreen(hour, minute, second, day, month, year, dayOfWeek, tmp, hum, pres, alt);
                break;
            case 1:
                printCurrentWeather();
                break;
            case -1:
                printNetworkStatus();
                break;
            default:
                // Forecast ranges from 2 to 9
                // Needs to subtract 2 to translate to index 0 through 7
                if (counter >= 2 && counter <= 9) {
                    printForecast(counter - 2);
                }
                break;
        }
        
        // Update 
        readRTC();
        readBME();
        getWeather();
        getForecast();
        sendSensorData(tmp, hum, qnh);

        btnPressed = false; // Reset button state
    }
    
}

