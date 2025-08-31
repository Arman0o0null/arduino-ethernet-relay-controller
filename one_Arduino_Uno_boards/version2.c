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
bool systemActive = false;
unsigned long lastTimeCheck = 0;
unsigned long currentSeconds = 0;

void setup() {
  Serial.begin(9600);

  // Initialize pins
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(RELAY3_PIN, OUTPUT);
  pinMode(RELAY4_PIN, OUTPUT);
  pinMode(STATUS_LED, OUTPUT);

  // Initialize Ethernet
  Ethernet.begin(mac, ip, gateway, subnet);
  server.begin();

  Serial.print("Relay Controller started at ");
  Serial.println(Ethernet.localIP());
}

void loop() {
  // Update time tracking
  if (millis() - lastTimeCheck >= 1000) {
    lastTimeCheck = millis();
    currentSeconds++;
    if (currentSeconds >= 86400) currentSeconds = 0; // Reset after 24 hours

    // Check time window
    checkTimeWindow();
  }

  // Handle web requests
  EthernetClient client = server.available();
  if (client) {
    handleWebRequest(client);
    client.stop();
  }

  // Update relay outputs
  digitalWrite(RELAY1_PIN, systemActive && relayStates[0] ? HIGH : LOW);
  digitalWrite(RELAY2_PIN, systemActive && relayStates[1] ? HIGH : LOW);
  digitalWrite(RELAY3_PIN, systemActive && relayStates[2] ? HIGH : LOW);
  digitalWrite(RELAY4_PIN, systemActive && relayStates[3] ? HIGH : LOW);

  delay(100);
}

void checkTimeWindow() {
  unsigned long startTime = activeWindow.startHour * 3600UL + activeWindow.startMinute * 60UL;
  unsigned long endTime = activeWindow.endHour * 3600UL + activeWindow.endMinute * 60UL;

  // Handle overnight windows
  if (activeWindow.endHour < activeWindow.startHour) {
    systemActive = (currentSeconds >= startTime) || (currentSeconds < endTime);
  } else {
    systemActive = (currentSeconds >= startTime) && (currentSeconds < endTime);
  }

  digitalWrite(STATUS_LED, systemActive ? HIGH : LOW);
}

void handleWebRequest(EthernetClient client) {
  String request = client.readStringUntil('\r');
  client.flush();

  // Process relay commands
  if (request.indexOf("GET /relay1/on") != -1) relayStates[0] = true;
  if (request.indexOf("GET /relay1/off") != -1) relayStates[0] = false;
  if (request.indexOf("GET /relay2/on") != -1) relayStates[1] = true;
  if (request.indexOf("GET /relay2/off") != -1) relayStates[1] = false;
  if (request.indexOf("GET /relay3/on") != -1) relayStates[2] = true;
  if (request.indexOf("GET /relay3/off") != -1) relayStates[2] = false;
  if (request.indexOf("GET /relay4/on") != -1) relayStates[3] = true;
  if (request.indexOf("GET /relay4/off") != -1) relayStates[3] = false;

  // Process time setting
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

  // Send web page
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();

  client.println("<!DOCTYPE html>");
  client.println("<html><head><title>Relay Controller</title>");
  client.println("<meta name='viewport' content='width=device-width, initial-scale=1'>");
  client.println("<style>");
  client.println("body {margin: 0; font-family: Arial, sans-serif;}");
  client.println(".menu {background: #44c; width: 120px; padding: 10px; height: 100vh; float: left; color: white;}");
  client.println(".menu button {display: block; width: 100%; margin: 10px 0; padding: 10px; background: #c5a; color: white; border: none; border-radius: 10px; font-weight: bold;}");
  client.println(".header {background: #aee; padding: 10px; display: flex; justify-content: space-between; align-items: center;}");
  client.println(".relay-status span {margin: 0 10px; font-weight: bold;}");
  client.println(".on {color: green;} .off {color: red;}");
  client.println(".status-info {text-align: right; font-weight: bold;}");
  client.println(".status-info span {color: blue;}");
  client.println(".content {margin-left: 130px; padding: 20px; background: #cecaf2; min-height: 100vh;}");
  client.println(".relay {margin: 10px; padding: 10px; border: 1px solid #aaa; border-radius: 5px; background: #fff;}");
  client.println("button {padding: 5px 15px; margin: 0 5px;}");
  client.println("input[type='time'] {padding: 5px; margin: 5px; width: 100px;}");
  client.println("</style></head><body>");

  client.println("<div class='menu'>");
  client.println("<button>TIME</button>");
  client.println("<button>SNMP</button>");
  client.println("<button>NETWORK</button>");
  client.println("</div>");

  client.println("<div class='header'>");
  client.println("<div class='relay-status'>");
  client.print("R1 : <span class='"); client.print(relayStates[0] ? "on'>ON" : "off'>OFF"); client.println("</span>");
  client.print("R2 : <span class='"); client.print(relayStates[1] ? "on'>ON" : "off'>OFF"); client.println("</span>");
  client.print("R3 : <span class='"); client.print(relayStates[2] ? "on'>ON" : "off'>OFF"); client.println("</span>");
  client.print("R4 : <span class='"); client.print(relayStates[3] ? "on'>ON" : "off'>OFF"); client.println("</span>");
  client.println("</div>");

  client.println("<div class='status-info'>");
  client.println("TEMPERATURE : <span>12°C</span><br>");
  client.println("HUMIDITY : <span>20%</span>");
  client.println("</div>");
  client.println("</div>");

  client.println("<div class='content'>");

  client.print("<h2>System Status: ");
  client.print(systemActive ? "<span style='color:green'>ACTIVE</span>" : "<span style='color:red'>INACTIVE</span>");
  client.println("</h2>");

  client.print("<p>Active Time: ");
  client.print(formatTime(activeWindow.startHour, activeWindow.startMinute));
  client.print(" to ");
  client.print(formatTime(activeWindow.endHour, activeWindow.endMinute));
  client.println("</p>");

  client.println("<h2>Set Active Time</h2>");
  client.println("<form action='/settime' method='get'>");
  client.println("Start: <input type='time' name='start' value='");
  client.print(formatTime(activeWindow.startHour, activeWindow.startMinute));
  client.println("'>");
  client.println("End: <input type='time' name='end' value='");
  client.print(formatTime(activeWindow.endHour, activeWindow.endMinute));
  client.println("'>");
  client.println("<button type='submit'>Save</button>");
  client.println("</form>");

  client.println("<h2>Relay Control</h2>");
  for (int i = 0; i < 4; i++) {
    client.print("<div class='relay ");
    client.print(relayStates[i] ? "on" : "off");
    client.println("'>");
    client.print("<h3>Relay ");
    client.print(i + 1);
    client.print(": ");
    client.print(relayStates[i] ? "ON" : "OFF");
    client.println("</h3>");
    client.print("<a href='/relay");
    client.print(i + 1);
    client.print("/on'><button>ON</button></a> ");
    client.print("<a href='/relay");
    client.print(i + 1);
    client.print("/off'><button>OFF</button></a>");
    client.println("</div>");
  }

  client.println("</div></body></html>");
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
