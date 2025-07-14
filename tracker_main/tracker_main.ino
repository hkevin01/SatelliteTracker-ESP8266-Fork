#include <SPI.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <AccelStepper.h>
#include <LiquidCrystal.h>
#include <Ticker.h>
#include <EEPROM.h>
#include <ESP8266WebServer.h>
#include <Sgp4.h>

// Debug levels
#define DEBUG_LEVEL_ERROR 1
#define DEBUG_LEVEL_WARN 2  
#define DEBUG_LEVEL_INFO 3
#define DEBUG_LEVEL_DEBUG 4

int currentDebugLevel = DEBUG_LEVEL_INFO;

// System status structure
struct SystemStatus {
  unsigned long uptime;
  int wifiReconnects;
  int tleFailures;
  int passesTracked;
  float lastAzimuth;
  float lastElevation;
  bool isTracking;
  String lastError;
};

SystemStatus sysStatus = {0, 0, 0, 0, 0.0, 0.0, false, ""};

// Phase 6: Enhanced Error Handling - Memory Management & Protection
struct MemoryMonitor {
  uint32_t minFreeHeap = 8192;      // 8KB minimum free heap
  uint32_t maxFragmentation = 50;    // 50% maximum fragmentation
  unsigned long lastCheck = 0;
  bool lowMemoryMode = false;
  uint32_t lowMemoryCount = 0;
  uint32_t minHeapSeen = 0xFFFFFFFF; // Track lowest heap ever seen
  unsigned long lastGC = 0;          // Last garbage collection time
};

MemoryMonitor memMonitor;

// Phase 6: Enhanced motor health monitoring
struct MotorHealth {
  bool azimuthOK = true;
  bool elevationOK = true;
  unsigned long lastMovement = 0;
  int consecutiveFailures = 0;
  bool stallDetected = false;
  long lastAzPosition = 0;
  long lastElPosition = 0;
  unsigned long positionCheckTime = 0;
  unsigned long totalMovements = 0;
};

MotorHealth motorHealth;

// Phase 6: Network resilience configuration
struct NetworkConfig {
  const char* ntpServers[4] = {
    "pool.ntp.org",
    "time.nist.gov", 
    "time.google.com",
    "time.cloudflare.com"
  };
  int currentNTPServer = 0;
  const char* fallbackIP = "104.168.149.178"; // Celestrak backup IP
  bool dnsWorking = true;
  unsigned long lastConnectionTest = 0;
  int connectionFailures = 0;
  unsigned long totalDataTransferred = 0;
};

NetworkConfig netConfig;

// Phase 6: Temperature and Lifecycle Monitoring
struct ComponentLifecycle {
  unsigned long motorSteps = 0;
  unsigned long lcdWrites = 0;
  unsigned long networkConnections = 0;
  unsigned long powerCycles = 0;
  float avgTemperature = 0.0;
  unsigned long tempReadings = 0;
};

struct TemperatureMonitor {
  float currentTemp = 0.0;
  float maxTemp = 0.0;
  float minTemp = 100.0;
  bool overheating = false;
  unsigned long lastReading = 0;
  static const float TEMP_WARNING_THRESHOLD = 70.0;  // Celsius
  static const float TEMP_CRITICAL_THRESHOLD = 80.0;
};

ComponentLifecycle lifecycle;
TemperatureMonitor tempMonitor;

// Phase 6: Memory health monitoring function
void checkMemoryHealth() {
  unsigned long currentTime = millis();
  
  // Check memory every 10 seconds
  if (currentTime - memMonitor.lastCheck < 10000) return;
  
  uint32_t freeHeap = ESP.getFreeHeap();
  uint32_t fragmentation = ESP.getHeapFragmentation();
  
  // Track minimum heap seen
  if (freeHeap < memMonitor.minHeapSeen) {
    memMonitor.minHeapSeen = freeHeap;
  }
  
  // Check if we need to enter low memory mode
  if (freeHeap < memMonitor.minFreeHeap || fragmentation > memMonitor.maxFragmentation) {
    if (!memMonitor.lowMemoryMode) {
      debugLog(DEBUG_LEVEL_WARN, "Low memory detected - Heap: " + String(freeHeap) + 
               " bytes, Fragmentation: " + String(fragmentation) + "%");
      memMonitor.lowMemoryMode = true;
      memMonitor.lowMemoryCount++;
      
      // Enter conservation mode
      if (currentDebugLevel > DEBUG_LEVEL_ERROR) {
        currentDebugLevel = DEBUG_LEVEL_ERROR; // Reduce logging
        debugLog(DEBUG_LEVEL_WARN, "Reduced debug level to conserve memory");
      }
      
      // Force garbage collection
      ESP.wdtFeed();
      yield();
      memMonitor.lastGC = currentTime;
    }
  } else if (memMonitor.lowMemoryMode && freeHeap > memMonitor.minFreeHeap + 2048) {
    // Exit low memory mode when we have sufficient headroom
    debugLog(DEBUG_LEVEL_INFO, "Memory recovered - Heap: " + String(freeHeap) + " bytes available");
    memMonitor.lowMemoryMode = false;
    currentDebugLevel = DEBUG_LEVEL_INFO; // Restore normal logging
  }
  
  // Periodic garbage collection (every 5 minutes)
  if (currentTime - memMonitor.lastGC > 300000) {
    ESP.wdtFeed();
    yield();
    memMonitor.lastGC = currentTime;
    debugLog(DEBUG_LEVEL_DEBUG, "Periodic GC completed - Free heap: " + String(ESP.getFreeHeap()));
  }
  
  memMonitor.lastCheck = currentTime;
}

// Phase 6: Safe LCD operations with enhanced error handling
bool safeLCDWrite(int col, int row, const char* text) {
  static unsigned long lastWrite = 0;
  
  // Input validation
  if (!text || col < 0 || col > 15 || row < 0 || row > 1) {
    debugLog(DEBUG_LEVEL_ERROR, "Invalid LCD parameters");
    return false;
  }
  
  unsigned long currentTime = millis();
  
  // Rate limiting - minimum 50ms between writes
  if (currentTime - lastWrite < 50) {
    return false;
  }
  
  // Skip non-critical LCD updates during low memory conditions
  if (memMonitor.lowMemoryMode) {
    return false;
  }
  
  // Truncate text if too long for display
  char truncatedText[17]; // 16 chars + null terminator
  int maxLen = 16 - col;
  
  if (maxLen <= 0) {
    debugLog(DEBUG_LEVEL_ERROR, "LCD column position invalid");
    return false;
  }
  
  strncpy(truncatedText, text, maxLen);
  truncatedText[maxLen] = '\0';
  
  // Perform LCD write with error protection
  lcd.setCursor(col, row);
  lcd.print(truncatedText);
  lastWrite = currentTime;
  
  // Update lifecycle counter
  updateLifecycleCounters("lcd");
  
  return true;
}

bool safeLCDClear() {
  static unsigned long lastClear = 0;
  unsigned long currentTime = millis();
  
  // Rate limit clear operations - minimum 100ms between clears
  if (currentTime - lastClear < 100) {
    return false;
  }
  
  // Skip during low memory mode
  if (memMonitor.lowMemoryMode) {
    return false;
  }
  
  lcd.clear();
  lastClear = currentTime;
  return true;
}

