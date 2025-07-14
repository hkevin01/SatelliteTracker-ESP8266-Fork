/*
 * ESP8266 Satellite Tracker - Phase 3B.1: WiFi Connection Test
 * Tests WiFi connectivity and stability
 * 
 * This test verifies:
 * - ESP8266 connects to WiFi successfully
 * - Connection remains stable over time
 * - Signal strength is adequate
 * - IP address assignment works
 * 
 * Setup required:
 * - Update WIFI_SSID and WIFI_PASSWORD below
 * - Ensure WiFi network is available
 */

#include <ESP8266WiFi.h>

// WiFi credentials - UPDATE THESE FOR YOUR NETWORK
const char* WIFI_SSID = "YOUR_WIFI_SSID";       // Replace with your WiFi name
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD"; // Replace with your WiFi password

// Test parameters
const int CONNECTION_TIMEOUT = 30000;  // 30 seconds max for connection
const int SIGNAL_STRENGTH_MIN = -70;   // Minimum acceptable signal strength (dBm)
const int STABILITY_TEST_DURATION = 300000; // 5 minutes stability test

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println();
  Serial.println("===============================================");
  Serial.println("ESP8266 Satellite Tracker - WiFi Connection Test");
  Serial.println("Test ID: 3B.1 - WiFi Connection and Stability");
  Serial.println("===============================================");
  
  // Display ESP8266 info
  Serial.print("ESP8266 Chip ID: ");
  Serial.println(ESP.getChipId(), HEX);
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());
  Serial.print("SDK Version: ");
  Serial.println(ESP.getSdkVersion());
  
  // Start WiFi in station mode
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(1000);
  
  Serial.println();
  Serial.println("Starting WiFi connection test...");
  Serial.print("Connecting to: ");
  Serial.println(WIFI_SSID);
  
  testWiFiConnection();
}

void loop() {
  static unsigned long testStartTime = 0;
  static unsigned long lastStatusUpdate = 0;
  static int disconnectionCount = 0;
  static bool stabilityTestComplete = false;
  
  // Start stability test timing
  if (testStartTime == 0) {
    testStartTime = millis();
  }
  
  // Monitor connection status
  if (WiFi.status() != WL_CONNECTED) {
    disconnectionCount++;
    Serial.print("ERROR: WiFi disconnected! Attempt #");
    Serial.println(disconnectionCount);
    
    // Try to reconnect
    WiFi.reconnect();
    delay(5000);
  }
  
  // Status update every 30 seconds
  if (millis() - lastStatusUpdate > 30000) {
    displayWiFiStatus();
    lastStatusUpdate = millis();
  }
  
  // Check if stability test is complete
  if (!stabilityTestComplete && (millis() - testStartTime > STABILITY_TEST_DURATION)) {
    Serial.println();
    Serial.println("===============================================");
    Serial.println("WiFi Stability Test Complete!");
    Serial.print("Test duration: ");
    Serial.print(STABILITY_TEST_DURATION / 1000);
    Serial.println(" seconds");
    Serial.print("Disconnections: ");
    Serial.println(disconnectionCount);
    
    if (disconnectionCount == 0) {
      Serial.println("RESULT: ✅ PASS - WiFi connection stable");
    } else if (disconnectionCount <= 2) {
      Serial.println("RESULT: ⚠️ PARTIAL - Some disconnections occurred");
    } else {
      Serial.println("RESULT: ❌ FAIL - Too many disconnections");
    }
    Serial.println("===============================================");
    
    stabilityTestComplete = true;
  }
  
  // After stability test, continue monitoring
  if (stabilityTestComplete) {
    delay(60000); // Check every minute
  } else {
    delay(1000);  // Check every second during stability test
  }
}

void testWiFiConnection() {
  unsigned long startTime = millis();
  
  // Begin WiFi connection
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  // Wait for connection with timeout
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED && (millis() - startTime < CONNECTION_TIMEOUT)) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  
  // Check connection result
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("✅ WiFi connection successful!");
    
    // Display connection details
    displayWiFiStatus();
    
    // Test signal strength
    int rssi = WiFi.RSSI();
    Serial.print("Signal strength test: ");
    if (rssi >= SIGNAL_STRENGTH_MIN) {
      Serial.print("✅ PASS (");
      Serial.print(rssi);
      Serial.println(" dBm - Good signal)");
    } else {
      Serial.print("⚠️ WEAK (");
      Serial.print(rssi);
      Serial.print(" dBm - Below ");
      Serial.print(SIGNAL_STRENGTH_MIN);
      Serial.println(" dBm threshold)");
    }
    
    // Test internet connectivity
    testInternetConnectivity();
    
  } else {
    Serial.println("❌ WiFi connection failed!");
    Serial.print("Status code: ");
    Serial.println(WiFi.status());
    printWiFiStatusCode(WiFi.status());
    
    Serial.println("Please check:");
    Serial.println("- WiFi SSID and password are correct");
    Serial.println("- WiFi network is available");
    Serial.println("- ESP8266 is within range");
  }
}

void displayWiFiStatus() {
  Serial.println("\n--- WiFi Status ---");
  Serial.print("Status: ");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected ✅");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Gateway: ");
    Serial.println(WiFi.gatewayIP());
    Serial.print("DNS: ");
    Serial.println(WiFi.dnsIP());
    Serial.print("Subnet Mask: ");
    Serial.println(WiFi.subnetMask());
    Serial.print("Signal Strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    Serial.print("Uptime: ");
    Serial.print(millis() / 1000);
    Serial.println(" seconds");
  } else {
    Serial.print("Disconnected ❌ (");
    Serial.print(WiFi.status());
    Serial.println(")");
    printWiFiStatusCode(WiFi.status());
  }
  Serial.println("-------------------");
}

void testInternetConnectivity() {
  Serial.println("\nTesting internet connectivity...");
  
  WiFiClient client;
  const char* host = "google.com";
  const int port = 80;
  
  if (client.connect(host, port)) {
    Serial.println("✅ Internet connectivity test passed");
    client.stop();
  } else {
    Serial.println("❌ Internet connectivity test failed");
    Serial.println("Check router internet connection");
  }
}

void printWiFiStatusCode(wl_status_t status) {
  switch (status) {
    case WL_IDLE_STATUS:
      Serial.println("WiFi is changing modes");
      break;
    case WL_NO_SSID_AVAIL:
      Serial.println("Configured SSID cannot be reached");
      break;
    case WL_SCAN_COMPLETED:
      Serial.println("Scan networks completed");
      break;
    case WL_CONNECTED:
      Serial.println("Connected to a WiFi network");
      break;
    case WL_CONNECT_FAILED:
      Serial.println("Connection failed");
      break;
    case WL_CONNECTION_LOST:
      Serial.println("Connection lost");
      break;
    case WL_DISCONNECTED:
      Serial.println("Disconnected from network");
      break;
    default:
      Serial.print("Unknown status: ");
      Serial.println(status);
      break;
  }
}
