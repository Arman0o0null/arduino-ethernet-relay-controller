#include <SPI.h>
#include <Ethernet2.h>

// Relay pins
#define RELAY1_PIN 5
#define RELAY2_PIN 6
#define RELAY3_PIN 7
#define RELAY4_PIN 8

// Status LED
#define STATUS_LED 13

// Network settings
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 177);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
EthernetServer server(80);

// Time settings
struct TimeWindow {
  uint8_t startHour = 8;
  uint8_t startMinute = 0;
  uint8_t endHour = 16;
  uint8_t endMinute = 0;
};
TimeWindow activeWindow;

// Relay states
bool relayStates[4] = {false, false, false, false};
bool systemActive = true;
bool ntpMode = true;
unsigned long lastTimeCheck = 0;
unsigned long currentSeconds = 0;

void setup() {
  Serial.begin(9600);

  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(RELAY3_PIN, OUTPUT);
  pinMode(RELAY4_PIN, OUTPUT);
  pinMode(STATUS_LED, OUTPUT);

  Ethernet.begin(mac, ip, gateway, subnet);
  server.begin();

  Serial.print(F("Relay Controller started at "));
  Serial.println(Ethernet.localIP());
}

void loop() {
  if (!ntpMode && millis() - lastTimeCheck >= 1000) {
    lastTimeCheck = millis();
    currentSeconds++;
    if (currentSeconds >= 86400) currentSeconds = 0;
    checkTimeWindow();
  }

  EthernetClient client = server.available();
  if (client) {
    handleWebRequest(client);
    client.stop();
  }

  digitalWrite(RELAY1_PIN, systemActive && relayStates[0] ? HIGH : LOW);
  digitalWrite(RELAY2_PIN, systemActive && relayStates[1] ? HIGH : LOW);
  digitalWrite(RELAY3_PIN, systemActive && relayStates[2] ? HIGH : LOW);
  digitalWrite(RELAY4_PIN, systemActive && relayStates[3] ? HIGH : LOW);

  digitalWrite(STATUS_LED, systemActive ? HIGH : LOW);
  delay(100);
}

void checkTimeWindow() {
  unsigned long startTime = activeWindow.startHour * 3600UL + activeWindow.startMinute * 60UL;
  unsigned long endTime = activeWindow.endHour * 3600UL + activeWindow.endMinute * 60UL;

  if (activeWindow.endHour < activeWindow.startHour) {
    systemActive = (currentSeconds >= startTime) || (currentSeconds < endTime);
  } else {
    systemActive = (currentSeconds >= startTime) && (currentSeconds < endTime);
  }
}