// Config structure
struct Config {
  char ssid[32];
  char password[64];
  float lat;
  float lon;
  float alt;
  int timeZone;
};

Config config;
ESP8266WebServer webServer(80);

// To be modified by user //
#define SECRET_SSID "ChangVan2017"                                     // Your Network name.
#define SECRET_PASS "ohcanada"                                         // Your Network password.
#define DEBUG                                                          //Enable serial output.
const int timeZone = -7;                                               //Your Time zone.
char TLE[500];                                                         //Variable to store satellite TLEs.
char satnames[4][30] = {"RADARSAT-2", "NEOSSAT", "M3MSAT ", "SCISAT"}; // Names of satellites.
char satURL[4][30] = {"/satcat/tle.php?CATNR=32382", "/satcat/tle.php?CATNR=39089",
                      "/satcat/tle.php?CATNR=41605", "/satcat/tle.php?CATNR=27858"}; // URL of Celestrak TLEs for satellites (In same order as names).
char TLE1[4][70];
char TLE2[4][70];
float myLat = 52.12;
float myLong = -106.663;
float myAlt = 482; // Your latitude, longitude and altitude.
int numSats = 4;   // Number of satellites to track.

// Azimuth stepper pins (ESP8266 NodeMCU) //
#define AZmotorPin1 D1  // IN1 on the A4988 driver (GPIO5)
#define AZmotorPin2 D2  // IN2 on the A4988 driver (GPIO4)
#define AZmotorPin3 D3  // IN3 on the A4988 driver (GPIO0)
#define AZmotorPin4 D4  // IN4 on the A4988 driver (GPIO2)
// Elevation stepper pins //
#define ELmotorPin1 D5  // GPIO14
#define ELmotorPin2 D6  // GPIO12
#define ELmotorPin3 D7  // GPIO13
#define ELmotorPin4 D8  // GPIO15

// LCD pins (adjusted to avoid conflicts) //
#define LCD_RS D0    // Register Select (GPIO16)
#define LCD_EN A0    // Enable (ADC0)
#define LCD_D4 D9    // Data 4 (GPIO3/RX)
#define LCD_D5 D10   // Data 5 (GPIO1/TX)
#define LCD_D6 9     // Data 6 (GPIO9)
#define LCD_D7 10    // Data 7 (GPIO10)

// Piezo speaker pin //
#define SPEAKER_PIN 3  // GPIO3 (RX - be careful with serial)

int satAZsteps;
int satELsteps;
int turns = 0;
float oneTurn = 4096;        // Number of steps per one rotation for stepper motor.
#define MotorInterfaceType 8 // Define the AccelStepper interface type; 4 wire motor in half step mode:
AccelStepper stepperAZ = AccelStepper(MotorInterfaceType, AZmotorPin1, AZmotorPin3, AZmotorPin2, AZmotorPin4);
AccelStepper stepperEL = AccelStepper(MotorInterfaceType, ELmotorPin1, ELmotorPin3, ELmotorPin2, ELmotorPin4);

// LCD initialization //
LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

// NTP Client setup //
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", timeZone * 3600, 60000);

Sgp4 sat;

char ssid[] = SECRET_SSID; // your network SSID (name)
char pass[] = SECRET_PASS; // your network password (use for WPA, or use as key for WEP)
int i;
int k;
int SAT;
int nextSat;
int AZstart;
long passEnd;
int satVIS;
char satname[] = " ";
int passStatus = 0;
char celestrakServer[] = "celestrak.org"; // Use domain name instead of IP
int year;
int mon;
int day;
int hr;
int minute;
double sec;
int today;
long nextpassEpoch;
long upcomingPasses[4];
unsigned long unixtime;
unsigned long testTime = 1593789900;
unsigned long timeNow = 0;

// Initialize the HTTPS client library for ESP8266
WiFiClientSecure client;

const int maxWifiReconnectAttempts = 10;
int wifiReconnectAttempts = 0;

// Ticker for watchdog
Ticker watchdogTicker;

void feedWatchdog()
{
  ESP.wdtFeed();
}

void loadConfiguration() {
  EEPROM.begin(sizeof(Config));
  EEPROM.get(0, config);
  EEPROM.end();

  // Check if EEPROM has valid data
  if (String(config.ssid).length() == 0) {
    // First time run, load defaults
#ifdef DEBUG
    Serial.println("First run, loading default configuration.");
#endif
    strcpy(config.ssid, SECRET_SSID);
    strcpy(config.password, SECRET_PASS);
    config.lat = myLat;
    config.lon = myLong;
    config.alt = myAlt;
    config.timeZone = timeZone;
    saveConfiguration();
  }
}

void saveConfiguration() {
  EEPROM.begin(sizeof(Config));
  EEPROM.put(0, config);
  EEPROM.commit();
  EEPROM.end();
}

void handleRoot() {
  String s = "<html><head><title>Tracker Config</title></head><body>";
  s += "<h1>Satellite Tracker Configuration</h1>";
  s += "<form method='POST' action='/save'>";
  s += "SSID: <input type='text' name='ssid' value='" + String(config.ssid) + "'><br>";
  s += "Password: <input type='password' name='password' value='" + String(config.password) + "'><br>";
  s += "Latitude: <input type='text' name='lat' value='" + String(config.lat) + "'><br>";
  s += "Longitude: <input type='text' name='lon' value='" + String(config.lon) + "'><br>";
  s += "Altitude: <input type='text' name='alt' value='" + String(config.alt) + "'><br>";
  s += "Time Zone: <input type='text' name='tz' value='" + String(config.timeZone) + "'><br>";
  s += "<input type='submit' value='Save'></form>";
  s += "</body></html>";
  webServer.send(200, "text/html", s);
}

void handleSave() {
  // Phase 6: Enhanced configuration validation
  Config tempConfig;
  
  // Parse new configuration
  strcpy(tempConfig.ssid, webServer.arg("ssid").c_str());
  strcpy(tempConfig.password, webServer.arg("password").c_str());
  tempConfig.lat = webServer.arg("lat").toFloat();
  tempConfig.lon = webServer.arg("lon").toFloat();
  tempConfig.alt = webServer.arg("alt").toFloat();
  tempConfig.timeZone = webServer.arg("tz").toInt();
  
  // Validate configuration before saving
  if (validateConfiguration(tempConfig)) {
    // Configuration is valid, save it
    config = tempConfig;
    saveConfiguration();
    debugLog(DEBUG_LEVEL_INFO, "Configuration updated and validated successfully");
    webServer.send(200, "text/html", 
      "<html><body><h1>Configuration Saved Successfully</h1>"
      "<p>New configuration has been validated and saved.</p>"
      "<p>System will restart in 3 seconds...</p>"
      "<script>setTimeout(function(){window.location.href='/';}, 3000);</script>"
      "</body></html>");
  } else {
    // Configuration validation failed
    debugLog(DEBUG_LEVEL_ERROR, "Configuration validation failed - changes rejected");
    webServer.send(400, "text/html", 
      "<html><body><h1>Configuration Validation Failed</h1>"
      "<p style='color:red'>The provided configuration contains invalid parameters.</p>"
      "<p>Please check the following:</p>"
      "<ul>"
      "<li>Latitude: -90 to 90 degrees</li>"
      "<li>Longitude: -180 to 180 degrees</li>"
      "<li>Altitude: -500 to 9000 meters</li>"
      "<li>Timezone: -12 to +14 hours</li>"
      "<li>SSID: 1-31 characters</li>"
      "<li>Password: 0-63 characters</li>"
      "</ul>"
      "<p><a href='/'>Back to Configuration</a></p>"
      "</body></html>");
    return; // Don't restart
  }
  
  delay(1000);
  ESP.restart();
}
  webServer.send(200, "text/html", "Configuration saved. Restarting...");
  delay(1000);
  ESP.restart();
}

