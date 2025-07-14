// Phase 6: TLE data validation functions
bool verifyTLEChecksum(const char* line) {
  if (!line || strlen(line) < 69) {
    debugLog(DEBUG_LEVEL_ERROR, "TLE line too short for checksum validation");
    return false;
  }
  
  int checksum = 0;
  
  // Calculate checksum for first 68 characters
  for (int i = 0; i < 68; i++) {
    if (isdigit(line[i])) {
      checksum += (line[i] - '0');
    } else if (line[i] == '-') {
      checksum += 1;
    }
  }
  
  int expectedChecksum = line[68] - '0';
  bool valid = (checksum % 10) == expectedChecksum;
  
  if (!valid) {
    debugLog(DEBUG_LEVEL_ERROR, "TLE checksum mismatch. Calculated: " + 
             String(checksum % 10) + ", Expected: " + String(expectedChecksum));
  }
  
  return valid;
}

bool validateTLEData(const char* line1, const char* line2, const char* satName) {
  if (!line1 || !line2) {
    debugLog(DEBUG_LEVEL_ERROR, "NULL TLE lines provided for " + String(satName ? satName : "satellite"));
    return false;
  }
  
  // Check line lengths
  if (strlen(line1) != 69 || strlen(line2) != 69) {
    debugLog(DEBUG_LEVEL_ERROR, "Invalid TLE line lengths for " + String(satName ? satName : "satellite"));
    return false;
  }
  
  // Check line identifiers
  if (line1[0] != '1' || line2[0] != '2') {
    debugLog(DEBUG_LEVEL_ERROR, "Invalid TLE line identifiers for " + String(satName ? satName : "satellite"));
    return false;
  }
  
  // Check satellite numbers match
  char satNum1[6], satNum2[6];
  strncpy(satNum1, &line1[2], 5); satNum1[5] = '\0';
  strncpy(satNum2, &line2[2], 5); satNum2[5] = '\0';
  
  if (strcmp(satNum1, satNum2) != 0) {
    debugLog(DEBUG_LEVEL_ERROR, "TLE satellite number mismatch for " + String(satName ? satName : "satellite"));
    return false;
  }
  
  // Verify checksums
  if (!verifyTLEChecksum(line1) || !verifyTLEChecksum(line2)) {
    debugLog(DEBUG_LEVEL_ERROR, "TLE checksum validation failed for " + String(satName ? satName : "satellite"));
    return false;
  }
  
  debugLog(DEBUG_LEVEL_DEBUG, "TLE validation passed for " + String(satName ? satName : "satellite"));
  return true;
}

// Phase 6: Enhanced HTTPS connection with fallback
bool connectHTTPSWithFallback(WiFiClientSecure &client, const char* hostname, int port = 443) {
  if (!hostname) {
    debugLog(DEBUG_LEVEL_ERROR, "NULL hostname provided");
    return false;
  }
  
  debugLog(DEBUG_LEVEL_DEBUG, "Attempting HTTPS connection to " + String(hostname) + ":" + String(port));
  
  // Try primary connection with hostname
  client.setTimeout(10000); // 10 second timeout
  
  if (client.connect(hostname, port)) {
    debugLog(DEBUG_LEVEL_DEBUG, "HTTPS connection successful via hostname");
    netConfig.dnsWorking = true;
    return true;
  }
  
  debugLog(DEBUG_LEVEL_WARN, "Hostname connection failed, trying fallback IP");
  
  // Try fallback IP if DNS/hostname fails
  if (client.connect(netConfig.fallbackIP, port)) {
    debugLog(DEBUG_LEVEL_WARN, "HTTPS connection successful via fallback IP: " + String(netConfig.fallbackIP));
    netConfig.dnsWorking = false;
    return true;
  }
  
  debugLog(DEBUG_LEVEL_ERROR, "All HTTPS connection attempts failed");
  netConfig.connectionFailures++;
  return false;
}

