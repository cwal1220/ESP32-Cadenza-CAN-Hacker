#include <mcp_can.h>
#include <SPI.h>

#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <ESPmDNS.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

void setup();
void loop();
void setup_runtime();
void setup_captive();
void button();
void SaveString(int startAt, const char *id);
void ReadString(byte startAt, byte bufor);
void handleNotFound();
ICACHE_RAM_ATTR void initDevice();
void setup_can();
bool readCanData(uint32_t *rxId, uint8_t *len, uint8_t *rxBuf);


#define EEPROM_LENGTH 1024
#define MQTT_TOPIC "CADENZA/CCAN_RX"
#define BOOT_PIN 9

// MCP2515 Setting
#define CAN0_INT_PIN 3
#define CAN0_CS_PIN 7
MCP_CAN CAN0(CAN0_CS_PIN);
// MCP2515 End

const char *mqttServer = "138.2.126.137";
const int mqttPort = 1883;
const char *mqttUser = "chan";
const char *mqttPassword = "chan";

char eRead[30];
byte len;
char ssid[30];
char password[30];

bool captive = true;

WiFiClient espClient;
PubSubClient client(espClient);
const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 1, 1);
DNSServer dnsServer;
WebServer webServer(80);

String responseHTML = ""
                      "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><title>Wi-Fi Setting Page</title></head><body><center>"
                      "<p>Wi-Fi Setting Page</p>"
                      "<form action='/button'>"
                      "<p><input type='text' name='ssid' placeholder='SSID' onblur='this.value=removeSpaces(this.value);'></p>"
                      "<p><input type='text' name='password' placeholder='WLAN Password'></p>"
                      "<p><input type='submit' value='Submit'></p></form>"
                      "<p>Wi-Fi Setting Page</p></center></body>"
                      "<script>function removeSpaces(string) {"
                      "   return string.split(' ').join('');"
                      "}</script></html>";

void setup()
{
    Serial.begin(115200);
    EEPROM.begin(EEPROM_LENGTH);
    pinMode(0, INPUT_PULLUP);
    attachInterrupt(BOOT_PIN, initDevice, FALLING);

    ReadString(0, 30);
    if (!strcmp(eRead, ""))
    {
        setup_captive();
    }
    else
    {
        captive = false;
        strcpy(ssid, eRead);
        ReadString(30, 30);
        strcpy(password, eRead);

        setup_runtime();
        client.setServer(mqttServer, mqttPort);
        while (!client.connected())
        {
            Serial.println("Connecting to MQTT...");
            if (client.connect("CAN_HACKER", mqttUser, mqttPassword))
            {
                Serial.println("connected");
                setup_can();
            }
            else
            {
                Serial.print("failed with state ");
                Serial.println(client.state());
                ESP.restart();
            }
        }
    }
}

void loop()
{
    if (captive)
    {
        dnsServer.processNextRequest();
        webServer.handleClient();
    }
    else
    {
        String jsonString;
        StaticJsonDocument<200> canDataJson;

        uint32_t rxId;
        uint8_t rxLen = 0;
        uint8_t rxBuf[8] = {0x00,};
        char dataString[64] = {'\0',};

        if(readCanData(&rxId, &rxLen, rxBuf))
        {
            for (byte i = 0; i < rxLen; i++)
            {
                sprintf(&dataString[i*5], "0x%.2X ", rxBuf[i]);
            }

            canDataJson["id"] = rxId;
            canDataJson["len"] = rxLen;
            canDataJson["data"] = dataString;

            serializeJson(canDataJson, jsonString);
            client.publish(MQTT_TOPIC, jsonString.c_str());
            // Debug
            Serial.println(jsonString.c_str());
        }
    }
    client.loop();
}

void setup_runtime()
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.println("");

    // Wait for connection
    int i = 0;
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(1000);
        Serial.print(".");
        yield;
        if (i++ > 20)
        {
            ESP.restart();
            return;
        }
    }
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    if (MDNS.begin("CAN_HACKER"))
    {
        Serial.println("MDNS responder started");
    }

    webServer.onNotFound(handleNotFound);
    webServer.begin();
    Serial.println("HTTP server started");
}

void setup_captive()
{
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP("CAN_HACKER");

    dnsServer.start(DNS_PORT, "*", apIP);

    webServer.on("/button", button);

    webServer.onNotFound([]()
                         { webServer.send(200, "text/html", responseHTML); });
    webServer.begin();
    Serial.println("Captive Portal Started");
}

void button()
{
    Serial.println("button pressed");
    Serial.println(webServer.arg("ssid"));
    SaveString(0, (webServer.arg("ssid")).c_str());
    SaveString(30, (webServer.arg("password")).c_str());
    webServer.send(200, "text/plain", "OK");
    ESP.restart();
}

void SaveString(int startAt, const char *id)
{
    for (byte i = 0; i <= strlen(id); i++)
    {
        EEPROM.write(i + startAt, (uint8_t)id[i]);
    }
    EEPROM.commit();
}

void ReadString(byte startAt, byte bufor)
{
    for (byte i = 0; i <= bufor; i++)
    {
        eRead[i] = (char)EEPROM.read(i + startAt);
    }
    len = bufor;
}

void handleNotFound()
{
    String message = "File Not Found\n";
    webServer.send(404, "text/plain", message);
}

ICACHE_RAM_ATTR void initDevice()
{
    Serial.println("Flushing EEPROM....");
    SaveString(0, "");
    ESP.restart();
}

void setup_can()
{
    // Initialize MCP2515 running at 16MHz with a baudrate of 500kb/s and the masks and filters disabled.
    if (CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK)
    {
        Serial.println("MCP2515 Initialized Successfully!");
    }
    else
    {
        Serial.println("Error Initializing MCP2515...");
    }

    CAN0.setMode(MCP_NORMAL); // Set operation mode to normal so the MCP2515 sends acks to received data.
    pinMode(CAN0_INT_PIN, INPUT); // Configuring pin for /INT input
    Serial.println("MCP2515 Initialize Done!");
}

bool readCanData(uint32_t *rxId, uint8_t *rxLen, uint8_t *rxBuf)
{
    if (!digitalRead(CAN0_INT_PIN)) // If CAN0_INT_PIN pin is low, read receive buffer
    {
        CAN0.readMsgBuf(rxId, rxLen, rxBuf);
        return true;
    }
    else
    {
        return false;
    }
}
