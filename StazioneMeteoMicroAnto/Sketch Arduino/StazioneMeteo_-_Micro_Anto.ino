#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_SHT31.h>
#include <Adafruit_BMP280.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

struct SensorData {
  float temperature;
  float humidity;
  float pressure;
  float seaLevelPressure;
  float dewpoint;
  float wetbulb;
  float heatIndex;
};

struct WeatherData {
  float temperatureMax;
  float temperatureMin;
  float humidityMax;
  float humidityMin;
};

struct DateTime {
  byte hour;
  byte minute;
  byte second;
};

const char* ssid = "SSID";
const char* password = "PASSWORD";

const char* ntpServer = "time.google.com";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;

uint32_t tempo;
DateTime currentTime;
String timestamp;

Adafruit_SHT31 sht31;
Adafruit_BMP280 bmp;  // BMP280 setup
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntpServer, gmtOffset_sec, daylightOffset_sec);

SensorData sensorData;
WeatherData weatherData;

// Standard sea level pressure
const float seaLevelPressure = 1013.25; // hPa

// Altitude
const float altitude = 50; // meters

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connessione alla WiFi...");
  }

  Serial.println("CONNESSO!");
  tempo = millis();

  // Initialize SHT31 sensor
  if (!sht31.begin(0x44)) {
    Serial.println("Impossibile trovare il sensore SHT31. Verifica il collegamento!");
    while (1);
  }

  sensorData.temperature = sht31.readTemperature();       // Read temperature from SHT31 sensor
  sensorData.humidity = sht31.readHumidity();              // Read humidity from SHT31 sensor
  sensorData.seaLevelPressure = sensorData.pressure * exp((altitude * 9.80665) / (287.0531 * (sensorData.temperature + 273.15))); // Calculate sea level pressure using the barometric formula
  sensorData.pressure = bmp.readPressure() / 100.0;      // Read pressure from BMP280 sensor and convert to hPa
  sensorData.heatIndex = computeHeatIndex(sensorData.temperature, sensorData.humidity); // Read Heat Index
  weatherData.temperatureMax = sht31.readTemperature();
  weatherData.temperatureMin = sht31.readTemperature();
  weatherData.humidityMax = sht31.readHumidity();
  weatherData.humidityMin = sht31.readHumidity();

  // Initialize BMP280 sensor
  if (!bmp.begin(0x76)) {
    Serial.println("Impossibile trovare il sensore BMP280. Verifica il collegamento!");
    while (1);
  }

  // Initialize NTP
  timeClient.begin();
  timeClient.update(); // Get the current time
}

