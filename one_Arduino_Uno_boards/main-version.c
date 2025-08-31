// === Includes & Definitions ===
#include <SPI.h>
#include <Ethernet2.h>

// Relay pins
#define RELAY1_PIN 5
#define RELAY2_PIN 6
#define RELAY3_PIN 7
#define RELAY4_PIN 8

// Status LED
#define STATUS_LED 13

// Default Network Settings
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(172, 16, 254, 250);
IPAddress gateway(172, 16, 254, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dnsServer(8, 8, 8, 8);

EthernetServer server(80);

// Time settings
struct TimeWindow {
  uint8_t startHour = 8;
  uint8_t startMinute = 0;
  uint8_t endHour = 16;
  uint8_t endMinute = 0;
};
TimeWindow activeWindow;

// Relay advanced settings
struct RelaySettings {
  bool state = false;
  String mode = "basic"; // basic, time, api, temp
  TimeWindow timeSettings;
  String apiEndpoint = "";
  float tempMin = 20.0;
  float tempMax = 30.0;
  float humidityMin = 30.0;
  float humidityMax = 70.0;
};
RelaySettings relaySettings[4];

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

  // Initialize relay settings
  relaySettings[0].mode = "time";
  relaySettings[1].mode = "api";
  relaySettings[2].mode = "temp";
  relaySettings[3].mode = "basic";

  Ethernet.begin(mac, ip, dnsServer, gateway, subnet);
  delay(1000);
  server.begin();
  Serial.print(F("Started at: "));
  Serial.println(Ethernet.localIP());
}

void loop() {
  if (!ntpMode && millis() - lastTimeCheck >= 1000) {
    lastTimeCheck = millis();
    currentSeconds++;
    if (currentSeconds >= 86400) currentSeconds = 0;
    checkTimeWindow();
  }

  // Check relay conditions
  checkRelayConditions();

  EthernetClient client = server.available();
  if (client) {
    handleWebRequest(client);
    client.stop();
  }

  // Control relays
  digitalWrite(RELAY1_PIN, systemActive && relaySettings[0].state ? HIGH : LOW);
  digitalWrite(RELAY2_PIN, systemActive && relaySettings[1].state ? HIGH : LOW);
  digitalWrite(RELAY3_PIN, systemActive && relaySettings[2].state ? HIGH : LOW);
  digitalWrite(RELAY4_PIN, systemActive && relaySettings[3].state ? HIGH : LOW);

  digitalWrite(STATUS_LED, systemActive ? HIGH : LOW);
  delay(100);
}
void checkRelayConditions() {
  for (int i = 0; i < 4; i++) {
    if (relaySettings[i].mode == "time") {
      unsigned long start = relaySettings[i].timeSettings.startHour * 3600UL + relaySettings[i].timeSettings.startMinute * 60UL;
      unsigned long end = relaySettings[i].timeSettings.endHour * 3600UL + relaySettings[i].timeSettings.endMinute * 60UL;
      if (relaySettings[i].timeSettings.endHour < relaySettings[i].timeSettings.startHour) {
        relaySettings[i].state = (currentSeconds >= start) || (currentSeconds < end);
      } else {
        relaySettings[i].state = (currentSeconds >= start) && (currentSeconds < end);
      }
    }
    // Note: API and temperature conditions would be checked here when implemented
    // For now, they remain manual control
  }
}

void checkTimeWindow() {
  unsigned long start = activeWindow.startHour * 3600UL + activeWindow.startMinute * 60UL;
  unsigned long end = activeWindow.endHour * 3600UL + activeWindow.endMinute * 60UL;
  if (activeWindow.endHour < activeWindow.startHour)
    systemActive = (currentSeconds >= start) || (currentSeconds < end);
  else
    systemActive = (currentSeconds >= start) && (currentSeconds < end);
}

