#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP_Mail_Client.h>
#include <ESP8266mDNS.h>
#include <lwip/napt.h>
#include <lwip/dns.h>

// --- Konfiguration ---
#define AP_SSID "ESP-Mail-Setup"
#define AP_PASS "esp12345678"

#define NAPT 1000
#define NAPT_PORT 10

DNSServer dnsServer;
ESP8266WebServer server(80);
SMTPSession smtp;

// --- Status ---
bool wifiConnected = false;
String sta_ssid = "";
String sta_pass = "";

// --- Setup ---
void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  delay(100);

  IPAddress myIP = WiFi.softAPIP();
  dnsServer.start(53, "*", myIP);

  server.on("/", handleRoot);
  server.on("/setup", handleSetup);
  server.on("/send", handleEmailSend);
  server.begin();

  Serial.println("📡 AP gestartet: " + myIP.toString());
}

// --- Loop ---
void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  MDNS.update();
}

// --- Web: Startseite ---
void handleRoot() {
  if (!wifiConnected) {
    showWifiForm();
  } else {
    showMailForm();
  }
}

// --- Web: WLAN-Daten empfangen ---
void handleSetup() {
  sta_ssid = server.arg("ssid");
  sta_pass = server.arg("pass");

  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(sta_ssid.c_str(), sta_pass.c_str());

  server.send(200, "text/html", "<h1>⏳ Verbindung wird aufgebaut...</h1><meta http-equiv='refresh' content='5'>");

  Serial.println("🔁 Verbinde mit: " + sta_ssid);

  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 20) {
    delay(500);
    Serial.print(".");
    tries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("\n✅ WLAN verbunden!");
    Serial.print("STA IP: ");
    Serial.println(WiFi.localIP());

    // DNS weitergeben
    WiFi.softAPDhcpServer().setDns(WiFi.dnsIP(0));

    // NAT aktivieren
    if (ip_napt_init(NAPT, NAPT_PORT) == ERR_OK && ip_napt_enable_no(SOFTAP_IF, 1) == ERR_OK) {
      Serial.println("✅ NAT aktiviert – AP hat Internetzugang!");
    } else {
      Serial.println("❌ NAT-Fehler");
    }

    // mDNS starten
    if (MDNS.begin("esp8266")) {
      MDNS.addService("http", "tcp", 80);
      Serial.println("🌐 mDNS aktiv (esp8266.local)");
    }

    showMailForm();
  } else {
    server.send(200, "text/html", "<h1>❌ Verbindung fehlgeschlagen</h1><a href='/'>🔁 Zurück</a>");
  }
}

// --- Web: Formularanzeige für WLAN ---
void showWifiForm() {
  server.send(200, "text/html", R"====(
  <h1>🔌 WLAN-Konfiguration</h1>
  <form action="/setup" method="get">
    <label>SSID:</label><br>
    <input name="ssid"><br>
    <label>Passwort:</label><br>
    <input name="pass" type="password"><br>
    <button type="submit">✅ Verbinden</button>
  </form>
  <style>
    body { font-family: sans-serif; background: #1e1e2f; color: #eee; padding: 20px; }
    input, button { width: 100%; padding: 10px; margin-top: 10px; background: #333; color: white; border: none; border-radius: 5px; }
    button { background: #4fc3f7; font-weight: bold; cursor: pointer; }
  </style>
  )====");
}

// --- Web: Formularanzeige für Mail ---
void showMailForm() {
  server.send(200, "text/html", R"====(
<h1>ESP Mail Workspace</h1>
<form action="/send" method="get">
  <label>Empfänger-Adresse:</label>
  <input type="email" name="to" required>
  <label>Betreff:</label>
  <input type="text" name="subject">
  <label>Inhalt:</label>
  <textarea name="body" rows="5"></textarea>
  <label>SMTP-Dienst:</label>
  <select name="host">
    <option value="smtp.gmail.com">Gmail</option>
    <option value="smtp.mail.yahoo.com">Yahoo</option>
    <option value="smtp.office365.com">Outlook</option>
    <option value="mail.gmx.net">GMX</option>
    <option value="smtp.strato.de">Strato</option>
  </select>
  <label>Port:</label>
  <select name="port">
    <option value="587">587 (STARTTLS)</option>
    <option value="465">465 (SSL/TLS)</option>
  </select>
  <button type="submit">📤 Senden</button>
</form>
<style>
body { font-family: 'Segoe UI', sans-serif; background: #1e1e2f; color: #eee; margin: 0; padding: 20px; }
h1 { color: #4fc3f7; text-align: center; }
form { max-width: 500px; margin: auto; background: #2a2a40; padding: 20px; border-radius: 10px; }
input, textarea, select { width: 100%; padding: 10px; margin: 10px 0; background: #333; border: none; color: #fff; border-radius: 5px; }
button { background: #4fc3f7; border: none; padding: 10px 20px; color: #000; border-radius: 5px; font-weight: bold; font-size: 16px; cursor: pointer; }
button:hover { background: #81d4fa; }
.success { color: #00e676; margin: 20px 0; text-align: center; }
.error { color: #ff5252; margin: 20px 0; text-align: center; }
a { color: #81d4fa; text-decoration: none; display: block; text-align: center; margin-top: 20px; }
</style>
)====");
}

// --- Web: Mailversand ---
void handleEmailSend() {
  String to = server.arg("to");
  String subject = server.arg("subject");
  String content = server.arg("body");
  String host = server.arg("host");
  int port = server.arg("port").toInt();

  Session_Config config;
  config.server.host_name = host;
  config.server.port = port;
  config.login.email = "familydimakis@gmail.com";  // Nur zu Testzwecken!
  config.login.password = "bimc mhoc uilj txdt";   // Nutze App-spezifisches Passwort!

  SMTP_Message msg;
  msg.sender.name = "ESP Citrix Mail";
  msg.sender.email = config.login.email;
  msg.subject = subject;
  msg.addRecipient("Empfänger", to);
  msg.text.content = content;
  msg.text.charSet = "utf-8";
  msg.text.transfer_encoding = "7bit";

  smtp.connect(&config);
  bool ok = MailClient.sendMail(&smtp, &msg);

  String response = "<h1>Mailversand</h1>";
  if (ok) {
    response += "<div class='success'>✅ E-Mail gesendet!</div>";
  } else {
    response += "<div class='error'>❌ Fehler beim Senden:</div><pre>" + smtp.errorReason() + "</pre>";
  }
  response += "<a href='/'>🔁 Neue Mail senden</a>";
  server.send(200, "text/html", response);
}