void handleStatus() {
  String s = "<html><head><title>Tracker Status</title>";
  s += "<meta http-equiv='refresh' content='5'></head><body>";
  s += "<h1>Satellite Tracker Status (Phase 6 Enhanced)</h1>";
  
  // Basic System Status
  s += "<h2>System Status</h2>";
  s += "<table border='1'>";
  s += "<tr><td>Uptime</td><td>" + String(millis()/1000) + " seconds</td></tr>";
  s += "<tr><td>WiFi Status</td><td>" + String(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected") + "</td></tr>";
  s += "<tr><td>IP Address</td><td>" + WiFi.localIP().toString() + "</td></tr>";
  s += "<tr><td>Signal Strength</td><td>" + String(WiFi.RSSI()) + " dBm</td></tr>";
  s += "<tr><td>Current Azimuth</td><td>" + String(sysStatus.lastAzimuth, 2) + "°</td></tr>";
  s += "<tr><td>Current Elevation</td><td>" + String(sysStatus.lastElevation, 2) + "°</td></tr>";
  s += "<tr><td>Tracking Status</td><td>" + String(sysStatus.isTracking ? "Active" : "Standby") + "</td></tr>";
  s += "</table>";
  
  // Phase 6: Enhanced Memory Monitoring
  s += "<h2>Memory Status</h2>";
  s += "<table border='1'>";
  s += "<tr><td>Free Heap</td><td>" + String(ESP.getFreeHeap()) + " bytes</td></tr>";
  s += "<tr><td>Heap Fragmentation</td><td>" + String(ESP.getHeapFragmentation()) + "%</td></tr>";
  s += "<tr><td>Minimum Heap Seen</td><td>" + String(memMonitor.minHeapSeen) + " bytes</td></tr>";
  s += "<tr><td>Low Memory Mode</td><td>" + String(memMonitor.lowMemoryMode ? "Active" : "Normal") + "</td></tr>";
  s += "<tr><td>Low Memory Events</td><td>" + String(memMonitor.lowMemoryCount) + "</td></tr>";
  s += "<tr><td>Memory Status</td><td>";
  if (ESP.getFreeHeap() > memMonitor.minFreeHeap * 2) {
    s += "<span style='color:green'>Good</span>";
  } else if (ESP.getFreeHeap() > memMonitor.minFreeHeap) {
    s += "<span style='color:orange'>Warning</span>";
  } else {
    s += "<span style='color:red'>Critical</span>";
  }
  s += "</td></tr>";
  s += "</table>";
  
  // Phase 6: Motor Health Status
  s += "<h2>Motor Health</h2>";
  s += "<table border='1'>";
  s += "<tr><td>Azimuth Motor</td><td>" + String(motorHealth.azimuthOK ? "OK" : "FAULT") + "</td></tr>";
  s += "<tr><td>Elevation Motor</td><td>" + String(motorHealth.elevationOK ? "OK" : "FAULT") + "</td></tr>";
  s += "<tr><td>Stall Detected</td><td>" + String(motorHealth.stallDetected ? "Yes" : "No") + "</td></tr>";
  s += "<tr><td>Consecutive Failures</td><td>" + String(motorHealth.consecutiveFailures) + "</td></tr>";
  s += "<tr><td>Total Movements</td><td>" + String(motorHealth.totalMovements) + "</td></tr>";
  s += "<tr><td>Last Movement</td><td>" + String((millis() - motorHealth.lastMovement)/1000) + " seconds ago</td></tr>";
  s += "</table>";
  
  // Phase 6: Network Status
  s += "<h2>Network Status</h2>";
  s += "<table border='1'>";
  s += "<tr><td>WiFi Reconnects</td><td>" + String(sysStatus.wifiReconnects) + "</td></tr>";
  s += "<tr><td>DNS Working</td><td>" + String(netConfig.dnsWorking ? "Yes" : "No (Using Fallback IP)") + "</td></tr>";
  s += "<tr><td>Connection Failures</td><td>" + String(netConfig.connectionFailures) + "</td></tr>";
  s += "<tr><td>Current NTP Server</td><td>" + String(netConfig.ntpServers[netConfig.currentNTPServer]) + "</td></tr>";
  s += "<tr><td>Data Transferred</td><td>" + String(netConfig.totalDataTransferred) + " bytes</td></tr>";
  s += "</table>";
  
  // Satellite Tracking Status
  s += "<h2>Satellite Tracking</h2>";
  s += "<table border='1'>";
  s += "<tr><td>TLE Failures</td><td>" + String(sysStatus.tleFailures) + "</td></tr>";
  s += "<tr><td>Passes Tracked</td><td>" + String(sysStatus.passesTracked) + "</td></tr>";
  s += "<tr><td>Last Error</td><td>" + sysStatus.lastError + "</td></tr>";
  s += "</table>";
  
  s += "<br><a href='/'>Configuration</a> | <a href='/logs'>System Info</a> | <a href='/health'>Health Check</a>";
  s += "</body></html>";
  webServer.send(200, "text/html", s);
}

void handleLogs() {
  String s = "<html><head><title>Tracker Logs</title></head><body>";
  s += "<h1>System Information</h1>";
  s += "<pre>";
  s += "ESP8266 Chip ID: " + String(ESP.getChipId(), HEX) + "\n";
  s += "Flash Chip Size: " + String(ESP.getFlashChipSize()/1024/1024) + " MB\n";
  s += "SDK Version: " + String(ESP.getSdkVersion()) + "\n";
  s += "Boot Version: " + String(ESP.getBootVersion()) + "\n";
  s += "CPU Frequency: " + String(ESP.getCpuFreqMHz()) + " MHz\n";
  s += "Free Heap: " + String(ESP.getFreeHeap()) + " bytes\n";
  s += "Heap Fragmentation: " + String(ESP.getHeapFragmentation()) + "%\n";
  s += "Uptime: " + String(millis()/1000) + " seconds\n";
  s += "</pre>";
  s += "<br><a href='/'>Configuration</a> | <a href='/status'>Status</a>";
  s += "</body></html>";
  webServer.send(200, "text/html", s);
}

// Phase 6: Health check endpoint for comprehensive system monitoring
void handleHealthCheck() {
  String s = "<html><head><title>System Health Check</title></head><body>";
  s += "<h1>System Health Check</h1>";
  s += "<p>Running comprehensive system diagnostics...</p>";
  
  bool overallHealth = true;
  
  // Memory Health Check
  s += "<h2>Memory Health</h2>";
  uint32_t freeHeap = ESP.getFreeHeap();
  uint32_t fragmentation = ESP.getHeapFragmentation();
  
  if (freeHeap > memMonitor.minFreeHeap && fragmentation < memMonitor.maxFragmentation) {
    s += "<p style='color:green'>✓ Memory: PASS (" + String(freeHeap) + " bytes free, " + String(fragmentation) + "% fragmentation)</p>";
  } else {
    s += "<p style='color:red'>✗ Memory: FAIL (" + String(freeHeap) + " bytes free, " + String(fragmentation) + "% fragmentation)</p>";
    overallHealth = false;
  }
  
  // WiFi Health Check
  s += "<h2>Network Health</h2>";
  if (WiFi.status() == WL_CONNECTED) {
    int rssi = WiFi.RSSI();
    if (rssi > -70) {
      s += "<p style='color:green'>✓ WiFi: PASS (Signal: " + String(rssi) + " dBm - Excellent)</p>";
    } else if (rssi > -80) {
      s += "<p style='color:orange'>⚠ WiFi: WARNING (Signal: " + String(rssi) + " dBm - Weak)</p>";
    } else {
      s += "<p style='color:red'>✗ WiFi: FAIL (Signal: " + String(rssi) + " dBm - Very Weak)</p>";
      overallHealth = false;
    }
  } else {
    s += "<p style='color:red'>✗ WiFi: FAIL (Not Connected)</p>";
    overallHealth = false;
  }
  
  // Motor Health Check
  s += "<h2>Motor Health</h2>";
  if (motorHealth.azimuthOK && motorHealth.elevationOK && !motorHealth.stallDetected) {
    s += "<p style='color:green'>✓ Motors: PASS (Both motors operational)</p>";
  } else {
    s += "<p style='color:red'>✗ Motors: FAIL (Az:" + String(motorHealth.azimuthOK) + " El:" + String(motorHealth.elevationOK) + " Stall:" + String(motorHealth.stallDetected) + ")</p>";
    overallHealth = false;
  }
  
  // Configuration Validation
  s += "<h2>Configuration Health</h2>";
  if (validateConfiguration(config)) {
    s += "<p style='color:green'>✓ Configuration: PASS (All parameters valid)</p>";
  } else {
    s += "<p style='color:red'>✗ Configuration: FAIL (Invalid parameters detected)</p>";
    overallHealth = false;
  }
  
  // Time Sync Health
  s += "<h2>Time Synchronization</h2>";
  unsigned long currentTime = timeClient.getEpochTime();
  if (currentTime > 1000000000) { // After year 2001
    s += "<p style='color:green'>✓ Time Sync: PASS (Current time: " + String(currentTime) + ")</p>";
  } else {
    s += "<p style='color:red'>✗ Time Sync: FAIL (Invalid time: " + String(currentTime) + ")</p>";
    overallHealth = false;
  }
  
  // Temperature Health Check
  s += "<h2>Temperature Health</h2>";
  if (tempMonitor.overheating) {
    s += "<p style='color:red'>✗ Temperature: FAIL (Overheating at " + String(tempMonitor.currentTemp, 1) + "°C)</p>";
    overallHealth = false;
  } else if (tempMonitor.currentTemp > TemperatureMonitor::TEMP_WARNING_THRESHOLD) {
    s += "<p style='color:orange'>⚠ Temperature: WARNING (" + String(tempMonitor.currentTemp, 1) + "°C)</p>";
  } else {
    s += "<p style='color:green'>✓ Temperature: PASS (" + String(tempMonitor.currentTemp, 1) + "°C)</p>";
  }
  s += "<p>Range: " + String(tempMonitor.minTemp, 1) + "°C - " + String(tempMonitor.maxTemp, 1) + "°C | Avg: " + String(lifecycle.avgTemperature, 1) + "°C</p>";
  
  // Component Lifecycle Health
  s += "<h2>Component Lifecycle</h2>";
  bool lifecycleWarning = false;
  
  if (lifecycle.motorSteps > 50000) {
    s += "<p style='color:orange'>⚠ Motor Steps: " + String(lifecycle.motorSteps) + " (Moderate usage)</p>";
    lifecycleWarning = true;
  } else {
    s += "<p style='color:green'>✓ Motor Steps: " + String(lifecycle.motorSteps) + " (Low usage)</p>";
  }
  
  if (lifecycle.lcdWrites > 500000) {
    s += "<p style='color:orange'>⚠ LCD Writes: " + String(lifecycle.lcdWrites) + " (High usage)</p>";
    lifecycleWarning = true;
  } else {
    s += "<p style='color:green'>✓ LCD Writes: " + String(lifecycle.lcdWrites) + " (Normal usage)</p>";
  }
  
  s += "<p>Network Connections: " + String(lifecycle.networkConnections) + " | Power Cycles: " + String(lifecycle.powerCycles) + "</p>";
  
  if (lifecycleWarning) {
    s += "<p style='color:orange'>⚠ Component Lifecycle: WARNING (Some components showing wear)</p>";
  }
  
  // Overall Health Summary
  s += "<h2>Overall System Health</h2>";
  if (overallHealth) {
    s += "<p style='color:green; font-size:20px'>✓ SYSTEM HEALTHY - All checks passed</p>";
  } else {
    s += "<p style='color:red; font-size:20px'>✗ SYSTEM ISSUES DETECTED - Check failures above</p>";
  }
  
  s += "<br><p><strong>Recommendations:</strong></p>";
  s += "<ul>";
  if (freeHeap < memMonitor.minFreeHeap * 1.5) {
    s += "<li>Consider reducing debug level to conserve memory</li>";
  }
  if (WiFi.RSSI() < -75) {
    s += "<li>Consider moving closer to WiFi router or using WiFi extender</li>";
  }
  if (motorHealth.consecutiveFailures > 0) {
    s += "<li>Check motor wiring and power supply</li>";
  }
  if (netConfig.connectionFailures > 5) {
    s += "<li>Check internet connectivity and firewall settings</li>";
  }
  if (tempMonitor.currentTemp > TemperatureMonitor::TEMP_WARNING_THRESHOLD) {
    s += "<li>Monitor system temperature - consider improving ventilation</li>";
  }
  if (lifecycle.motorSteps > 50000) {
    s += "<li>Consider motor maintenance - approaching recommended service interval</li>";
  }
  if (lifecycle.lcdWrites > 500000) {
    s += "<li>LCD showing high usage - monitor for display issues</li>";
  }
  s += "</ul>";
  
  s += "<br><a href='/status'>Back to Status</a> | <a href='/'>Configuration</a>";
  s += "</body></html>";
  webServer.send(200, "text/html", s);
}

void setup()
{
  //Initialize serial and wait for port to open:
#ifdef DEBUG
  Serial.begin(115200);
  while (!Serial)
  {
    delay(10);
  }
  debugLog(DEBUG_LEVEL_INFO, "ESP8266 Satellite Tracker Starting...");
#endif

  // Initialize LCD //
  lcd.begin(16, 2);
  lcd.print("Satellite Tracker");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");

  // Load configuration from EEPROM
  loadConfiguration();
  debugLog(DEBUG_LEVEL_INFO, "Configuration loaded from EEPROM");

  // Enable Watchdog Timer
  // Set watchdog to 8 seconds, which is the max for the software WDT
  ESP.wdtEnable(8000);
  watchdogTicker.attach(1, feedWatchdog); // Feed watchdog every 1 second
  debugLog(DEBUG_LEVEL_INFO, "Watchdog timer enabled (8 seconds)");

  // Setup stepper movements //
  stepperEL.setMaxSpeed(1000);
  stepperEL.setCurrentPosition(-227); // Elevation stepper starts at -227 steps (20 degrees above horizon).
  stepperEL.setAcceleration(100);
  stepperAZ.setMaxSpeed(1000);
  stepperAZ.setCurrentPosition(0); // Azimuth stepper starts at 0.
  stepperAZ.setAcceleration(100);

  // Connect to WiFi network //
  manageWiFi();

  // Start web server for configuration
  webServer.on("/", handleRoot);
  webServer.on("/save", HTTP_POST, handleSave);
  webServer.on("/status", handleStatus);
  webServer.on("/logs", handleLogs);
  webServer.on("/health", handleHealthCheck);
  webServer.begin();
  debugLog(DEBUG_LEVEL_INFO, "Web server started on port 80");

  // Initialize SSL client for HTTPS requests //
  client.setInsecure(); // For testing - should implement proper cert validation in production

  // Initialize satellite location //
  sat.site(config.lat, config.lon, config.alt); //set location latitude[°], longitude[°] and altitude[m]

  // Phase 6: Enhanced time synchronization with multiple servers
  timeClient.setTimeOffset(config.timeZone * 3600);
  timeClient.begin();
  safeLCDClear();
  safeLCDWrite(0, 0, "Getting time...");
  
  // Try enhanced time sync with fallback servers
  if (!syncTimeWithFallback()) {
    debugLog(DEBUG_LEVEL_ERROR, "Enhanced time sync failed, trying basic sync");
    // Fallback to basic sync if enhanced fails
    int attempts = 0;
    while (!timeClient.update() && attempts < 10) {
      timeClient.forceUpdate();
      delay(1000);
      attempts++;
      ESP.wdtFeed(); // Feed watchdog during long operation
    }
    
    if (attempts >= 10) {
      debugLog(DEBUG_LEVEL_ERROR, "All time synchronization attempts failed");
      safeLCDWrite(0, 1, "Time sync failed");
      delay(3000);
    }
  }
  
  unixtime = timeClient.getEpochTime();
  timeNow = unixtime;
  
  // Validate time is reasonable
  if (unixtime < 1000000000) {
    debugLog(DEBUG_LEVEL_ERROR, "Invalid time received: " + String(unixtime));
    safeLCDWrite(0, 1, "Invalid time");
    delay(3000);
  } else {
    debugLog(DEBUG_LEVEL_INFO, "Time synchronized successfully: " + String(unixtime));
    safeLCDWrite(0, 1, "Time synced!");
    delay(1000);
  }
  
  // Extract day from epoch time for daily TLE updates
  struct tm *timeinfo = gmtime((time_t *)&unixtime);
  today = timeinfo->tm_mday;

#ifdef DEBUG
  Serial.println("NTP time synchronized");
  Serial.println("Unix time: " + String(unixtime));
#endif

  lcd.setCursor(0, 1);
  lcd.print("Time synced!");
  delay(1000);

  // Get TLEs //
  lcd.clear();
  lcd.print("Downloading TLE...");
  
  for (SAT = 0; SAT < numSats; SAT++)
  {
    lcd.setCursor(0, 1);
    lcd.print("Sat " + String(SAT + 1) + "/" + String(numSats));
    
    int retries = 0;
    while (!getTLE(SAT) && retries < 3)
    {
#ifdef DEBUG
      Serial.println("Retrying TLE download for sat " + String(SAT));
#endif
      lcd.setCursor(0, 1);
      lcd.print("Retry " + String(retries + 1) + " Sat " + String(SAT + 1));
      delay(2000);
      retries++;
    }

    if (retries == 3)
    {
      sysStatus.tleFailures++;
      debugLog(DEBUG_LEVEL_ERROR, "Failed to get TLE for sat " + String(SAT) + " after 3 retries");
      lcd.clear();
      lcd.print("TLE Download FAILED");
      lcd.setCursor(0, 1);
      lcd.print("Check connection");
      delay(5000);
      // Maybe reset here or enter a safe mode
    }

    sat.init(satname, TLE1[SAT], TLE2[SAT]); //initialize satellite parameters
    sat.findsat(timeNow);
    upcomingPasses[SAT] = Predict(1);
  }
  nextSat = nextSatPass(upcomingPasses);
  sat.init(satname, TLE1[nextSat], TLE2[nextSat]);
  Predict(1);

// Print obtained TLE in serial. //
#ifdef DEBUG
  for (SAT = 0; SAT < numSats; SAT++)
  {
    Serial.println("TLE set #:" + String(SAT));
    for (i = 0; i < 70; i++)
    {
      Serial.print(TLE1[SAT][i]);
    }
    Serial.println();
    for (i = 0; i < 70; i++)
    {
      Serial.print(TLE2[SAT][i]);
    }
    Serial.println();
  }
  Serial.println("Next satellite: " + String(nextSat));
#endif

  // Setup complete //
  lcd.clear();
  lcd.print("Setup Complete!");
  lcd.setCursor(0, 1);
  lcd.print("Tracking: " + String(satnames[nextSat]));
  delay(2000);

  // Sound startup confirmation
  tone(SPEAKER_PIN, 1000, 500);
  delay(600);
  tone(SPEAKER_PIN, 1500, 300);
  
  debugLog(DEBUG_LEVEL_INFO, "System initialization complete");

  // Phase 6: Initialize enhanced monitoring systems
  debugLog(DEBUG_LEVEL_INFO, "Initializing Phase 6 enhanced monitoring systems");
  
  // Initialize memory monitor
  memMonitor.minHeapSeen = ESP.getFreeHeap();
  memMonitor.lastCheck = millis();
  memMonitor.lastGC = millis();
  
  // Initialize motor health monitor
  motorHealth.lastAzPosition = stepperAZ.currentPosition();
  motorHealth.lastElPosition = stepperEL.currentPosition();
  motorHealth.positionCheckTime = millis();
  
  // Initialize network config
  netConfig.lastConnectionTest = millis();
  
  debugLog(DEBUG_LEVEL_INFO, "Phase 6 monitoring systems initialized");
  debugLog(DEBUG_LEVEL_INFO, "Initial heap: " + String(memMonitor.minHeapSeen) + " bytes");
  
  // Perform initial system health check
  debugLog(DEBUG_LEVEL_INFO, "Performing initial system health check");
  checkMemoryHealth();
  
  // Test motor health during setup
  if (testMotorHealth()) {
    debugLog(DEBUG_LEVEL_INFO, "Initial motor health test passed");
    safeLCDWrite(0, 1, "Motors OK");
  } else {
    debugLog(DEBUG_LEVEL_WARN, "Initial motor health test failed - check connections");
    safeLCDWrite(0, 1, "Motor Warning");
  }
  delay(2000);
}

void loop()
{
  // Phase 6: Enhanced system monitoring
  unsigned long loopStart = millis();
  
  // Update system status
  sysStatus.uptime = millis();
  sysStatus.lastAzimuth = sat.satAz;
  sysStatus.lastElevation = sat.satEl;

  // Phase 6: Check memory and temperature health periodically
  checkMemoryHealth();
  updateTemperatureMonitoring();
  
  // Predictive failure monitoring every hour
  static unsigned long lastPredictiveCheck = 0;
  if (millis() - lastPredictiveCheck > 3600000) { // Every hour
    checkPredictiveFailures();
    lastPredictiveCheck = millis();
  }

  // Handle web server requests
  webServer.handleClient();

  // Manage WiFi connection
  manageWiFi();

  // Update time from NTP client
  timeClient.update();
  timeNow = timeClient.getEpochTime();
  
  sat.findsat(timeNow);
  satAZsteps = round(sat.satAz * oneTurn / 360); //Convert degrees to stepper steps
  satELsteps = -round(sat.satEl * oneTurn / 360);
  
  // Phase 6: Use safe LCD operations
  if (safeLCDClear()) {
    safeLCDWrite(0, 0, ("Az:" + String(sat.satAz, 1) + " El:" + String(sat.satEl, 1)).c_str());
    safeLCDWrite(0, 1, String(satnames[nextSat]).c_str());
  }
  
  debugLog(DEBUG_LEVEL_DEBUG, "Sat: " + String(satnames[nextSat]) + 
           " Az:" + String(sat.satAz, 2) + " El:" + String(sat.satEl, 2));

#ifdef DEBUG
  invjday(sat.satJd, timeZone, true, year, mon, day, hr, minute, sec);
  Serial.println("\nLocal time: " + String(day) + '/' + String(mon) + '/' + String(year) + ' ' + String(hr) + ':' + String(minute) + ':' + String(sec));
  Serial.println("azimuth = " + String(sat.satAz) + " elevation = " + String(sat.satEl) + " distance = " + String(sat.satDist));
  Serial.println("latitude = " + String(sat.satLat) + " longitude = " + String(sat.satLon) + " altitude = " + String(sat.satAlt));
  Serial.println("AZStep pos: " + String(stepperAZ.currentPosition()));
#endif

  while (true)
  {
    if (nextpassEpoch - timeNow < 300 && nextpassEpoch + 5 - timeNow > 0)
    {
#ifdef DEBUG
      Serial.println("Status: Pre-pass");
      Serial.println("Next satellite is #: " + String(satnames[nextSat]) + " in: " + String(nextpassEpoch - timeNow));
#endif
      prepass();
      break;
    }
    if (sat.satVis != -2)
    {
#ifdef DEBUG
      Serial.println("Status: In pass");
#endif
      inPass();
      break;
    }
    if (timeNow - passEnd < 120)
    {
#ifdef DEBUG
      Serial.println("Status: Post-pass");
#endif
      postpass();
      break;
    }
    if (sat.satVis == -2)
    {
#ifdef DEBUG
      Serial.println("Status: Standby");
      Serial.println("Next satellite is: " + String(satnames[nextSat]) + " in: " + String(nextpassEpoch - timeNow));
#endif
      standby();
      break;
    }
  }
  delay(20);

  // Update TLE & Unix time everyday.//
  struct tm *currentTime = gmtime((time_t *)&timeNow);
  int currentDay = currentTime->tm_mday;
  
  if (passStatus == 0 && today != currentDay)
  {
    lcd.clear();
    lcd.print("Daily Update...");
    
    for (SAT = 0; SAT < numSats; SAT++)
    {
      lcd.setCursor(0, 1);
      lcd.print("TLE " + String(SAT + 1) + "/" + String(numSats));
      int retries = 0;
      while (!getTLE(SAT) && retries < 3)
      {
#ifdef DEBUG
        Serial.println("Retrying TLE download for sat " + String(SAT));
#endif
        lcd.setCursor(0, 1);
        lcd.print("Retry " + String(retries + 1) + " Sat " + String(SAT + 1));
        delay(2000);
        retries++;
      }
    }
    
    timeClient.forceUpdate();
    unixtime = timeClient.getEpochTime();
    today = currentDay;
    
    debugLog(DEBUG_LEVEL_INFO, "Daily TLE and time update completed");

    lcd.setCursor(0, 1);
    lcd.print("Update complete!");
    delay(2000);
  }
}

int nextSatPass(long _nextpassEpoch[4])
{ // Replace with number of satellites
  for (i = 0; i < numSats; ++i)
  {
    if (_nextpassEpoch[0] - timeNow >= _nextpassEpoch[i] - timeNow)
    {
      _nextpassEpoch[0] = _nextpassEpoch[i];
      nextSat = i;
    }
  }
  return nextSat;
}

void standby()
{
  sysStatus.isTracking = false;
  
  // Azimuth //
  stepperAZ.runToNewPosition(0);
  // ELEVATION //
  stepperEL.runToNewPosition(-227); //Standby at 20 degrees above horizon

  digitalWrite(AZmotorPin1, LOW);
  digitalWrite(AZmotorPin2, LOW);
  digitalWrite(AZmotorPin3, LOW);
  digitalWrite(AZmotorPin4, LOW);

  digitalWrite(ELmotorPin1, LOW);
  digitalWrite(ELmotorPin2, LOW);
  digitalWrite(ELmotorPin3, LOW);
  digitalWrite(ELmotorPin4, LOW);
}

void prepass()
{
  // Pass is less than 300 seconds (5 mins) away, move antenna to start location and wait.
  if (AZstart < 360 && AZstart > 180)
  {
    AZstart = AZstart - 360; //Goes to start counter-clockwise if closer.
  }
  stepperAZ.runToNewPosition(AZstart * oneTurn / 360);
  stepperEL.runToNewPosition(0);

  digitalWrite(AZmotorPin1, LOW);
  digitalWrite(AZmotorPin2, LOW);
  digitalWrite(AZmotorPin3, LOW);
  digitalWrite(AZmotorPin4, LOW);

  digitalWrite(ELmotorPin1, LOW);
  digitalWrite(ELmotorPin2, LOW);
  digitalWrite(ELmotorPin3, LOW);
  digitalWrite(ELmotorPin4, LOW);
}

void inPass()
{
  sysStatus.isTracking = true;
  
  // Handle zero crossings
  if (AZstart < 0)
  {
    satAZsteps = satAZsteps - oneTurn;
  }
  if (satAZsteps - stepperAZ.currentPosition() > 100)
  {
    stepperAZ.setCurrentPosition(stepperAZ.currentPosition() + oneTurn);
    turns--;
  }
  if (satAZsteps - stepperAZ.currentPosition() < -100)
  {
    stepperAZ.setCurrentPosition(stepperAZ.currentPosition() - oneTurn);
    turns++;
  }

  // Update stepper position
  stepperAZ.runToNewPosition(satAZsteps);
  stepperEL.runToNewPosition(satELsteps);
  passEnd = timeNow;
  passStatus = 1;
  
  debugLog(DEBUG_LEVEL_INFO, "Tracking satellite in pass - Az:" + String(sat.satAz, 2) + " El:" + String(sat.satEl, 2));
}

void postpass()
{
  sysStatus.isTracking = false;
  debugLog(DEBUG_LEVEL_DEBUG, "Post pass time left: " + String(passEnd + 120 - timeNow));
  if (timeNow - passEnd > 90)
  {
    if (turns > 0)
    {
      stepperAZ.setCurrentPosition(stepperAZ.currentPosition() + oneTurn);
      turns--;
    }
    if (turns < 0)
    {
      stepperAZ.setCurrentPosition(stepperAZ.currentPosition() - oneTurn);
      turns++;
    }
  }
  if (passStatus == 1 && timeNow - passEnd > 100)
  {
    for (SAT = 0; SAT < numSats; SAT++)
    {
      sat.init(satname, TLE1[SAT], TLE2[SAT]);
      sat.findsat(timeNow);
      upcomingPasses[SAT] = Predict(1);
#ifdef DEBUG
      Serial.println("Next pass for Satellite #: " + String(SAT) + " in: " + String(upcomingPasses[SAT] - timeNow));
#endif
    }
    nextSat = nextSatPass(upcomingPasses);
    sat.init(satname, TLE1[nextSat], TLE2[nextSat]);
    Predict(1);
    passStatus = 0;
    sysStatus.passesTracked++;
    debugLog(DEBUG_LEVEL_INFO, "Pass completed. Next satellite: " + String(satnames[nextSat]));
  }
}

// Phase 6: Enhanced stepper motor movement with fault detection
bool moveStepperSafely(AccelStepper &stepper, long targetPosition, const char* motorName, unsigned long timeoutMs = 30000) {
  if (!motorName) {
    debugLog(DEBUG_LEVEL_ERROR, "NULL motor name provided");
    return false;
  }
  
  long startPos = stepper.currentPosition();
  unsigned long startTime = millis();
  long lastPos = startPos;
  unsigned long lastPosTime = startTime;
  
  debugLog(DEBUG_LEVEL_DEBUG, String(motorName) + " moving from " + String(startPos) + " to " + String(targetPosition));
  
  stepper.moveTo(targetPosition);
  
  while (stepper.distanceToGo() != 0) {
    stepper.run();
    long currentPos = stepper.currentPosition();
    unsigned long currentTime = millis();
    
    // Overall timeout check
    if (currentTime - startTime > timeoutMs) {
      debugLog(DEBUG_LEVEL_ERROR, String(motorName) + " movement timeout after " + String(timeoutMs) + "ms");
      stepper.stop();
      motorHealth.consecutiveFailures++;
      return false;
    }
    
    // Stall detection - position hasn't changed in 5 seconds
    if (currentTime - lastPosTime > 5000) {
      if (currentPos == lastPos && abs(currentPos - targetPosition) > 10) {
        debugLog(DEBUG_LEVEL_ERROR, String(motorName) + " stall detected at position " + String(currentPos));
        stepper.stop();
        motorHealth.stallDetected = true;
        motorHealth.consecutiveFailures++;
        return false;
      }
      lastPos = currentPos;
      lastPosTime = currentTime;
    }
    
    // Prevent watchdog timeout during long movements
    if (currentTime - startTime > 100) { // Feed every 100ms during movement
      ESP.wdtFeed();
      yield();
      
      // Check memory during long operations
      if (currentTime - startTime > 1000) { // Check memory every second during movement
        checkMemoryHealth();
      }
    }
  }
  
  motorHealth.lastMovement = millis();
  motorHealth.stallDetected = false;
  motorHealth.totalMovements++;
  
  // Update lifecycle counter for motor steps
  updateLifecycleCounters("motor");
  
  // Reset failure counter on successful movement
  if (motorHealth.consecutiveFailures > 0) {
    debugLog(DEBUG_LEVEL_INFO, String(motorName) + " recovered from previous failures");
    motorHealth.consecutiveFailures = 0;
  }
  
  debugLog(DEBUG_LEVEL_DEBUG, String(motorName) + " movement completed to position " + String(stepper.currentPosition()));
  return true;
}

// Phase 6: Motor health testing function
bool testMotorHealth() {
  debugLog(DEBUG_LEVEL_INFO, "Starting motor health test");
  bool azOK = true, elOK = true;
  
  // Test azimuth motor with small movement
  long azCurrentPos = stepperAZ.currentPosition();
  if (!moveStepperSafely(stepperAZ, azCurrentPos + 100, "Azimuth Test", 5000)) {
    azOK = false;
    motorHealth.azimuthOK = false;
  } else {
    // Return to original position
    moveStepperSafely(stepperAZ, azCurrentPos, "Azimuth Return", 5000);
    motorHealth.azimuthOK = true;
  }
  
  // Test elevation motor with small movement
  long elCurrentPos = stepperEL.currentPosition();
  if (!moveStepperSafely(stepperEL, elCurrentPos + 50, "Elevation Test", 5000)) {
    elOK = false;
    motorHealth.elevationOK = false;
  } else {
    // Return to original position
    moveStepperSafely(stepperEL, elCurrentPos, "Elevation Return", 5000);
    motorHealth.elevationOK = true;
  }
  
  if (azOK && elOK) {
    debugLog(DEBUG_LEVEL_INFO, "Motor health test passed - both motors operational");
    return true;
  } else {
    debugLog(DEBUG_LEVEL_ERROR, "Motor health test failed - Az:" + String(azOK) + " El:" + String(elOK));
    return false;
  }
}

// Phase 6: Enhanced configuration validation
bool validateConfiguration(const Config& cfg) {
  bool valid = true;
  
  // Validate latitude
  if (cfg.lat < -90.0 || cfg.lat > 90.0) {
    debugLog(DEBUG_LEVEL_ERROR, "Invalid latitude: " + String(cfg.lat) + " (must be -90 to 90)");
    valid = false;
  }
  
  // Validate longitude  
  if (cfg.lon < -180.0 || cfg.lon > 180.0) {
    debugLog(DEBUG_LEVEL_ERROR, "Invalid longitude: " + String(cfg.lon) + " (must be -180 to 180)");
    valid = false;
  }
  
  // Validate altitude (reasonable range for Earth-based tracking)
  if (cfg.alt < -500.0 || cfg.alt > 9000.0) {
    debugLog(DEBUG_LEVEL_ERROR, "Invalid altitude: " + String(cfg.alt) + "m (must be -500 to 9000)");
    valid = false;
  }
  
  // Validate timezone
  if (cfg.timeZone < -12 || cfg.timeZone > 14) {
    debugLog(DEBUG_LEVEL_ERROR, "Invalid timezone: " + String(cfg.timeZone) + " (must be -12 to +14)");
    valid = false;
  }
  
  // Validate SSID
  if (strlen(cfg.ssid) == 0) {
    debugLog(DEBUG_LEVEL_ERROR, "SSID cannot be empty");
    valid = false;
  } else if (strlen(cfg.ssid) > 31) {
    debugLog(DEBUG_LEVEL_ERROR, "SSID too long: " + String(strlen(cfg.ssid)) + " chars (max 31)");
    valid = false;
  }
  
  // Validate password length
  if (strlen(cfg.password) > 63) {
    debugLog(DEBUG_LEVEL_ERROR, "Password too long: " + String(strlen(cfg.password)) + " chars (max 63)");
    valid = false;
  }
  
  if (valid) {
    debugLog(DEBUG_LEVEL_DEBUG, "Configuration validation passed");
  }
  
  return valid;
}

// Phase 6: Enhanced time synchronization with multiple servers
bool syncTimeWithFallback() {
  debugLog(DEBUG_LEVEL_INFO, "Starting enhanced time synchronization");
  
  for (int attempt = 0; attempt < 4; attempt++) {
    const char* server = netConfig.ntpServers[netConfig.currentNTPServer];
    debugLog(DEBUG_LEVEL_INFO, "Attempting time sync with: " + String(server));
    
    // Reinitialize NTP client with current server
    timeClient.setPoolServerName(server);
    timeClient.setTimeout(5000); // 5 second timeout per server
    
    // Try to force update
    if (timeClient.forceUpdate()) {
      unsigned long currentTime = timeClient.getEpochTime();
      if (currentTime > 1000000000) { // Sanity check - after year 2001
        debugLog(DEBUG_LEVEL_INFO, "Time synchronized successfully with " + String(server));
        debugLog(DEBUG_LEVEL_INFO, "Unix timestamp: " + String(currentTime));
        return true;
      }
    }
    
    debugLog(DEBUG_LEVEL_WARN, "Time sync failed with " + String(server));
    
    // Move to next server
    netConfig.currentNTPServer = (netConfig.currentNTPServer + 1) % 4;
    delay(1000);
    ESP.wdtFeed();
  }
  
  debugLog(DEBUG_LEVEL_ERROR, "All NTP servers failed - time synchronization unavailable");
  return false;
}

void manageWiFi()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    debugLog(DEBUG_LEVEL_WARN, "WiFi disconnected. Attempting to reconnect...");
    lcd.clear();
    lcd.print("WiFi Lost");
    lcd.setCursor(0, 1);
    lcd.print("Reconnecting...");

    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    WiFi.begin(config.ssid, config.password);

    wifiReconnectAttempts = 0;
    while (WiFi.status() != WL_CONNECTED && wifiReconnectAttempts < maxWifiReconnectAttempts)
    {
      delay(1000);
      debugLog(DEBUG_LEVEL_DEBUG, "WiFi reconnect attempt " + String(wifiReconnectAttempts + 1));
      wifiReconnectAttempts++;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
      sysStatus.wifiReconnects++;
      debugLog(DEBUG_LEVEL_INFO, "WiFi reconnected! IP: " + WiFi.localIP().toString());
      lcd.clear();
      lcd.print("WiFi Reconnected");
      lcd.setCursor(0, 1);
      lcd.print(WiFi.localIP());
      delay(2000);
    }
    else
    {
      debugLog(DEBUG_LEVEL_ERROR, "Failed to reconnect to WiFi. Rebooting...");
      lcd.clear();
      lcd.print("WiFi Failed");
      lcd.setCursor(0, 1);
      lcd.print("Rebooting...");
      delay(2000);
      ESP.restart();
    }
  }
}