// === Main Web Request Handler ===
void handleWebRequest(EthernetClient client) {
  String req = client.readStringUntil('\r'); client.flush();

  // Relay controls
  for (int i = 0; i < 4; i++) {
    if (req.indexOf("/relay" + String(i+1) + "/on") != -1) relaySettings[i].state = true;
    if (req.indexOf("/relay" + String(i+1) + "/off") != -1) relaySettings[i].state = false;
    
    // Relay mode setting
    if (req.indexOf("/relay" + String(i+1) + "/mode/") != -1) {
      int modeStart = req.indexOf("/mode/") + 6;
      String mode = req.substring(modeStart, req.indexOf(" ", modeStart));
      relaySettings[i].mode = mode;
    }

    // Time settings for relay 1
    if (i == 0 && req.indexOf("GET /relay1/settime?") != -1) {
      int s = req.indexOf("start=") + 6;
      int e = req.indexOf("&end=");
      String st = req.substring(s, e), en = req.substring(e + 5, req.indexOf(" ", e));
      relaySettings[0].timeSettings.startHour = st.substring(0, 2).toInt();
      relaySettings[0].timeSettings.startMinute = st.substring(3, 5).toInt();
      relaySettings[0].timeSettings.endHour = en.substring(0, 2).toInt();
      relaySettings[0].timeSettings.endMinute = en.substring(3, 5).toInt();
    }

    // API settings for relay 2
    if (i == 1 && req.indexOf("GET /relay2/setapi?") != -1) {
      int apiStart = req.indexOf("endpoint=") + 9;
      String endpoint = req.substring(apiStart, req.indexOf(" ", apiStart));
      endpoint.replace("%2F", "/");
      endpoint.replace("%3A", ":");
      relaySettings[1].apiEndpoint = endpoint;
    }

    // Temperature settings for relay 3
    if (i == 2 && req.indexOf("GET /relay3/settemp?") != -1) {
      int tempMinStart = req.indexOf("tempMin=") + 8;
      int tempMaxStart = req.indexOf("&tempMax=") + 9;
      int humMinStart = req.indexOf("&humMin=") + 8;
      int humMaxStart = req.indexOf("&humMax=") + 8;

      relaySettings[2].tempMin = req.substring(tempMinStart, tempMaxStart - 9).toFloat();
      relaySettings[2].tempMax = req.substring(tempMaxStart, humMinStart - 8).toFloat();
      relaySettings[2].humidityMin = req.substring(humMinStart, humMaxStart - 8).toFloat();
      relaySettings[2].humidityMax = req.substring(humMaxStart, req.indexOf(" ", humMaxStart)).toFloat();
    }
  }

  // Time setting
  if (req.indexOf("GET /settime?") != -1) {
    int s = req.indexOf("start=") + 6;
    int e = req.indexOf("&end=");
    String st = req.substring(s, e), en = req.substring(e + 5, req.indexOf(" ", e));
    activeWindow.startHour = st.substring(0, 2).toInt();
    activeWindow.startMinute = st.substring(3, 5).toInt();
    activeWindow.endHour = en.substring(0, 2).toInt();
    activeWindow.endMinute = en.substring(3, 5).toInt();
  }

  if (req.indexOf("/ntp") != -1) { ntpMode = true; systemActive = true; }
  if (req.indexOf("/manual") != -1) { ntpMode = false; checkTimeWindow(); }

  // Network config handler
  if (req.indexOf("GET /setnetwork?") != -1) {
    auto parseIP = [](String part) {
      int p[4]; sscanf(part.c_str(), "%d.%d.%d.%d", &p[0], &p[1], &p[2], &p[3]);
      return IPAddress(p[0], p[1], p[2], p[3]);
    };

    int ipStart = req.indexOf("ip=") + 3;
    int snStart = req.indexOf("subnet=") + 7;
    int gwStart = req.indexOf("gateway=") + 8;
    int dnsStart = req.indexOf("dns=") + 4;

    IPAddress newIP = parseIP(req.substring(ipStart, req.indexOf("&subnet")));
    IPAddress newSubnet = parseIP(req.substring(snStart, req.indexOf("&gateway")));
    IPAddress newGW = parseIP(req.substring(gwStart, req.indexOf("&dns")));
    IPAddress newDNS = parseIP(req.substring(dnsStart, req.indexOf(" ", dnsStart)));
    dnsServer = newDNS;

    Ethernet.begin(mac, newIP, dnsServer, newGW, newSubnet);
  }

  // === Web UI ===
  client.println(F("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n"));
  client.println(F("<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>"));
  client.println(F("<title>Arman Relay Control</title><style>"));
  client.println(F("body {margin:0;font-family:'Segoe UI',sans-serif;background:#f3f4f6;}"));
  client.println(F(".sidebar {width:160px;background:#3a3f51;position:fixed;top:0;bottom:0;padding:20px;color:white;}"));
  client.println(F(".sidebar button {background:#5867dd;color:#fff;border:none;padding:12px;margin:8px 0;width:100%;border-radius:20px;cursor:pointer;font-weight:bold;transition:0.3s;}"));
  client.println(F(".sidebar button:hover {background:#4854c1;}"));
  client.println(F(".submenu {display:none; padding-left:0;}"));
  client.println(F(".setting-btn:hover + .submenu, .submenu:hover {display:block;}"));
  client.println(F(".submenu button {background:#7f8ff6;margin-top:4px;width:100%;}"));
  client.println(F(".submenu button:hover {background:#6d7de0;}"));
  client.println(F(".content {margin-left:180px;padding:20px;margin-top:60px;} .hidden {display:none;}"));
  client.println(F(".section {background:white;padding:20px;margin-top:20px;border-radius:10px;box-shadow:0 4px 8px rgba(0,0,0,0.1);}"));
  client.println(F(".on {color:green;} .off {color:red;}"));
  client.println(F("</style></head><body>"));

  // ✅ Fixed status header
  client.println(F("<div style='background:#3a3f51;color:white;padding:10px 20px;position:fixed;width:100%;top:0;left:0;z-index:1000;'>"));
  client.print(F("<strong>Status:</strong> "));
  client.print(systemActive ? F("<span style='color:#0f0;'>ACTIVE</span>") : F("<span style='color:#f00;'>INACTIVE</span>"));
  client.print(F(" | <strong>Time Mode:</strong> "));
  client.print(ntpMode ? F("NTP") : F("Manual"));
  client.print(F(" | <strong>Active Time:</strong> "));
  client.print(formatTime(activeWindow.startHour, activeWindow.startMinute));
  client.print(" - ");
  client.print(formatTime(activeWindow.endHour, activeWindow.endMinute));
  for (int i = 0; i < 4; i++) {
    client.print(F(" | R"));
    client.print(i+1);
    client.print(F(": "));
    client.print(relaySettings[i].state ? "ON" : "OFF");
  }
  client.println(F("</div>"));

  // === Sidebar ===
  client.println(F("<div class='sidebar'>"));
  // STATUS button removed here as requested
  client.println(F("<button class='setting-btn'>SETTING</button>"));
  client.println(F("<div class='submenu'>"));
  client.println(F("<button onclick=\"show('time')\">TIME</button>"));
  client.println(F("<button onclick=\"show('snmp')\">SNMP</button>"));
  client.println(F("<button onclick=\"show('network')\">NETWORK</button>"));
  client.println(F("</div>"));
  client.println(F("<button onclick=\"show('relay')\">RELAY SETTING</button>"));
  client.println(F("<button onclick=\"location.href='/logout'\">LOGOUT</button>"));
  client.println(F("</div><div class='content'>"));

  // === RELAY SETTING ===
  // (Keep all your original relay UI blocks here as-is — unchanged)
  // Including tabs for Relay 1 (time), Relay 2 (API), Relay 3 (temp/humidity), Relay 4 (basic)

  // === TIME SETTINGS ===
  // (Also unchanged — manual/ntp mode with form)

  // === SNMP Section ===
  client.println(F("<div class='section hidden' id='snmp'><h2>SNMP</h2><p>Coming soon...</p></div>"));

  // === NETWORK SETTINGS ===
  client.println(F("<div class='section hidden' id='network'><h2>Network Setup</h2>"));
  client.println(F("<form method='get' action='/setnetwork'>"));

  client.println(F("<div class='form-group'>"));
  client.print(F("<label>IP Address</label><input name='ip' value='"));
  client.print(Ethernet.localIP());
  client.println(F("'>"));
  client.println(F("</div>"));

  client.println(F("<div class='form-group'>"));
  client.print(F("<label>Subnet Mask</label><input name='subnet' value='"));
  client.print(Ethernet.subnetMask());
  client.println(F("'>"));
  client.println(F("</div>"));

  client.println(F("<div class='form-group'>"));
  client.print(F("<label>Gateway</label><input name='gateway' value='"));
  client.print(Ethernet.gatewayIP());
  client.println(F("'>"));
  client.println(F("</div>"));

  client.println(F("<div class='form-group'>"));
  client.print(F("<label>DNS Server</label><input name='dns' value='"));
  client.print(dnsServer);
  client.println(F("'>"));
  client.println(F("</div>"));

  client.println(F("<button type='submit' class='btn'>Save Network Settings</button>"));
  client.println(F("</form></div>"));

  client.println(F("</div></body></html>"));
}

// === Format Time Helper ===
String formatTime(uint8_t h, uint8_t m) {
  String s = (h < 10 ? "0" : "") + String(h) + ":";
  s += (m < 10 ? "0" : "") + String(m);
  return s;
}
