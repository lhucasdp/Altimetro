#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SFE_BMP180.h>

SFE_BMP180 pressure;
File mySensorData;

double baseline;
unsigned long lastWrite = 0;
unsigned long lastFlush = 0;
const int chipSelect = 10;

// --- parâmetros do filtro ---
const int windowSize = 5;    // número de amostras na média móvel
double altitudeBuffer[windowSize];
int bufferIndex = 0;
bool bufferFilled = false;

// -------------------------------------------------------------
void setup() {
  Serial.begin(9600);
  Wire.begin();

  Serial.println(F("Inicializando BMP180..."));
  if (!pressure.begin()) {
    Serial.println(F("Falha ao inicializar BMP180 (desconectado?)"));
    while (1);
  }

  Serial.println(F("Calculando baseline..."));
  baseline = getBaseline();
  Serial.print(F("Baseline (pressão de referência): "));
  Serial.println(baseline);

  Serial.println(F("Inicializando SD..."));
  if (!SD.begin(chipSelect)) {
    Serial.println(F("Falha ao inicializar SD!"));
    while (1);
  }

  // Cria novo arquivo incremental (data_001.txt, data_002.txt, ...)
  int fileIndex = 1;
  char filename[15];
  do {
    sprintf(filename, "data_%03d.txt", fileIndex++);
  } while (SD.exists(filename));

  mySensorData = SD.open(filename, FILE_WRITE);
  if (!mySensorData) {
    Serial.println(F("Erro ao abrir arquivo!"));
    while (1);
  }

  mySensorData.println(F("================="));
  mySensorData.println(F("tempo_ms\taltitude_m"));
  mySensorData.flush();
  Serial.print(F("Gravando em: "));
  Serial.println(filename);
  Serial.println(F("Pronto para registrar."));
}

// -------------------------------------------------------------
void loop() {
  unsigned long currentMillis = millis();

  // Grava a cada 100 ms
  if (currentMillis - lastWrite >= 100) {
    lastWrite = currentMillis;

    double P = getPressure();
    if (P < 0) return; // erro no sensor, ignora leitura

    double altitudeRaw = pressure.altitude(P, baseline);
    double altitudeSmooth = filtroLeve(altitudeRaw);

    Serial.print(currentMillis); Serial.print("\t");
    Serial.println(altitudeSmooth);

    if (mySensorData) {
      mySensorData.print(currentMillis);
      mySensorData.print("\t");
      mySensorData.println(altitudeSmooth);
    }
  }

  // Faz flush a cada 5 segundos
  if (currentMillis - lastFlush >= 5000) {
    lastFlush = currentMillis;
    if (mySensorData) {
      mySensorData.flush();
    }
  }
}

// -------------------------------------------------------------
double getPressure() {
  char status;
  double T, P;

  status = pressure.startTemperature();
  if (status == 0) return -1;
  delay(status);

  status = pressure.getTemperature(T);
  if (status == 0) return -1;

  status = pressure.startPressure(3);
  if (status == 0) return -1;
  delay(status);

  status = pressure.getPressure(P, T);
  if (status == 0) return -1;

  return P;
}

// -------------------------------------------------------------
double getBaseline() {
  double soma = 0;
  for (int i = 0; i < 10; i++) {
    soma += getPressure();
    delay(50);
  }
  return soma / 10;
}

// -------------------------------------------------------------
// Filtro leve: média móvel simples sobre as últimas N leituras
double filtroLeve(double novaLeitura) {
  altitudeBuffer[bufferIndex] = novaLeitura;
  bufferIndex = (bufferIndex + 1) % windowSize;

  if (bufferIndex == 0) bufferFilled = true;

  double soma = 0;
  int count = bufferFilled ? windowSize : bufferIndex;
  for (int i = 0; i < count; i++) {
    soma += altitudeBuffer[i];
  }

  return soma / count;
}
