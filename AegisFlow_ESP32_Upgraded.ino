#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <Preferences.h>
#include <math.h>

// ==================================================
// PIN CONFIGURATION
// ==================================================

#define DHTPIN 18
#define DHTTYPE DHT11
#define GAS_PIN 34
#define RELAY_PIN 25

#define RELAY_ACTIVE_LOW true

// ==================================================
// WIFI CONFIGURATION
// ==================================================

#include "secrets.h"
// WIFI_SSID and WIFI_PASSWORD are now defined in secrets.h


// ==================================================
// MQTT CONFIGURATION
// ==================================================

const char* MQTT_BROKER = "broker.emqx.io";
const int MQTT_PORT = 1883;

// Current sensor topics
const char* TOPIC_TEMPERATURE =
  "Team4/sensor/temperature";

const char* TOPIC_HUMIDITY =
  "Team4/sensor/humidity";

const char* TOPIC_GAS =
  "Team4/sensor/gas";

// Risk topics
const char* TOPIC_RISK =
  "Team4/ventilation/risk";

const char* TOPIC_GAS_DEVIATION =
  "Team4/ventilation/gasDeviation";

const char* TOPIC_GAS_TREND =
  "Team4/ventilation/gasTrend";

const char* TOPIC_LEVEL =
  "Team4/ventilation/level";

// Baseline topics
const char* TOPIC_BASELINE_TEMPERATURE =
  "Team4/baseline/temperature";

const char* TOPIC_BASELINE_HUMIDITY =
  "Team4/baseline/humidity";

const char* TOPIC_BASELINE_GAS =
  "Team4/baseline/gas";

// ==================================================
// OBJECTS
// ==================================================

DHT dht(DHTPIN, DHTTYPE);
WiFiClient espClient;
PubSubClient mqttClient(espClient);
Preferences preferences;

// ==================================================
// BASELINE
// ==================================================

float baselineTemp = 0;
float baselineHumidity = 0;
float baselineGas = 0;

bool baselineExists = false;

// ==================================================
// GAS FILTERING
// ==================================================

float filteredGas = 0;
float previousGas = 0;
bool firstGasReading = true;

// ==================================================
// FAN AND TIMING
// ==================================================

bool fanState = false;

unsigned long lastSensorRead = 0;
const unsigned long SENSOR_INTERVAL = 5000;

// ==================================================
// RELAY CONTROL
// ==================================================

void setFan(bool fanOn) {

  if (RELAY_ACTIVE_LOW) {

    digitalWrite(
      RELAY_PIN,
      fanOn ? LOW : HIGH
    );

  } else {

    digitalWrite(
      RELAY_PIN,
      fanOn ? HIGH : LOW
    );
  }

  Serial.print("Fan: ");
  Serial.println(fanOn ? "ON" : "OFF");
}

// ==================================================
// WIFI CONNECTION
// ==================================================

