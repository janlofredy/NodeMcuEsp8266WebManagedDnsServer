#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <FS.h>
#include <ArduinoJson.h>

const byte DNS_PORT = 53;
IPAddress local_IP(192, 168, 1, 100); // Static IP
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

DNSServer dnsServer;
ESP8266WebServer server(80);
StaticJsonDocument<1024> dnsDoc;

void saveDNS() {
  File file = SPIFFS.open("/dns.json", "w");
  if (file) {
    serializeJson(dnsDoc, file);
    file.close();
  }
}

void loadDNS() {
  File file = SPIFFS.open("/dns.json", "r");
  if (file) {
    DeserializationError error = deserializeJson(dnsDoc, file);
    file.close();
    if (error) Serial.println("Failed to load DNS JSON");
  }
}

IPAddress resolveDomain(const String& domain) {
  if (dnsDoc.containsKey(domain)) {
    IPAddress ip;
    ip.fromString(dnsDoc[domain].as<String>());
    return ip;
  }
  return local_IP; // Default fallback
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.config(local_IP, gateway, subnet);
  WiFi.begin("YOUR_SSID", "YOUR_PASSWORD");
  while (WiFi.status() != WL_CONNECTED) delay(500);

  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS failed");
    return;
  }

  loadDNS();

  dnsServer.start(DNS_PORT, "*", local_IP);
  server.serveStatic("/", SPIFFS, "/index.html");

  server.on("/add", HTTP_POST, []() {
    String domain = server.arg("domain");
    String ip = server.arg("ip");
    dnsDoc[domain] = ip;
    saveDNS();
    server.send(200, "text/plain", "Added");
  });

  server.on("/delete", HTTP_POST, []() {
    String domain = server.arg("domain");
    dnsDoc.remove(domain);
    saveDNS();
    server.send(200, "text/plain", "Deleted");
  });

  server.on("/list", HTTP_GET, []() {
    String output;
    serializeJsonPretty(dnsDoc, output);
    server.send(200, "application/json", output);
  });

  server.begin();
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
}