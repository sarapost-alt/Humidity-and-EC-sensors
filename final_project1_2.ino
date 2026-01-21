#include <WiFi.h>
#include <ThingSpeak.h>
#include "time.h"
#include <PubSubClient.h>

// ================= הגדרות רשת =================
const char* ssid = "agrotech";
const char* password = "1Afuna2gezer";

// ================= הגדרות ThingSpeak =================
unsigned long myChannelNumber = 3216807;
const char * myWriteAPIKey = "630G2F9IRBJZ3ZU1";

// הפרדה בין לקוח MQTT ללקוח ThingSpeakת!)
WiFiClient mqttClient;
WiFiClient tsClient;

PubSubClient client(mqttClient);

// ================= הגדרות MQTT =================
const char* mqtt_server = "192.168.0.102";
const int mqtt_port = 1883;
const char* mqtt_user = "mqtt-user";
const char* mqtt_password = "1234";

// -----------------------------------------------------------
// הגדרת כתובות הברזים המרוחקים (OUTPUT)
// -----------------------------------------------------------
const char* topic_remote_valve1 = "/greenhouse/outside/irrigation/solenoid3";
const char* topic_remote_valve2 = "/greenhouse/outside/irrigation/solenoid2";
const char* topic_remote_pump   = "/greenhouse/outside/irrigation/fertigation"; // fert pump

// -----------------------------------------------------------
// הגדרת כתובות לקבלת פקודות ידניות מהבית (INPUT)
// -----------------------------------------------------------
const char* topic_cmd_mode   = "greenhouse/controller/mode"; // 1=Manual, 0=Auto
const char* topic_cmd_valve1 = "greenhouse/controller/manual/valve1";
const char* topic_cmd_valve2 = "greenhouse/controller/manual/valve2";
const char* topic_cmd_pump   = "greenhouse/controller/manual/pump";

// -----------------------------------------------------------
// הגדרת כתובות לדיווח סטטוס (FEEDBACK)
// -----------------------------------------------------------
const char* topic_state_valve1 = "greenhouse/controller/state/valve1";
const char* topic_state_valve2 = "greenhouse/controller/state/valve2";
const char* topic_state_pump   = "greenhouse/controller/state/pump";

// ================= משתנים גלובליים =================
bool manualMode = false;

// משתנים לשמירת המצב הרצוי
bool target_v1 = false;
bool target_v2 = false;
bool target_pump = false;

// משתנים לשמירת המצב האחרון שנשלח (למניעת הצפה)
bool last_sent_v1 = false;
bool last_sent_v2 = false;
bool last_sent_pump = false;

// ================= הגדרות זמן ופינים (חיישנים בלבד) =================
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 7200;
const int   daylightOffset_sec = 3600;

// חיישנים מקומיים (ADC1 - מומלץ)
const int PIN_MOISTURE_1 = 36;
const int PIN_EC_1       = 39;
const int PIN_MOISTURE_2 = 34;
const int PIN_EC_2       = 35;

unsigned long lastUploadTime = 0;
const unsigned long uploadInterval = 15000;
// ================= פונקציית Callback =================
void callback(char* topic, byte* payload, unsigned int length) {
  if (length == 0) return; // הגנה

  String topicStr = String(topic);
  char value = (char)payload[0]; // '1' or '0'

  if (topicStr == topic_cmd_mode) {
    manualMode = (value == '1');
    Serial.println(manualMode ? "Mode: MANUAL" : "Mode: AUTO");
  }

  if (manualMode) {
    if (topicStr == topic_cmd_valve1) target_v1 = (value == '1');
    if (topicStr == topic_cmd_valve2) target_v2 = (value == '1');
    if (topicStr == topic_cmd_pump)   target_pump = (value == '1');
  }
}

void ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.print("WiFi disconnected. Reconnecting");
  WiFi.disconnect();
  WiFi.begin(ssid, password);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi connected. IP: ");
    Serial.println(WiFi.localIP());
    // זמן (NTP) אחרי שיש רשת
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  } else {
    Serial.println("WiFi reconnect failed (timeout).");
  }
}

void reconnectMQTT() {
  if (client.connected()) return;

  // נסיונות חיבור בלי לולאה אינסופית אגרסיבית
  Serial.print("Connecting to MQTT...");
  String clientId = "ESP32_Master_Controller_";
  clientId += String((uint32_t)ESP.getEfuseMac(), HEX);

  if (client.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
    Serial.println("connected");
    client.subscribe(topic_cmd_mode);
    client.subscribe(topic_cmd_valve1);
    client.subscribe(topic_cmd_valve2);
    client.subscribe(topic_cmd_pump);
  } else {
    Serial.print("failed, rc=");
    Serial.println(client.state());
  }
}

// ================= לוגיקת ההשקיה המעודכנת =================

void calculateAutoSchedule(int hour, int minute) {
  target_v1 = false;
  target_v2 = false;
  target_pump = false;

  // --- ברז 1: שעה ביום (07:00-08:00), ללא דשן ---
  if (hour == 17) {
    target_v1 = true;
    target_pump = false;
  }
  // --- ברז 2: שעה ביום (08:00-09:00), דשן עד דקה 58 ---
  else if (hour == 14  ) {
    target_v2 = true;
    if (minute < 58) {
      target_pump = true;  // דשן פתוח
    } else {
      target_pump = false; // שטיפה ב-2 דקות האחרונות
    }
  }
}