// Phase 6: Enhanced getTLE function with comprehensive error handling
bool getTLE(int SAT){
  const unsigned long timeout = 15000; // 15 second timeout
  unsigned long startTime = millis();
  
  debugLog(DEBUG_LEVEL_INFO, "Starting TLE download for " + String(satnames[SAT]) + " (satellite " + String(SAT) + ")");
  
  // Check memory before large operation
  checkMemoryHealth();
  
  // Enhanced HTTPS connection with fallback
  if (!connectHTTPSWithFallback(client, celestrakServer)) {
    debugLog(DEBUG_LEVEL_ERROR, "Failed to establish HTTPS connection for satellite " + String(SAT));
    return false;
  }
  
  debugLog(DEBUG_LEVEL_DEBUG, "Connected to server via HTTPS for " + String(satnames[SAT]));
  
  // Make HTTPS request with enhanced headers
  client.print("GET ");
  client.print(String(satURL[SAT]));
  client.println(" HTTP/1.1");
  client.println("Host: celestrak.org");
  client.println("User-Agent: ESP8266SatelliteTracker/2.0");
  client.println("Accept: text/plain");
  client.println("Connection: close");
  client.println();
  
  // Wait for response with enhanced timeout handling
  while (client.available() == 0) {
    if (millis() - startTime > timeout) {
      debugLog(DEBUG_LEVEL_ERROR, "HTTPS request timeout for " + String(satnames[SAT]));
      client.stop();
      return false;
    }
    ESP.wdtFeed(); // Feed watchdog during wait
    yield();
  }
  
  // Read response with improved parsing
  String response = "";
  bool headersPassed = false;
  int bytesRead = 0;
  
  while (client.connected() || client.available()) {
    if (client.available()) {
      String line = client.readStringUntil('\n');
      bytesRead += line.length();
      
      // Skip HTTP headers
      if (!headersPassed) {
        if (line == "\r") {
          headersPassed = true;
          debugLog(DEBUG_LEVEL_DEBUG, "Headers processed, reading TLE data");
        }
        continue;
      }
      
      // Collect TLE data
      response += line + "\n";
      
      // Prevent excessive memory usage
      if (response.length() > 1000) {
        debugLog(DEBUG_LEVEL_WARN, "TLE response too large, truncating");
        break;
      }
    }
    
    // Timeout check
    if (millis() - startTime > timeout) {
      debugLog(DEBUG_LEVEL_ERROR, "TLE response timeout for " + String(satnames[SAT]));
      break;
    }
    
    ESP.wdtFeed();
    yield();
  }
  
  client.stop();
  netConfig.totalDataTransferred += bytesRead;
  
  debugLog(DEBUG_LEVEL_DEBUG, "Downloaded " + String(bytesRead) + " bytes for " + String(satnames[SAT]));
  
  // Parse TLE data with enhanced validation
  if (response.length() < 140) {
    debugLog(DEBUG_LEVEL_ERROR, "TLE response too short for " + String(satnames[SAT]) + " (got " + String(response.length()) + " bytes)");
    return false;
  }
  
  // Find first line (satellite name)
  int nameEnd = response.indexOf('\n');
  if (nameEnd < 0) {
    debugLog(DEBUG_LEVEL_ERROR, "Invalid TLE format - no line breaks found");
    return false;
  }
  
  // Extract TLE line 1
  int line1Start = nameEnd + 1;
  int line1End = response.indexOf('\n', line1Start);
  if (line1End < 0 || line1End - line1Start < 69) {
    debugLog(DEBUG_LEVEL_ERROR, "Invalid TLE line 1 format for " + String(satnames[SAT]));
    return false;
  }
  
  // Copy TLE line 1
  for (int i = 0; i < 69 && line1Start + i < response.length(); i++) {
    TLE1[SAT][i] = response.charAt(line1Start + i);
  }
  TLE1[SAT][69] = '\0';
  
  // Extract TLE line 2
  int line2Start = line1End + 1;
  int line2End = response.indexOf('\n', line2Start);
  if (line2Start + 69 > response.length()) {
    debugLog(DEBUG_LEVEL_ERROR, "Invalid TLE line 2 format for " + String(satnames[SAT]));
    return false;
  }
  
  // Copy TLE line 2
  for (int i = 0; i < 69 && line2Start + i < response.length(); i++) {
    TLE2[SAT][i] = response.charAt(line2Start + i);
  }
  TLE2[SAT][69] = '\0';
  
  // Validate TLE data integrity
  if (!validateTLEData(TLE1[SAT], TLE2[SAT], satnames[SAT])) {
    debugLog(DEBUG_LEVEL_ERROR, "TLE validation failed for " + String(satnames[SAT]));
    return false;
  }
  
  unsigned long downloadTime = millis() - startTime;
  debugLog(DEBUG_LEVEL_INFO, "TLE download completed successfully for " + String(satnames[SAT]) + " in " + String(downloadTime) + "ms");
  
  return true;
}
