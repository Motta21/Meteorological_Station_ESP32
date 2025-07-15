/*
This code was build to use in a Weather Station using ESP32.
Components: ESP32 DEV KIT, SD MODULE, RTC DS3231, BMP 280, HUT21D.
Build to use in a Programa Educação, on Brazil, this use is free!
Esp-32 has a RTC but are with a NTP (Netowork Time Protocol), and if you want to use in a enviroment
you should to use a RTC without network.

build by: Mateus Motta
*/
#include <SD.h>
#include <SPI.h>
#include <Wire.h>
#include <RTClib.h>
#include "Adafruit_HTU21DF.h"
#include <Adafruit_BMP280.h>
#include <math.h>
#include <esp_sleep.h>


// === DEFINE ===
#define SD_CS_PIN 5
#define ALTITUDE_ESTACAO 654.0     // Substitua pela altitude real da sua estação
#define uS_TO_S_FACTOR 1000000ULL  // Conversão de segundos para microssegundos

// === OBJETOS ===
TwoWire RTC_WIRE2 = TwoWire(1);  // Barramento I2C separado para o RTC
Adafruit_HTU21DF htu = Adafruit_HTU21DF();
Adafruit_BMP280 bmp;
RTC_DS3231 rtc;

// === VARIÁVEIS GLOBAIS ===
int tempoDeSleep = 470;  // Durma por 8min
float tempHTU[6], umiHTU[6], presBMP[6], presNivelMar[6];
float temperaturaMedia, temperaturaMin, temperaturaMax, temperaturaUltima;
float umidadeMedia, umidadeMin, umidadeMax, umidadeUltima;
float presMedia, presMin, presMax, presUltima;
float presNivelMarMedia, presNivelMarMin, presNivelMarMax, presNivelMarUltima;
int leituraIndex = 0;
bool jaMedido = false;
char datetime[30];

void setup() {
  Serial.begin(115200);
  delay(1000);
  Wire.begin(21, 22);       // I2C padrão (sensores)
  RTC_WIRE2.begin(26, 25);  // I2C secundário (RTC)

  if (!rtc.begin(&RTC_WIRE2)) {
    Serial.println("Falha no RTC!");
    logErro("Falha no RTC!");
  }

  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("Falha no SD!");
    logErro("Falha no SD!");
  }

  if (!htu.begin()) {
    Serial.println("Falha no sensor HTU!");
    logErro("Falha no sensor HTU!");
  }

  if (!bmp.begin(0x76)) {
    Serial.println("Falha no sensor BMP!");
    logErro("Falha no sensor BMP!");
  }

  Serial.println("Estação Iniciada");
}

void loop() {
  DateTime now = rtc.now();

  if (now.minute() % 10 == 0 && now.second() == 0 && !jaMedido) {
    jaMedido = true;
    getDateTime();

    float tHTU = htu.readTemperature();
    float hHTU = htu.readHumidity();
    float pBMP = bmp.readPressure() / 100.0;
    float pNivelMar = pBMP / pow(1.0 - (ALTITUDE_ESTACAO / 44330.0), 5.255);  // pressão ao nível do mar

    if (!isnan(tHTU) && !isnan(hHTU)) {
      tempHTU[leituraIndex] = tHTU;
      umiHTU[leituraIndex] = hHTU;
      temperaturaUltima = tHTU;
      umidadeUltima = hHTU;
    } else {
      Serial.println("Leitura HTU inválida");
      logErro("Leitura HTU inválida");
    }

    if (!isnan(pBMP)) {
      presBMP[leituraIndex] = pBMP;
      presUltima = pBMP;
      presNivelMar[leituraIndex] = pNivelMar;
      presNivelMarUltima = pNivelMar;
    } else {
      Serial.println("Leitura BMP inválida");
      logErro("Leitura BMP inválida");
    }

    Serial.println("Dados registrados");
    Serial.print(now.timestamp());
    Serial.printf(" Temp: %.1f °C | Umi: %.1f %% | Press: %.1f hPa | Press. Nível Mar: %.1f hPa\n",
                  tHTU, hHTU, pBMP, pNivelMar);
    salvaDados10Min(datetime, tHTU, hHTU, pBMP, pNivelMar);

    leituraIndex++;
    if (leituraIndex == 6) {
      calcularEstatisticas();
      salvarEstatisticas();
      leituraIndex = 0;
    }

    delay(10000);

    Serial.println("Modo Sleep 8 min");
    Serial.flush();
    esp_sleep_enable_timer_wakeup(tempoDeSleep * uS_TO_S_FACTOR);
    esp_deep_sleep_start();  // Após isso, tudo reinicia do setup()
  }

  if (now.minute() % 10 != 0) {
    jaMedido = false;
  }
}