void handleWebRequest(EthernetClient client) {
  String request = client.readStringUntil('\r');
  client.flush();

  if (request.indexOf("GET /relay1/on") != -1) relayStates[0] = true;
  if (request.indexOf("GET /relay1/off") != -1) relayStates[0] = false;
  if (request.indexOf("GET /relay2/on") != -1) relayStates[1] = true;
  if (request.indexOf("GET /relay2/off") != -1) relayStates[1] = false;
  if (request.indexOf("GET /relay3/on") != -1) relayStates[2] = true;
  if (request.indexOf("GET /relay3/off") != -1) relayStates[2] = false;
  if (request.indexOf("GET /relay4/on") != -1) relayStates[3] = true;
  if (request.indexOf("GET /relay4/off") != -1) relayStates[3] = false;

  if (request.indexOf("GET /settime?") != -1) {
    int startIdx = request.indexOf("start=") + 6;
    int endIdx = request.indexOf("&end=");
    int endEndIdx = request.indexOf(" ", endIdx);
    String startTime = request.substring(startIdx, endIdx);
    String endTime = request.substring(endIdx + 5, endEndIdx);
    activeWindow.startHour = startTime.substring(0, 2).toInt();
    activeWindow.startMinute = startTime.substring(3, 5).toInt();
    activeWindow.endHour = endTime.substring(0, 2).toInt();
    activeWindow.endMinute = endTime.substring(3, 5).toInt();
  }

  if (request.indexOf("/ntp") != -1) {
    ntpMode = true;
    systemActive = true;
  }
  if (request.indexOf("/manual") != -1) {
    ntpMode = false;
    checkTimeWindow();
  }

  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: text/html"));
  client.println(F("Connection: close"));
  client.println();

  client.println(F("<!DOCTYPE html><html><head><title>Relay Controller</title>"));
  client.println(F("<meta name='viewport' content='width=device-width, initial-scale=1'>"));
  client.println(F("<style>"));
  client.println(F("body {margin:0;font-family:'Segoe UI',sans-serif;background:#f3f4f6;color:#333;}"));
  client.println(F(".sidebar {width:140px;background:#3a3f51;position:fixed;top:0;bottom:0;padding:20px;color:#fff;}"));
  client.println(F(".sidebar button {background:#5867dd;border:none;color:#fff;padding:12px;width:100%;margin:10px 0;border-radius:20px;cursor:pointer;font-weight:bold;}"));
  client.println(F(".content {margin-left:160px;padding:20px;}"));
  client.println(F(".hidden {display:none;}"));
  client.println(F(".section {background:white;padding:20px;margin-top:20px;border-radius:10px;box-shadow:0 4px 8px rgba(0,0,0,0.1);}"));
  client.println(F(".relay {margin-bottom:15px;padding:15px;border-radius:8px;background:#eef;}"));
  client.println(F(".relay h3 {margin:0 0 10px 0;}"));
  client.println(F(".on {color:green;} .off {color:red;}"));
  client.println(F("button.toggle {padding:6px 16px;margin:0 4px;border:none;border-radius:20px;font-weight:bold;}"));
  client.println(F("</style>"));

  client.println(F("<script>function show(id){var s=document.getElementsByClassName('section');for(let i=0;i<s.length;i++)s[i].style.display='none';document.getElementById(id).style.display='block';}"));
  client.println(F("function toggleTimeMode(mode){document.getElementById('manual-time').style.display = mode === 'manual' ? 'block' : 'none';window.location.href='/' + mode;}"));
  client.println(F("</script>"));

  client.println(F("</head><body>"));
  client.println(F("<div class='sidebar'>"));
  client.println(F("<button onclick=\"show('status')\">STATUS</button>"));
  client.println(F("<button onclick=\"show('settings')\">SETTINGS</button>"));
  client.println(F("<button onclick=\"show('time')\">TIME</button>"));
  client.println(F("<button onclick=\"show('network')\">NETWORK</button>"));
  client.println(F("</div>"));

  client.println(F("<div class='content'>"));

  // STATUS
  client.println(F("<div class='section' id='status'>"));
  client.print(F("<h2>System Status: "));
  client.print(systemActive ? F("<span class='on'>ACTIVE</span>") : F("<span class='off'>INACTIVE</span>"));
  client.println(F("</h2>"));
  for (int i = 0; i < 4; i++) {
    client.print(F("<p><strong>Relay "));
    client.print(i + 1);
    client.print(F(":</strong> "));
    client.print(relayStates[i] ? F("<span class='on'>ON</span>") : F("<span class='off'>OFF</span>"));
    client.println(F("</p>"));
  }
  client.println(F("<p><strong>Temperature:</strong> 12°C<br><strong>Humidity:</strong> 20%</p>"));
  client.println(F("</div>"));

  // SETTINGS
  client.println(F("<div class='section hidden' id='settings'>"));
  client.println(F("<h2>Relay Controls</h2>"));
  for (int i = 0; i < 4; i++) {
    client.print(F("<div class='relay'><h3>Relay "));
    client.print(i + 1);
    client.print(F(": <span class='"));
    client.print(relayStates[i] ? F("on'>ON") : F("off'>OFF"));
    client.println(F("</span></h3>"));
    client.print(F("<a href='/relay"));
    client.print(i + 1);
    client.println(F("/on'><button class='toggle' style='background:#0c9;color:#fff;'>ON</button></a>"));
    client.print(F("<a href='/relay"));
    client.print(i + 1);
    client.println(F("/off'><button class='toggle' style='background:#e44;color:#fff;'>OFF</button></a></div>"));
  }
  client.println(F("</div>"));

  // TIME
  client.println(F("<div class='section hidden' id='time'>"));
  client.println(F("<h2>Time Mode</h2>"));
  client.println(F("<button class='toggle' style='background:#0c9;color:white' onclick=\"toggleTimeMode('ntp')\">NTP Mode</button>"));
  client.println(F("<button class='toggle' style='background:#5867dd;color:white' onclick=\"toggleTimeMode('manual')\">Manual Mode</button>"));
  client.println(F("<div id='manual-time' style='margin-top:20px; display:"));
  client.print(ntpMode ? F("none") : F("block"));
  client.println(F(";'>"));
  client.println(F("<form action='/settime' method='get'>"));
  client.print(F("Start: <input type='time' name='start' value='"));
  client.print(formatTime(activeWindow.startHour, activeWindow.startMinute));
  client.println(F("'> "));
  client.print(F("End: <input type='time' name='end' value='"));
  client.print(formatTime(activeWindow.endHour, activeWindow.endMinute));
  client.println(F("'> <button type='submit'>Save</button>"));
  client.println(F("</form></div></div>"));

  // NETWORK
  client.println(F("<div class='section hidden' id='network'>"));
  client.println(F("<h2>Network Configuration</h2><p>Feature coming soon...</p>"));
  client.println(F("</div>"));

  client.println(F("</div></body></html>"));
}

String formatTime(uint8_t hours, uint8_t minutes) {
  String timeStr = "";
  if (hours < 10) timeStr += "0";
  timeStr += hours;
  timeStr += ":";
  if (minutes < 10) timeStr += "0";
  timeStr += minutes;
  return timeStr;
}