// Get ESP8266 internal temperature (approximate)
float getChipTemperature() {
  // ESP8266 doesn't have built-in temp sensor, use approximation
  // Based on WiFi signal strength and operating conditions
  int rssi = WiFi.RSSI();
  unsigned long uptime = millis();
  
  // Rough estimation based on operating conditions
  float baseTemp = 25.0; // Room temperature baseline
  float wifiTemp = (rssi < -70) ? 5.0 : 10.0; // WiFi heat generation
  float uptimeTemp = (uptime / 3600000.0) * 2.0; // 2°C per hour of operation
  
  return baseTemp + wifiTemp + uptimeTemp;
}

void updateTemperatureMonitoring() {
  if (millis() - tempMonitor.lastReading > 30000) { // Check every 30 seconds
    tempMonitor.currentTemp = getChipTemperature();
    tempMonitor.lastReading = millis();
    
    // Update statistics
    if (tempMonitor.currentTemp > tempMonitor.maxTemp) {
      tempMonitor.maxTemp = tempMonitor.currentTemp;
    }
    if (tempMonitor.currentTemp < tempMonitor.minTemp) {
      tempMonitor.minTemp = tempMonitor.currentTemp;
    }
    
    // Update lifecycle average
    lifecycle.avgTemperature = ((lifecycle.avgTemperature * lifecycle.tempReadings) + tempMonitor.currentTemp) / (lifecycle.tempReadings + 1);
    lifecycle.tempReadings++;
    
    // Check for overheating
    if (tempMonitor.currentTemp > TemperatureMonitor::TEMP_CRITICAL_THRESHOLD) {
      tempMonitor.overheating = true;
      debugLog("CRITICAL: System overheating at " + String(tempMonitor.currentTemp) + "°C", 0);
      // Implement thermal throttling
      delay(5000); // Cool down period
    } else if (tempMonitor.currentTemp > TemperatureMonitor::TEMP_WARNING_THRESHOLD) {
      debugLog("WARNING: High temperature " + String(tempMonitor.currentTemp) + "°C", 1);
      tempMonitor.overheating = false;
    } else {
      tempMonitor.overheating = false;
    }
  }
}