// === OBTER DATA E HORA ===
void getDateTime() {
  DateTime now = rtc.now();
  snprintf(datetime, sizeof(datetime), "%02d/%02d/%04d %02d:%02d",
           now.day(), now.month(), now.year(), now.hour(), now.minute());
}

// === SALVAR A CADA 10 MINUTOS ===
void salvaDados10Min(const char* dataHora, float temp, float umi, float pressao, float pressaoNivelMar) {
  File file = SD.open("/dados_10min.csv", FILE_APPEND);
  if (file) {
    if (file.size() == 0) {
      file.println("DataHora,Temperatura,Umidade,PressaoLocal,PressaoNivelMar");
    }
    file.printf("%s,%.1f,%.1f,%.1f,%.1f\n", dataHora, temp, umi, pressao, pressaoNivelMar);
    file.close();
    Serial.println("Dados de 10min salvos.");
  } else {
    Serial.println("Falha ao salvar dados de 10min");
    logErro("Falha ao salvar dados de 10min");
  }
}

// === SALVAR ESTATÍSTICAS A CADA HORA ===
void salvarEstatisticas() {
  File file = SD.open("/estatisticas_hora.csv", FILE_APPEND);
  if (file) {
    if (!SD.exists("estatisticas_hora.csv")) {
      file.println("DataHora,TempMedia,TempMin,TempMax,TempUltima,UmidMedia,UmidMin,UmidMax,UmidUltima,PresMedia,PresMin,PresMax,PresUltima,PresNivelMarMedia,PresNivelMarMin,PresNivelMarMax,PresNivelMarUltima");
    }

    file.printf("%s,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f\n",
                datetime,
                temperaturaMedia, temperaturaMin, temperaturaMax, temperaturaUltima,
                umidadeMedia, umidadeMin, umidadeMax, umidadeUltima,
                presMedia, presMin, presMax, presUltima,
                presNivelMarMedia, presNivelMarMin, presNivelMarMax, presNivelMarUltima);
    file.close();
    Serial.println("Estatísticas salvas.");
  } else {
    logErro("Falha ao salvar estatísticas");
    Serial.println("Falha ao salvar estatísticas");
  }
}

// === REGISTRAR ERROS ===
void logErro(const char* msg) {
  getDateTime();
  File file = SD.open("/erros.csv", FILE_APPEND);
  if (file) {
    file.printf("%s;%s\n", datetime, msg);
    file.close();
    Serial.println("Erro registrado");
  }
}

// === CÁLCULO DE ESTATÍSTICAS ===
void calcularEstatisticas() {
  temperaturaMin = temperaturaMax = tempHTU[0];
  umidadeMin = umidadeMax = umiHTU[0];
  presMin = presMax = presBMP[0];
  presNivelMarMin = presNivelMarMax = presNivelMar[0];

  float somaTemp = 0, somaUmi = 0, somaPres = 0, somaPresNivelMar = 0;

  for (int i = 0; i < 6; i++) {
    float t = tempHTU[i];
    float u = umiHTU[i];
    float p = presBMP[i];
    float pnm = presNivelMar[i];

    somaTemp += t;
    somaUmi += u;
    somaPres += p;
    somaPresNivelMar += pnm;

    if (t > temperaturaMax) temperaturaMax = t;
    if (t < temperaturaMin) temperaturaMin = t;

    if (u > umidadeMax) umidadeMax = u;
    if (u < umidadeMin) umidadeMin = u;

    if (p > presMax) presMax = p;
    if (p < presMin) presMin = p;

    if (pnm > presNivelMarMax) presNivelMarMax = pnm;
    if (pnm < presNivelMarMin) presNivelMarMin = pnm;
  }

  temperaturaMedia = somaTemp / 6.0;
  umidadeMedia = somaUmi / 6.0;
  presMedia = somaPres / 6.0;
  presNivelMarMedia = somaPresNivelMar / 6.0;
}