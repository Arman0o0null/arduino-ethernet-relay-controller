#include <SPI.h>
#include <Ethernet.h>
#include <avr/pgmspace.h>
#include <ICMPPing.h>
#include <utility/w5100.h>

// Ethernet Configuration (Default)
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x30 };
IPAddress ip(192, 168, 1, 30);
IPAddress subnet(255, 255, 255, 0);
IPAddress gateway(192, 168, 1, 1);
IPAddress target(192, 168, 1, 31);

EthernetServer server(80);

// Relay
const int relayPin = 7;
bool relayState = false;

// Login
const char* username = "admin";
const char* password = "1234";

// Ping
SOCKET pingSocket = 0;
ICMPPing ping(pingSocket, (uint16_t)random(0, 255));
unsigned long lastPingTime = 0;
const unsigned long pingInterval = 300000; // 5 minutes

// Login Page
const char loginPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html>
<head><title>Login</title><style>
body { font-family: Arial; background: #f2f2f2; text-align: center; padding-top: 50px; }
form { background: white; padding: 20px; display: inline-block; border-radius: 10px; box-shadow: 0 0 10px gray; }
input[type=text], input[type=password] {
  padding: 10px; margin: 10px; width: 200px; border-radius: 5px; border: 1px solid #ccc;
}
input[type=submit] {
  padding: 10px 30px; background: #007BFF; color: white; border: none;
  border-radius: 5px; cursor: pointer;
}
input[type=submit]:hover { background-color: #0056b3; }
</style></head><body>
<h2>Device Login</h2>
<form action="/login">
Username:<br><input name="user" type="text"><br>
Password:<br><input name="pass" type="password"><br>
<input type="submit" value="Login">
</form></body></html>
)rawliteral";

// Control Page
const char controlPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html>
<head><title>Control Panel</title><style>
body { font-family: Arial; background: #e9ecef; text-align: center; padding-top: 30px; }
.container { background: white; padding: 20px; display: inline-block; border-radius: 10px; box-shadow: 0 0 10px gray; }
h2 { margin-bottom: 10px; }
button, input[type=submit] {
  padding: 10px 30px; margin: 10px; border: none; border-radius: 5px; cursor: pointer;
}
.on { background-color: #28a745; color: white; }
.on:hover { background-color: #218838; }
.off { background-color: #dc3545; color: white; }
.off:hover { background-color: #c82333; }
input[type=text] {
  padding: 8px; margin: 5px; width: 180px; border-radius: 5px; border: 1px solid #ccc;
}
.section { margin-top: 30px; }
</style></head><body>
<div class="container">
  <div class="section">
    <h2>Relay Control</h2>
    <p>Status: %STATE%</p>
    <a href="/on"><button class="on">Turn ON</button></a>
    <a href="/off"><button class="off">Turn OFF</button></a>
  </div>
  <div class="section">
    <h2>Ethernet Settings</h2>
    <form action="/netconfig">
    IP Address:<br><input name="ip" type="text"><br>
    Subnet Mask:<br><input name="subnet" type="text"><br>
    Gateway:<br><input name="gateway" type="text"><br>
    Ping Target IP:<br><input name="target" type="text"><br>
    <input type="submit" value="Save Settings">
    </form>
  </div>
</div></body></html>
)rawliteral";

void setup() {
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);
  Ethernet.begin(mac, ip, gateway, gateway, subnet);
  server.begin();

  Serial.begin(9600);
  randomSeed(analogRead(0));
  Serial.println("Web server started at http://192.168.1.30");
}

void loop() {
  EthernetClient client = server.available();
  if (client) {
    String request = "";
    boolean currentLineIsBlank = true;

    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        request += c;

        if (c == '\n' && currentLineIsBlank) {
          if (request.indexOf("GET /login?user=admin&pass=1234") >= 0) {
            sendControlPage(client);
          } else if (request.indexOf("GET /on") >= 0) {
            relayState = true;
            digitalWrite(relayPin, HIGH);
            sendControlPage(client);
          } else if (request.indexOf("GET /off") >= 0) {
            relayState = false;
            digitalWrite(relayPin, LOW);
            sendControlPage(client);
          } else if (request.indexOf("GET /netconfig?") >= 0) {
            sendConfigSuccess(client);
          } else {
            sendLoginPage(client);
          }
          break;
        }
        if (c == '\n') currentLineIsBlank = true;
        else if (c != '\r') currentLineIsBlank = false;
      }
    }
    delay(1);
    client.stop();
  }

  // Ping every 5 minutes
  if (millis() - lastPingTime > pingInterval) {
    lastPingTime = millis();
    ICMPEchoReply echoReply = ping(target, 4);
    if (echoReply.status != SUCCESS) {
      Serial.println("Ping failed. Turning off relay!");
      relayState = false;
      digitalWrite(relayPin, LOW);
    } else {
      Serial.println("Ping OK!");
    }
  }
}

void sendLoginPage(EthernetClient& client) {
  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: text/html"));
  client.println();
  for (uint16_t i = 0; i < sizeof(loginPage) - 1; i++) {
    client.write(pgm_read_byte_near(loginPage + i));
  }
}

void sendControlPage(EthernetClient& client) {
  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: text/html"));
  client.println();
  const char* ptr = controlPage;
  bool stateInserted = false;
  for (uint16_t i = 0; i < sizeof(controlPage) - 1; i++) {
    char c = pgm_read_byte_near(ptr + i);
    if (!stateInserted && c == '%' &&
        pgm_read_byte_near(ptr + i + 1) == 'S' &&
        pgm_read_byte_near(ptr + i + 2) == 'T' &&
        pgm_read_byte_near(ptr + i + 3) == 'A' &&
        pgm_read_byte_near(ptr + i + 4) == 'T' &&
        pgm_read_byte_near(ptr + i + 5) == 'E' &&
        pgm_read_byte_near(ptr + i + 6) == '%') {
      client.print(relayState ? "ON" : "OFF");
      i += 6;
      stateInserted = true;
    } else {
      client.write(c);
    }
  }
}

void sendConfigSuccess(EthernetClient& client) {
  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: text/html"));
  client.println();
  client.println(F("<html><body><h2>Settings Saved (not stored yet)</h2><a href='/login?user=admin&pass=1234'>Back</a></body></html>"));
}
