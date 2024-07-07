#include <RTClib.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <UrlEncode.h>
#include <NTPClient.h>

# define DEBUG_MODE false

# define CLOSING 1
# define CLOSED 2
# define OPENING 3
# define OPEN 4

# define TOP_END_SWITCH_PIN 32
# define BOTTOM_END_SWITCH_PIN 33

# define MOTOR_POWER_PIN 25
# define MOTOR_UP_PIN 27
# define MOTOR_DOWN_PIN 26

# define VOLTAGE_MONITOR_PIN 34

# define CYCLE_TIME 1000 // ms
# define ERROR_AFTER_CYCLES_AWAKE 120

// values for sleep timer
#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  300      /* Time ESP32 will go to sleep (in seconds) */

const char* ssid = "Kapelln2a";
const char* password = "!a2nllepaK#";

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

// +international_country_code + phone number
// Portugal +351, example: +351912345678
String phoneNumber = "436607002655";
String apiKey = "2729443";

String messageToSend = "";

String temperature = "";

// rtc object and time var for the clock
RTC_DS3231 rtc;
DateTime now;

const float voltageDivider = 98;
float voltage = 0;

int state = 0;
int nextState = 0;
int cycles = 0;

bool shouldUpdateTime = false;

// Array mit Morgendämmerungszeiten (Minuten seit Mitternacht)
const int dawnTimes[] = {
   714, 709, 704, 658, 652, 645, 639, 632,
   625, 619, 612, 606, 600, 554, 549, 544,
   540, 535, 532, 528, 526, 523, 521, 520,
   519, 518, 518, 519, 519, 520, 521, 523,
   524, 526, 528, 530, 532, 534, 537, 539,
   542, 544, 547, 549, 552, 555, 557, 600,
   603, 605, 608, 611
};

// Array mit Abenddämmerungszeiten (Minuten seit Mitternacht)
const int duskTimes[] = {
   1734, 1746, 1758, 1810, 1822, 1834, 1846, 1858,
   1911, 1922, 1934, 1946, 1957, 2008, 2019, 2029,
   2039, 2049, 2058, 2107, 2115, 2123, 2130, 2136,
   2142, 2147, 2152, 2156, 2159, 2202, 2205, 2207,
   2208, 2209, 2209, 2209, 2208, 2207, 2205, 2203,
   2201, 2158, 2154, 2151, 2147, 2143, 2139, 2134,
   2130, 2125, 2121, 2116
};

bool isChickenAwake() {
  int currentMinute = (now.hour() * 100) + now.minute();

  int weekNumber = (int)((now.month() -1) * 4.33 + now.day() / 7);
  // Serial.print("weekNumber: ");
  // Serial.println(weekNumber)

  if (weekNumber < 1 || weekNumber > 52) {
    weekNumber = 1;
  }

  int dawnTime = dawnTimes[weekNumber - 1];
  int duskTime = duskTimes[weekNumber - 1];

  if (currentMinute >= dawnTime && currentMinute < duskTime) {
    return true; // Es ist Tag (Tageslicht)
  } else {
    return false; // Es ist Nacht
  }
}

void updateRTC() {
  startWiFi();
  timeClient.begin();
  if(now.month() > 3 && now.month() < 11) timeClient.setTimeOffset(7200); // Offset für Sommerzeit
  else timeClient.setTimeOffset(3600); // Offset für Winterzeit
  timeClient.update();
  time_t epochTime = timeClient.getEpochTime();
  rtc.adjust(DateTime(epochTime));
  // clear all alarms, just in case
  rtc.clearAlarm(1);
  rtc.clearAlarm(2);
  messageToSend = "RTC aktualisiert";
}

void storeValue(char* to, int val, char behind) {
  *to++ = '0' + val / 10;
  *to++ = '0' + val % 10;
  *to++ = behind;
}

char* getTime() {
  static char Buffer[10];
  storeValue(Buffer, now.hour(), ':');
  storeValue(Buffer + 3, now.minute() % 60, ':');
  storeValue(Buffer + 6, now.second() % 60, 0);
  return Buffer;
}

// find out, what state it is in, when power on
int detectState() {
  bool chickenAwake = isChickenAwake();
  int detectedState = 0;
  if(digitalRead(BOTTOM_END_SWITCH_PIN) && !chickenAwake) detectedState = CLOSING;
  if(!digitalRead(BOTTOM_END_SWITCH_PIN) && !chickenAwake) detectedState = CLOSED;
  if(digitalRead(TOP_END_SWITCH_PIN) && chickenAwake) detectedState = OPENING;
  if(!digitalRead(TOP_END_SWITCH_PIN) && chickenAwake) detectedState = OPEN;
  return detectedState;
}

bool startWiFi() {
  if(WiFi.status() == WL_CONNECTED) return true;
  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  int retries = 0;
  while(WiFi.status() != WL_CONNECTED && retries < 50) {
    delay(500);
    Serial.print(".");
    retries++;
  }
  if(WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.print("Connected to WiFi network with IP Address: ");
    Serial.println(WiFi.localIP());
    return true;
  }
  return false;
}