void loop() {
  sensorData.temperature = sht31.readTemperature();       // Read temperature from SHT31 sensor
  sensorData.humidity = sht31.readHumidity();              // Read humidity from SHT31 sensor
  sensorData.seaLevelPressure = sensorData.pressure * exp((altitude * 9.80665) / (287.0531 * (sensorData.temperature + 273.15))); // Calculate sea level pressure using the barometric formula
  sensorData.pressure = bmp.readPressure() / 100.0;      // Read pressure from BMP280 sensor and convert to hPa
  sensorData.heatIndex = computeHeatIndex(sensorData.temperature, sensorData.humidity); // Read Heat Index

  // Calculate dew point
  float dewpointcalcolo = (log(sensorData.humidity / 100.0) + ((17.27 * sensorData.temperature) / (237.3 + sensorData.temperature))) / 17.27;
  sensorData.dewpoint = (237.3 * dewpointcalcolo) / (1 - dewpointcalcolo);

  // Calculate wet bulb temperature
  sensorData.wetbulb = (sensorData.temperature * (0.45 + 0.006 * sensorData.humidity * (sensorData.pressure ) / 1060));


  if (isnan(sensorData.temperature) || isnan(sensorData.humidity)) {
    Serial.println("Impossibile leggere i dati dal sensore SHT31!");
    // Aggiungi qui altre azioni in caso di errore nella lettura del sensore
    return;
  }
  if (isnan(sensorData.temperature) || isnan(sensorData.humidity)) {
    Serial.println("Impossibile leggere i dati dal sensore SHT31!");
  }

  if(!timeClient.update()){
     timeClient.forceUpdate(); // Update the current time
  }

  // Get the current date and time
  currentTime.hour = (timeClient.getHours() + daylightOffset_sec / 3600) % 24;
  currentTime.minute = timeClient.getMinutes();
  currentTime.second = timeClient.getSeconds();

  String hourString = (currentTime.hour < 10) ? "0" + String(currentTime.hour) : String(currentTime.hour);
  String minuteString = (currentTime.minute < 10) ? "0" + String(currentTime.minute) : String(currentTime.minute);
  String secondString = (currentTime.second < 10) ? "0" + String(currentTime.second) : String(currentTime.second);

  timestamp = hourString + "." + minuteString + "." + secondString;

  if (timestamp == "00.00.00") {
    weatherData.temperatureMax = sensorData.temperature;
    weatherData.temperatureMin = sensorData.temperature;
    weatherData.humidityMax = sensorData.humidity;
    weatherData.humidityMin = sensorData.humidity;
  }
  if (!isnan(sensorData.temperature)) {
    if (sensorData.temperature > weatherData.temperatureMax) {
      weatherData.temperatureMax = sensorData.temperature;
    }
    if (sensorData.temperature < weatherData.temperatureMin) {
      weatherData.temperatureMin = sensorData.temperature;
    }
  }
  if (!isnan(sensorData.humidity)) {
    if (sensorData.humidity > weatherData.humidityMax) {
      weatherData.humidityMax = sensorData.humidity;
    }
    if (sensorData.humidity < weatherData.humidityMin) {
      weatherData.humidityMin = sensorData.humidity;
    }
  }

  if (millis() - tempo >= 5000) {
    sendSensorData();
    tempo = millis();
  }
}

void sendSensorData() {
  HTTPClient http;
  Serial.println(timestamp);

  // Build the complete URL with data   1
  String url = "https://MIOSITOWEBSTAZIONEMETEO.altervista.org/stazionemeteo.php?data=";
  url += String(sensorData.temperature) + "-" + String(sensorData.humidity) + "-" + String(sensorData.pressure)
         + "-" + String(sensorData.dewpoint) + "-" + String(sensorData.wetbulb) + "-" +
         String(weatherData.temperatureMax) + "-" + String(weatherData.temperatureMin) + "-" + String(weatherData.humidityMax) + "-" + String(weatherData.humidityMin) + "-" +
         timestamp + "-" + String(sensorData.heatIndex) + "-" +
         String(sensorData.seaLevelPressure);

  http.begin(url);

  int httpResponseCode = http.GET();
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println(httpResponseCode);
    Serial.println(response);
  } else {
    Serial.print("Errore della HTTP request: ");
    Serial.println(httpResponseCode);
  }

  http.end();
}

float computeHeatIndex(float temperature, float humidity) {
  // Compute heat index only if temperature is between 26 and 35 degrees Celsius
  if (temperature >= 26 && temperature <= 35) {
    // Constants for the Steadman's heat index formula
    const float c1 = -8.78469475556;
    const float c2 = 1.61139411;
    const float c3 = 2.33854883889;
    const float c4 = -0.14611605;
    const float c5 = -0.012308094;
    const float c6 = -0.0164248277778;
    const float c7 = 0.002211732;
    const float c8 = 0.00072546;
    const float c9 = -0.000003582;

    // Calculate the heat index
    float heatIndex = c1 + c2 * temperature + c3 * humidity + c4 * temperature * humidity +
                      c5 * temperature * temperature + c6 * humidity * humidity +
                      c7 * temperature * temperature * humidity + c8 * temperature * humidity * humidity +
                      c9 * temperature * temperature * humidity * humidity;

    return heatIndex;
  } else {
    return temperature; // Return the actual temperature if outside the valid range for heat index calculation
  }
}