void connectWiFi() {

  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  Serial.print("Connecting to WiFi");

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );

  while (WiFi.status() != WL_CONNECTED) {

    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected!");

  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

// ==================================================
// MQTT CONNECTION
// ==================================================

void connectMQTT() {

  while (!mqttClient.connected()) {

    Serial.print("Connecting to MQTT...");

    String clientID = "AegisFlow-";
    clientID += String(random(0xffff), HEX);

    if (mqttClient.connect(clientID.c_str())) {

      Serial.println("Connected!");

      // Publish baseline immediately
      if (baselineExists) {

        String baselineTempString =
          String(baselineTemp, 2);

        String baselineHumidityString =
          String(baselineHumidity, 2);

        String baselineGasString =
          String(baselineGas, 2);

        mqttClient.publish(
          TOPIC_BASELINE_TEMPERATURE,
          baselineTempString.c_str(),
          true
        );

        mqttClient.publish(
          TOPIC_BASELINE_HUMIDITY,
          baselineHumidityString.c_str(),
          true
        );

        mqttClient.publish(
          TOPIC_BASELINE_GAS,
          baselineGasString.c_str(),
          true
        );

        Serial.println(
          "Baseline published to MQTT"
        );
      }

    } else {

      Serial.print("MQTT failed: ");
      Serial.println(mqttClient.state());

      delay(3000);
    }
  }
}

// ==================================================
// LOAD BASELINE
// ==================================================

void loadBaseline() {

  preferences.begin(
    "ventilation",
    true
  );

  baselineExists =
    preferences.getBool(
      "exists",
      false
    );

  if (baselineExists) {

    baselineTemp =
      preferences.getFloat(
        "temp",
        0
      );

    baselineHumidity =
      preferences.getFloat(
        "humidity",
        0
      );

    baselineGas =
      preferences.getFloat(
        "gas",
        0
      );
  }

  preferences.end();

  Serial.println();
  Serial.println(
    "========== BASELINE =========="
  );

  if (baselineExists) {

    Serial.print("Temperature: ");
    Serial.print(baselineTemp);
    Serial.println(" C");

    Serial.print("Humidity: ");
    Serial.print(baselineHumidity);
    Serial.println(" %");

    Serial.print("Gas: ");
    Serial.println(baselineGas);

  } else {

    Serial.println("No baseline found!");
    Serial.println("Run calibration first.");
  }

  Serial.println(
    "=============================="
  );
}

// ==================================================
// MQTT PUBLISH
// ==================================================

void publishData(
  float temperature,
  float humidity,
  float gas,
  float risk,
  float gasDeviation,
  float gasTrend,
  int ventilationLevel
) {

  // Current values
  String tempString =
    String(temperature, 2);

  String humidityString =
    String(humidity, 2);

  String gasString =
    String(gas, 2);

  String riskString =
    String(risk, 2);

  String deviationString =
    String(gasDeviation, 2);

  String trendString =
    String(gasTrend, 2);

  String levelString =
    String(ventilationLevel);

  // ================================================
  // CURRENT SENSOR VALUES
  // ================================================

  mqttClient.publish(
    TOPIC_TEMPERATURE,
    tempString.c_str()
  );

  mqttClient.publish(
    TOPIC_HUMIDITY,
    humidityString.c_str()
  );

  mqttClient.publish(
    TOPIC_GAS,
    gasString.c_str()
  );

  // ================================================
  // RISK VALUES
  // ================================================

  mqttClient.publish(
    TOPIC_RISK,
    riskString.c_str()
  );

  mqttClient.publish(
    TOPIC_GAS_DEVIATION,
    deviationString.c_str()
  );

  mqttClient.publish(
    TOPIC_GAS_TREND,
    trendString.c_str()
  );

  mqttClient.publish(
    TOPIC_LEVEL,
    levelString.c_str()
  );

  // ================================================
  // BASELINE VALUES - RETAINED
  // ================================================

  String baselineTempString =
    String(baselineTemp, 2);

  String baselineHumidityString =
    String(baselineHumidity, 2);

  String baselineGasString =
    String(baselineGas, 2);

  mqttClient.publish(
    TOPIC_BASELINE_TEMPERATURE,
    baselineTempString.c_str(),
    true
  );

  mqttClient.publish(
    TOPIC_BASELINE_HUMIDITY,
    baselineHumidityString.c_str(),
    true
  );

  mqttClient.publish(
    TOPIC_BASELINE_GAS,
    baselineGasString.c_str(),
    true
  );
}

// ==================================================
// SETUP
// ==================================================

void setup() {

  Serial.begin(115200);

  dht.begin();

  pinMode(
    RELAY_PIN,
    OUTPUT
  );

  // Fan OFF at startup
  setFan(false);

  randomSeed(micros());

  Serial.println();
  Serial.println(
    "================================="
  );

  Serial.println(
    "       AEGISFLOW SYSTEM"
  );

  Serial.println(
    "================================="
  );

  // Load calibrated baseline
  loadBaseline();

  // Connect WiFi
  connectWiFi();

  // Configure MQTT
  mqttClient.setServer(
    MQTT_BROKER,
    MQTT_PORT
  );

  Serial.println(
    "System ready!"
  );
}

// ==================================================
// MAIN LOOP
// ==================================================

void loop() {

  // ==================================================
  // WIFI
  // ==================================================

  if (
    WiFi.status() != WL_CONNECTED
  ) {

    connectWiFi();
  }

  // ==================================================
  // MQTT
  // ==================================================

  if (
    !mqttClient.connected()
  ) {

    connectMQTT();
  }

  mqttClient.loop();

  // ==================================================
  // SENSOR TIMING
  // ==================================================

  unsigned long currentMillis =
    millis();

  if (
    currentMillis -
    lastSensorRead <
    SENSOR_INTERVAL
  ) {

    return;
  }

  lastSensorRead =
    currentMillis;

  // ==================================================
  // READ SENSORS
  // ==================================================

  float temperature =
    dht.readTemperature();

  float humidity =
    dht.readHumidity();

  int rawGas =
    analogRead(GAS_PIN);

  // ==================================================
  // DHT VALIDATION
  // ==================================================

  if (
    isnan(temperature) ||
    isnan(humidity)
  ) {

    Serial.println(
      "DHT reading failed!"
    );

    return;
  }

  // ==================================================
  // GAS FILTERING
  // ==================================================

  if (firstGasReading) {

    filteredGas =
      rawGas;

    previousGas =
      filteredGas;

    firstGasReading =
      false;

  } else {

    filteredGas =
      (filteredGas * 0.7) +
      (rawGas * 0.3);
  }

  // ==================================================
  // GAS TREND
  // ==================================================

  float gasTrend =
    filteredGas -
    previousGas;

  previousGas =
    filteredGas;

  // ==================================================
  // BASELINE VALIDATION
  // ==================================================

  if (
    !baselineExists ||
    baselineGas <= 0
  ) {

    Serial.println(
      "Baseline unavailable!"
    );

    Serial.println(
      "Run calibration first."
    );

    setFan(false);

    fanState =
      false;

    return;
  }

  // ==================================================
  // DIFFERENCES FROM BASELINE
  // ==================================================

  float temperatureDifference =
    abs(
      temperature -
      baselineTemp
    );

  float humidityDifference =
    abs(
      humidity -
      baselineHumidity
    );

  float gasDeviation =
    (
      (filteredGas -
       baselineGas)
      /
      baselineGas
    ) * 100.0;

  // Only positive gas deviation contributes
  float positiveGasDeviation =
    max(
      0.0f,
      gasDeviation
    );

  // Only positive gas trend contributes
  float positiveGasTrend =
    max(
      0.0f,
      gasTrend
    );

  // ==================================================
  // REDUCED RISK CALCULATION
  // MAXIMUM = 40
  // ==================================================

  float temperatureRisk =
    (
      temperatureDifference /
      5.0
    ) * 8.0;

  float humidityRisk =
    (
      humidityDifference /
      15.0
    ) * 8.0;

  float gasRisk =
    (
      positiveGasDeviation /
      25.0
    ) * 16.0;

  float trendRisk =
    (
      positiveGasTrend /
      50.0
    ) * 8.0;

  // ==================================================
  // LIMIT INDIVIDUAL CONTRIBUTIONS
  // ==================================================

  temperatureRisk =
    constrain(
      temperatureRisk,
      0.0,
      8.0
    );

  humidityRisk =
    constrain(
      humidityRisk,
      0.0,
      8.0
    );

  gasRisk =
    constrain(
      gasRisk,
      0.0,
      16.0
    );

  trendRisk =
    constrain(
      trendRisk,
      0.0,
      8.0
    );

  // ==================================================
  // FINAL RISK SCORE
  // MAXIMUM = 40
  // ==================================================

  float riskScore =
    temperatureRisk +
    humidityRisk +
    gasRisk +
    trendRisk;

  riskScore =
    constrain(
      riskScore,
      0.0,
      40.0
    );

  // ==================================================
  // RISK LEVEL
  // ==================================================

  int ventilationLevel = 0;

  if (riskScore < 8) {

    ventilationLevel = 0;

  } else if (riskScore < 16) {

    ventilationLevel = 1;

  } else if (riskScore < 28) {

    ventilationLevel = 2;

  } else {

    ventilationLevel = 3;
  }

  // ==================================================
  // FAN CONTROL
  //
  // FAN ON  >= 10
  // FAN OFF < 10
  // ==================================================

  if (
    riskScore >= 10 &&
    !fanState
  ) {

    fanState = true;

    setFan(true);

    Serial.println(
      "WARNING: Risk >= 10 - FAN ON"
    );

  } else if (
    riskScore < 10 &&
    fanState
  ) {

    fanState = false;

    setFan(false);

    Serial.println(
      "Risk < 10 - FAN OFF"
    );
  }

  // ==================================================
  // SERIAL MONITOR
  // ==================================================

  Serial.println();
  Serial.println(
    "======= CURRENT CONDITIONS ======="
  );

  Serial.print(
    "Temperature: "
  );

  Serial.print(
    temperature
  );

  Serial.println(
    " C"
  );

  Serial.print(
    "Baseline Temperature: "
  );

  Serial.print(
    baselineTemp
  );

  Serial.println(
    " C"
  );

  Serial.print(
    "Temperature Difference: "
  );

  Serial.print(
    temperatureDifference
  );

  Serial.println(
    " C"
  );

  Serial.println();

  Serial.print(
    "Humidity: "
  );

  Serial.print(
    humidity
  );

  Serial.println(
    " %"
  );

  Serial.print(
    "Baseline Humidity: "
  );

  Serial.print(
    baselineHumidity
  );

  Serial.println(
    " %"
  );

  Serial.print(
    "Humidity Difference: "
  );

  Serial.print(
    humidityDifference
  );

  Serial.println(
    " %"
  );

  Serial.println();

  Serial.print(
    "Raw Gas: "
  );

  Serial.println(
    rawGas
  );

  Serial.print(
    "Filtered Gas: "
  );

  Serial.println(
    filteredGas
  );

  Serial.print(
    "Baseline Gas: "
  );

  Serial.println(
    baselineGas
  );

  Serial.print(
    "Gas Deviation: "
  );

  Serial.print(
    gasDeviation
  );

  Serial.println(
    " %"
  );

  Serial.print(
    "Gas Trend: "
  );

  Serial.println(
    gasTrend
  );

  Serial.println();

  // ================================================
  // RISK BREAKDOWN
  // ================================================

  Serial.print(
    "Temperature Risk: "
  );

  Serial.println(
    temperatureRisk
  );

  Serial.print(
    "Humidity Risk: "
  );

  Serial.println(
    humidityRisk
  );

  Serial.print(
    "Gas Risk: "
  );

  Serial.println(
    gasRisk
  );

  Serial.print(
    "Trend Risk: "
  );

  Serial.println(
    trendRisk
  );

  Serial.println();

  Serial.print(
    "Risk Score: "
  );

  Serial.print(
    riskScore
  );

  Serial.println(
    " / 40"
  );

  Serial.print(
    "Risk Level: "
  );

  Serial.println(
    ventilationLevel
  );

  Serial.print(
    "Fan State: "
  );

  Serial.println(
    fanState ? "ON" : "OFF"
  );

  Serial.println(
    "=================================="
  );

  // ==================================================
  // MQTT PUBLISH
  // ==================================================

  publishData(
    temperature,
    humidity,
    filteredGas,
    riskScore,
    gasDeviation,
    gasTrend,
    ventilationLevel
  );
}