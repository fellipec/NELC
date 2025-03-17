# NELC (NTP ESP LCD Clock)

NELC é um projeto baseado no ESP8266 que exibe a data e hora obtidas de um servidor NTP em um display LCD 16x2, além de medir e exibir temperatura e umidade usando um sensor DHT22.

## 📌 Recursos
- Conexão Wi-Fi para sincronização com um servidor NTP.
- Exibição de data e hora ajustadas para o fuso horário UTC-3.
- Leitura e exibição da temperatura e umidade ambiente com um sensor DHT22.
- Atualização automática da exibição no LCD.

## 🛠️ Componentes Utilizados
- **ESP8266 (Wemos D1)**
- **1602 LCD Keypad Shield**
- **Sensor DHT22** (temperatura e umidade)
- **Jumpers** para conexão

## 🔧 Configuração e Uso

1. Clone este repositório:
   ```sh
   git clone https://github.com/fellipec/NELC.git
   cd NELC
   ```
2. Instale o [PlatformIO](https://platformio.org/) no VS Code.
3. Adicione as credenciais da sua rede Wi-Fi no arquivo `wifipasswd.h`:
   ```cpp
   #define WIFI_SSID "Seu_SSID"
   #define WIFI_PASSWORD "Sua_Senha"
   ```
4. Compile e faça o upload do código para o ESP8266.

## 📜 Licença
Este projeto é open-source e está sob a licença [GPL 3.0](LICENSE).

## 🤝 Contribuições
Sinta-se à vontade para abrir issues e pull requests para melhorias!
