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
StaticJsonDocument<256> authDoc;

IPAddress local_IP, gateway, subnet;
String www_username, www_password;

void loadIPConfig() {
  File file = SPIFFS.open("/netconfig.json", "r");
  if (!file) return;
  deserializeJson(netDoc, file);
  file.close();
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

void loadAuth() {
  File file = SPIFFS.open("/auth.json", "r");
  if (!file) return;
  deserializeJson(authDoc, file);
  file.close();
  www_username = authDoc["user"].as<String>();
  www_password = authDoc["pass"].as<String>();
}

void saveAuth(String user, String pass) {
  authDoc["user"] = user;
  authDoc["pass"] = pass;
  File file = SPIFFS.open("/auth.json", "w");
  serializeJson(authDoc, file);
  file.close();
  www_username = user;
  www_password = pass;
}

void saveDNS() {
  File file = SPIFFS.open("/dns.json", "w");
  serializeJson(dnsDoc, file);
  file.close();
}

void loadDNS() {
  File file = SPIFFS.open("/dns.json", "r");
  if (!file) return;
  deserializeJson(dnsDoc, file);
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
  loadAuth();

  WiFi.mode(WIFI_STA);
  WiFi.config(local_IP, gateway, subnet);
  WiFi.begin("YOUR_SSID", "YOUR_PASSWORD");
  while (WiFi.status() != WL_CONNECTED) delay(500);

  dnsServer.start(DNS_PORT, "*", local_IP);

  server.on("/", HTTP_GET, []() {
    if (!server.authenticate(www_username.c_str(), www_password.c_str())) return server.requestAuthentication();
    File file = SPIFFS.open("/index.html", "r");
    server.streamFile(file, "text/html");
    file.close();
  });

  server.on("/add", HTTP_POST, []() {
    if (!server.authenticate(www_username.c_str(), www_password.c_str())) return server.requestAuthentication();
    dnsDoc[server.arg("domain")] = server.arg("ip");
    saveDNS();
    server.send(200, "text/plain", "Added");
  });

  server.on("/delete", HTTP_POST, []() {
    if (!server.authenticate(www_username.c_str(), www_password.c_str())) return server.requestAuthentication();
    dnsDoc.remove(server.arg("domain"));
    saveDNS();
    server.send(200, "text/plain", "Deleted");
  });

  server.on("/list", HTTP_GET, []() {
    if (!server.authenticate(www_username.c_str(), www_password.c_str())) return server.requestAuthentication();
    String output;
    serializeJsonPretty(dnsDoc, output);
    server.send(200, "application/json", output);
  });

  server.on("/setip", HTTP_POST, []() {
    if (!server.authenticate(www_username.c_str(), www_password.c_str())) return server.requestAuthentication();
    saveIPConfig(server.arg("ip"), server.arg("gateway"), server.arg("subnet"));
    server.send(200, "text/plain", "IP settings saved. Reboot to apply.");
  });

  server.on("/setauth", HTTP_POST, []() {
    if (!server.authenticate(www_username.c_str(), www_password.c_str())) return server.requestAuthentication();
    saveAuth(server.arg("user"), server.arg("pass"));
    server.send(200, "text/plain", "Credentials updated. Use new login next time.");
  });

  server.begin();
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
}