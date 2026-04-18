#pragma once

#include <WiFi.h>

// HTTP parsing utilities: first-line parse, Content-Length extraction
struct CWWebServerHTTP {
  
  // Parse HTTP request first line and query string
  // Returns: method, path, key, value
  // Path is stripped of query parameters
  static void parseFirstLine(
    const String& firstLine,
    String& method,
    String& path,
    String& key,
    String& value
  ) {
    // Parse "GET /path?query=value HTTP/1.1" style
    int s1 = firstLine.indexOf(' ');
    int s2 = firstLine.indexOf(' ', s1 + 1);
    method = firstLine.substring(0, s1);
    path = firstLine.substring(s1 + 1, s2);
    
    // Extract query string if present
    if (path.indexOf('?') > 0) {
      String qs = path.substring(path.indexOf('?') + 1);
      parseEncodedAssignment(qs, key, value);
      path = path.substring(0, path.indexOf('?'));
    }
  }

  // Parse a single "key=value" assignment (URL encoded)
  static void parseEncodedAssignment(const String& encoded, String& key, String& value) {
    int eqIdx = encoded.indexOf('=');
    if (eqIdx < 0) {
      key = encoded;
      value = "";
      return;
    }
    key = encoded.substring(0, eqIdx);
    value = encoded.substring(eqIdx + 1);
  }

  // Drain and extract headers from HTTP request
  // Specifically looks for Content-Length and Expect: 100-continue
  // Returns: contentLength (0 if not found), expectContinue flag
  static void drainHeaders(
    WiFiClient& client,
    int timeout_ms,
    int& contentLength,
    bool& expectContinue
  ) {
    contentLength = 0;
    expectContinue = false;
    
    client.setTimeout(timeout_ms);
    unsigned long deadline = millis() + (timeout_ms * 2);
    
    while (client.connected() && millis() < deadline) {
      String line = client.readStringUntil('\n');
      line.trim();
      if (line.isEmpty()) break;  // blank line = end of headers
      
      String lower = line;
      lower.toLowerCase();
      
      if (lower.startsWith("content-length:")) {
        contentLength = line.substring(15).toInt();
      } else if (lower.startsWith("expect:")) {
        if (lower.indexOf("100-continue") >= 0) {
          expectContinue = true;
        }
      }
    }
  }
};