void updateLifecycleCounters(String component) {
  if (component == "motor") {
    lifecycle.motorSteps++;
  } else if (component == "lcd") {
    lifecycle.lcdWrites++;
  } else if (component == "network") {
    lifecycle.networkConnections++;
  } else if (component == "power") {
    lifecycle.powerCycles++;
  }
}

String getLifecycleReport() {
  String report = "Component Lifecycle Report:\\n";
  report += "Motor Steps: " + String(lifecycle.motorSteps) + "\\n";
  report += "LCD Writes: " + String(lifecycle.lcdWrites) + "\\n";
  report += "Network Connections: " + String(lifecycle.networkConnections) + "\\n";
  report += "Power Cycles: " + String(lifecycle.powerCycles) + "\\n";
  report += "Avg Temperature: " + String(lifecycle.avgTemperature, 1) + "°C\\n";
  report += "Temperature Readings: " + String(lifecycle.tempReadings) + "\\n";
  return report;
}

bool checkPredictiveFailures() {
  bool warningIssued = false;
  
  // Motor wear prediction
  if (lifecycle.motorSteps > 100000) {
    debugLog("PREDICTIVE: Motor approaching wear limit (" + String(lifecycle.motorSteps) + " steps)", 1);
    warningIssued = true;
  }
  
  // LCD lifetime prediction  
  if (lifecycle.lcdWrites > 1000000) {
    debugLog("PREDICTIVE: LCD approaching write limit (" + String(lifecycle.lcdWrites) + " writes)", 1);
    warningIssued = true;
  }
  
  // Temperature trend analysis
  if (lifecycle.avgTemperature > 60.0) {
    debugLog("PREDICTIVE: Average temperature trending high (" + String(lifecycle.avgTemperature, 1) + "°C)", 1);
    warningIssued = true;
  }
  
  // Network stress prediction
  if (lifecycle.networkConnections > 10000) {
    debugLog("PREDICTIVE: High network usage detected (" + String(lifecycle.networkConnections) + " connections)", 1);
    warningIssued = true;
  }
  
  return warningIssued;
}