void sendMessage(String message) {
  //add time, voltage and temp to message
  delay(500);
  voltage = analogRead(VOLTAGE_MONITOR_PIN) / voltageDivider;
  Serial.println(voltage);

  String composedMessage = (String)getTime() + " " + message + "\nTemperatur: " + temperature + "°C\nSpannung: " + voltage + "V";
  messageToSend = "";

  if (DEBUG_MODE) {
    Serial.print("DEBUG. Message to send: ");
    Serial.println(composedMessage);
  } else {

    if(startWiFi()) {
      // Send Message to WhatsAPP
      // Data to send with HTTP POST
      String url = "http://api.callmebot.com/whatsapp.php?phone=" + phoneNumber + "&apikey=" + apiKey + "&text=" + urlEncode(composedMessage);
      WiFiClient client;    
      HTTPClient http;
      http.begin(client, url);

      // Specify content-type header
      http.addHeader("Content-Type", "application/x-www-form-urlencoded");
      
      // Send HTTP POST request
      int httpResponseCode = http.POST(url);
      if (httpResponseCode == 200){
        Serial.print("Message sent successfully");
      }
      else{
        Serial.println("Error sending the message");
        Serial.print("HTTP response code: ");
        Serial.println(httpResponseCode);
      }

      // Free resources
      http.end();
    }
    
  }

}

void updateMotorState() {
  digitalWrite(MOTOR_POWER_PIN, LOW);
  digitalWrite(MOTOR_UP_PIN, LOW);
  digitalWrite(MOTOR_DOWN_PIN, LOW);
  if(state == OPENING || state == CLOSING) digitalWrite(MOTOR_POWER_PIN, HIGH);
  if(state == OPENING) digitalWrite(MOTOR_UP_PIN, HIGH);
  if(state == CLOSING) digitalWrite(MOTOR_DOWN_PIN, HIGH);
}

void changeState(int stateToSet) {
  nextState = state + 1;
  if(nextState > 4) nextState = 1;
  
  if(stateToSet == 0) {
    stateToSet = nextState;
  }

  state = stateToSet;
  
  if(state == OPENING && !digitalRead(TOP_END_SWITCH_PIN)) state = OPEN;
  if(state == CLOSING && !digitalRead(BOTTOM_END_SWITCH_PIN)) state = CLOSED;
  if(state == OPENING) 
    attachInterrupt(digitalPinToInterrupt(TOP_END_SWITCH_PIN), topEndSwitchInterrupt, FALLING);
  if(state == CLOSING) 
    attachInterrupt(digitalPinToInterrupt(BOTTOM_END_SWITCH_PIN), bottomEndSwitchInterrupt, FALLING);
  Serial.print("changed State to ");
  Serial.println(getStateName(state));
  updateMotorState();
}

void topEndSwitchInterrupt() {
  Serial.println("top interrupt");
  detachInterrupt(digitalPinToInterrupt(TOP_END_SWITCH_PIN));
  if(state == OPENING) {
    changeState(OPEN);
    shouldUpdateTime = true;
    messageToSend = "Hühnertür ist jetzt offen!";
  }
}

void bottomEndSwitchInterrupt() {
  Serial.println("bottom interrupt");
  detachInterrupt(digitalPinToInterrupt(BOTTOM_END_SWITCH_PIN));
  if(state == CLOSING) {
    changeState(CLOSED);
    messageToSend = "Hühnertür ist jetzt geschloßen!";
  }
}

String getStateName(int stateToGetNameFrom) {
  if(stateToGetNameFrom == CLOSING) return "CLOSING";
  if(stateToGetNameFrom == CLOSED) return "CLOSED";
  if(stateToGetNameFrom == OPENING) return "OPENING";
  if(stateToGetNameFrom == OPEN) return "OPEN";
  return "UNKNOWN";
}

void setup() {
  // start serial connection for debugging
  Serial.begin(115200);
  Serial.println("Hello, Chicken Coop");

  //set all pin modes
  pinMode(TOP_END_SWITCH_PIN, INPUT_PULLUP);
  pinMode(BOTTOM_END_SWITCH_PIN, INPUT_PULLUP);

  pinMode(MOTOR_POWER_PIN, OUTPUT);
  pinMode(MOTOR_UP_PIN, OUTPUT);
  pinMode(MOTOR_DOWN_PIN, OUTPUT);

  cycles = 0;

  // connect rtc module and fetch current time
  if (! rtc.begin()) {
    Serial.println("RTC module is NOT found");
    Serial.flush();
    while (1);
  }
  now = rtc.now();

  int detectedState = detectState();
  changeState(detectedState);
}

void loop() {
  delay(CYCLE_TIME);
  now = rtc.now();
  Serial.println(getTime());
  temperature = rtc.getTemperature();
  // Serial.println(temperature);

  bool chickenAwake = isChickenAwake();

  if(state == CLOSED && chickenAwake) changeState(OPENING);
  if(state == OPEN && !chickenAwake) changeState(CLOSING);
  if(messageToSend != "") {
    sendMessage(messageToSend);
  }
  
  if(shouldUpdateTime) {
    updateRTC();
    shouldUpdateTime = false;
  }
  
  if(messageToSend == "" && (state == OPEN || state == CLOSED)) 
  {
    Serial.println("Go to sleep...");
    Serial.println("");
    if(WiFi.status() == WL_CONNECTED) WiFi.disconnect();
    if (DEBUG_MODE) esp_sleep_enable_timer_wakeup(5 * uS_TO_S_FACTOR);
    else esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    Serial.flush(); 
    esp_deep_sleep_start();
  }
  cycles++;
  // Serial.println(cycles);
  if(cycles > ERROR_AFTER_CYCLES_AWAKE) {
    messageToSend = "Controller ist zu lange wach. Bitte prüfen!";
    cycles = 0;
  }
}