// ================= שליחת פקודות MQTT =================
void publishChanges() {
  
  // --- ניהול ברז 1 ---
  if (target_v1 != last_sent_v1) {
    const char* payload = target_v1 ? "1" : "0";
    // התיקון: שימוש ב-if כדי לבדוק אם השליחה הצליחה
    if (client.publish(topic_remote_valve1, payload)) {
       Serial.print("Success: Valve 1 -> "); Serial.println(payload);
       client.publish(topic_state_valve1, payload); // עדכון הסטטוס לבית רק אם הצליח
       last_sent_v1 = target_v1; // רק עכשיו מותר לנו לעדכן את הזיכרון!
    } else {
       Serial.println("Error: Failed to send command to Valve 1. Will retry next loop.");
       // לא מעדכנים את last_sent_v1, כדי שבסיבוב הבא הוא ינסה שוב!
    }
  }

  // --- ניהול ברז 2 ---
  if (target_v2 != last_sent_v2) {
    const char* payload = target_v2 ? "1" : "0";
    if (client.publish(topic_remote_valve2, payload)) {
       Serial.print("Success: Valve 2 -> "); Serial.println(payload);
       client.publish(topic_state_valve2, payload);
       last_sent_v2 = target_v2;
    } else {
       Serial.println("Error: Failed to send command to Valve 2.");
    }
  }

  // --- ניהול משאבה ---
  if (target_pump != last_sent_pump) {
    const char* payload = target_pump ? "1" : "0";
    if (client.publish(topic_remote_pump, payload)) {
       Serial.print("Success: Pump -> "); Serial.println(payload);
       client.publish(topic_state_pump, payload);
       last_sent_pump = target_pump;
    } else {
       Serial.println("Error: Failed to send command to Pump.");
    }
  }
}

// ================= הגדרות כיול לחות (נשאר כמו קודם) =================
const int S1_DRY = 0;   
const int S1_SAT = 479; 

const int S2_DRY = 0;   
const int S2_SAT = 538; 

// ================= הגדרות כיול EC (מחושב לפי הנתונים החדשים) =================
// פקטור המרה: הערך הרצוי (50) חלקי הקריאה שהתקבלה
const float EC1_FACTOR = 2.2/1650; 
const float EC2_FACTOR = 2.2/1650;

// ======================================================================

void readAndUploadSensors() {
  Serial.println("Reading Sensors...");

  int raw_m1 = analogRead(PIN_MOISTURE_1);
  int raw_ec1 = analogRead(PIN_EC_1);
  int raw_m2 = analogRead(PIN_MOISTURE_2);
  int raw_ec2 = analogRead(PIN_EC_2);

  // --- 1. חישוב לחות (באחוזים 0-100) ---
  int sat_index_1 = map(raw_m1, S1_DRY, S1_SAT, 0, 100);
  int sat_index_2 = map(raw_m2, S2_DRY, S2_SAT, 0, 100);
  
  // הגנה מחריגות
  sat_index_1 = constrain(sat_index_1, 0, 100);
  sat_index_2 = constrain(sat_index_2, 0, 100);

  // --- 2. חישוב EC (ביחידות שלך - 0 עד 50) ---
  float final_ec1 = raw_ec1 * EC1_FACTOR;
  float final_ec2 = raw_ec2 * EC2_FACTOR;

  // --- דיבאג ל-Serial Monitor ---
  Serial.println("-----------------------------");
  Serial.print("Line 1 | Moisture: "); Serial.print(sat_index_1); 
  Serial.print("% | EC: "); Serial.println(final_ec1);
  
  Serial.print("Line 2 | Moisture: "); Serial.print(sat_index_2); 
  Serial.print("% | EC: "); Serial.println(final_ec2);
  Serial.println("-----------------------------");

  // --- שליחה ל-ThingSpeak ---
  ThingSpeak.setField(1, sat_index_1);
  ThingSpeak.setField(2, final_ec1);     
  ThingSpeak.setField(3, sat_index_2);
  ThingSpeak.setField(4, final_ec2);

  int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
  if(x == 200){
    Serial.println("ThingSpeak update successful.");
  } else {
    Serial.println("Error updating ThingSpeak: " + String(x));
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);

  // הגדרות ADC כלליות ליציבות (לא כיול חיישן)
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi connected. IP: ");
    Serial.println(WiFi.localIP());
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  } else {
    Serial.println("WiFi connection failed (timeout).");
  }

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  ThingSpeak.begin(tsClient);
}

void loop() {
  ensureWiFi();

  reconnectMQTT();
  client.loop();

  // 1. קביעת המצב הרצוי (Target Logic)
  if (!manualMode) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      calculateAutoSchedule(timeinfo.tm_hour, timeinfo.tm_min);
    } else {
      // אם אין זמן, לא משנים לוגיקה (נשארים עם מצב קודם)
      Serial.println("NTP time not available yet.");
    }
  }

  // 2. שליחת פקודות לברזים המרוחקים
  publishChanges();

  // 3. קריאת חיישנים ושליחה לענן
  if (millis() - lastUploadTime > uploadInterval) {
    readAndUploadSensors();
    lastUploadTime = millis();
  }
}
