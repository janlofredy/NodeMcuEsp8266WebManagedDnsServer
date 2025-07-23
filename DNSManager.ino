#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <FS.h>
#include <ArduinoJson.h>

const byte DNS_PORT = 53;
DNSServer dnsServer;
ESP8266WebServer server(80);
StaticJsonDocument<1024> dnsDoc;
StaticJsonDocument<256> netDoc;

const char* www_username = "admin";
const char* www_password = "esp8266";

IPAddress local_IP, gateway, subnet;

void loadIPConfig() {
  File file = SPIFFS.open("/netconfig.json", "r");
  if (!file) return;
  DeserializationError error = deserializeJson(netDoc, file);
  file.close();
  if (error) return;
  local_IP.fromString(netDoc["ip"]);
  gateway.fromString(netDoc["gateway"]);
  subnet.fromString(netDoc["subnet"]);
}

void saveIPConfig(String ip, String gw, String sn) {
  netDoc["ip"] = ip;
  netDoc["gateway"] = gw;
  netDoc["subnet"] = sn;
  File file = SPIFFS.open("/netconfig.json", "w");
  serializeJson(netDoc, file);
  file.close();
}

void saveDNS() {
  File file = SPIFFS.open("/dns.json", "w");
  serializeJson(dnsDoc, file);
  file.close();
}

void loadDNS() {
  File file = SPIFFS.open("/dns.json", "r");
  if (!file) return;
  DeserializationError error = deserializeJson(dnsDoc, file);
  file.close();
}

IPAddress resolveDomain(const String& domain) {
  if (dnsDoc.containsKey(domain)) {
    IPAddress ip;
    ip.fromString(dnsDoc[domain].as<String>());
    return ip;
  }
  return local_IP;
}

void setup() {
  Serial.begin(115200);
  SPIFFS.begin();
  loadIPConfig();
  loadDNS();

  WiFi.mode(WIFI_STA);
  WiFi.config(local_IP, gateway, subnet);
  WiFi.begin("YOUR_SSID", "YOUR_PASSWORD");
  while (WiFi.status() != WL_CONNECTED) delay(500);

  dnsServer.start(DNS_PORT, "*", local_IP);

  server.on("/", HTTP_GET, []() {
    if (!server.authenticate(www_username, www_password)) return server.requestAuthentication();
    File file = SPIFFS.open("/index.html", "r");
    server.streamFile(file, "text/html");
    file.close();
  });

  server.on("/add", HTTP_POST, []() {
    if (!server.authenticate(www_username, www_password)) return server.requestAuthentication();
    dnsDoc[server.arg("domain")] = server.arg("ip");
    saveDNS();
    server.send(200, "text/plain", "Added");
  });

  server.on("/delete", HTTP_POST, []() {
    if (!server.authenticate(www_username, www_password)) return server.requestAuthentication();
    dnsDoc.remove(server.arg("domain"));
    saveDNS();
    server.send(200, "text/plain", "Deleted");
  });

  server.on("/list", HTTP_GET, []() {
    if (!server.authenticate(www_username, www_password)) return server.requestAuthentication();
    String output;
    serializeJsonPretty(dnsDoc, output);
    server.send(200, "application/json", output);
  });

  server.on("/setip", HTTP_POST, []() {
    if (!server.authenticate(www_username, www_password)) return server.requestAuthentication();
    saveIPConfig(server.arg("ip"), server.arg("gateway"), server.arg("subnet"));
    server.send(200, "text/plain", "IP settings saved. Reboot to apply.");
  });

  server.begin();
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
}