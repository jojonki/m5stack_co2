#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <M5Stack.h>
#include <SD.h>
#include <SparkFunCCS811.h>  //Click here to get the library: http://librarymanager/All#SparkFun_CCS811
#include <WiFi.h>

#define CCS811_ADDR 0x5B  // Default I2C Address
//#define CCS811_ADDR 0x5A //Alternate I2C Address
CCS811 myCCS811(CCS811_ADDR);

const char* wifi_conf_path = "/wifi.txt";

// Notify Line
char host[255];
char buffer[255];
const int capacity = JSON_OBJECT_SIZE(2);
StaticJsonDocument<capacity> json_request;
unsigned long last_line_push;             // time stamp of notification
const uint16_t LINE_PUSH_INTERVAL = 300;  // 5 minutes
const uint16_t CO2_TH = 1500;             // ppm

void exit() {
    while (1)
        ;
}
char* strToChar(String str) { return const_cast<char*>(str.c_str()); }

void notifyLine(uint16_t co2, uint16_t tvoc) {
    Serial.println("Notify LINE");
    Serial.print("CO2: ");
    Serial.println(co2);
    Serial.print("TVOC: ");
    Serial.println(tvoc);
    json_request["value1"] = co2;
    json_request["value2"] = tvoc;

    serializeJson(json_request, Serial);
    // Serial.println("");

    serializeJson(json_request, buffer, sizeof(buffer));

    HTTPClient http;
    http.begin(host);
    http.addHeader("Content-Type", "application/json");
    int status_code = http.POST((uint8_t*)buffer, strlen(buffer));
    Serial.printf("status_code=%d\r\n", status_code);
    if (status_code == 200) {
        Stream* resp = http.getStreamPtr();

        DynamicJsonDocument json_response(255);
        deserializeJson(json_response, *resp);

        serializeJson(json_response, Serial);
        Serial.println("");
    }
    http.end();
}

void updateSensor() {
    uint16_t bg_color;
    uint16_t co2 = myCCS811.getCO2();
    uint16_t tvoc = myCCS811.getTVOC();

    if (co2 > CO2_TH) {
        unsigned long now = millis() / 1000;
        if ((now - last_line_push) > LINE_PUSH_INTERVAL) {
            notifyLine(co2, tvoc);
            last_line_push = now;
        }

        bg_color = M5.Lcd.color565(200, 120, 0);
    } else {
        bg_color = BLACK;
    }
    M5.Lcd.fillScreen(bg_color);
    M5.Lcd.setTextColor(WHITE, bg_color);

    Serial.print(co2);
    Serial.println(" ppm");

    // M5.Lcd.println("CCS811 data:");

    M5.Lcd.print("CO2  : ");
    M5.Lcd.print(co2);
    M5.Lcd.println(" ppm");
    M5.Lcd.print("TVOC : ");
    M5.Lcd.print(tvoc);
    M5.Lcd.println(" ppb");
}

void printSensorError() {
    uint8_t error = myCCS811.getErrorRegister();

    if (error == 0xFF)  // comm error
    {
        Serial.println("Failed to get ERROR_ID register.");
    } else {
        Serial.print("Error: ");
        if (error & 1 << 5) Serial.print("HeaterSupply");
        if (error & 1 << 4) Serial.print("HeaterFault");
        if (error & 1 << 3) Serial.print("MaxResistance");
        if (error & 1 << 2) Serial.print("MeasModeInvalid");
        if (error & 1 << 1) Serial.print("ReadRegInvalid");
        if (error & 1 << 0) Serial.print("MsgInvalid");
        Serial.println("");
    }
}

void setup() {
    M5.begin();
    M5.Power.begin();
    M5.Lcd.setBrightness(5);
    M5.Lcd.setTextSize(3);
    M5.Lcd.setCursor(0, 0);

    Serial.begin(115200);
    Wire.begin();

    if (!SD.begin()) {
        Serial.println("SD Card Mount Failed");
        exit();
    }

    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("No SD card attached");
        exit();
    }
    File file = SD.open(wifi_conf_path);
    if (!file) {
        Serial.printf("Failed to open %s", wifi_conf_path);
        exit();
    }

    Serial.println("Read Wifi info from SD Card.");
    uint8_t line_ct = 1;
    String ssid_str, pass_str, host_str;
    while (file.available() && line_ct <= 3) {
        if (line_ct == 1) {
            ssid_str = file.readStringUntil('\n');
            Serial.print("SSID: ");
            Serial.println(ssid_str);
        } else if (line_ct == 2) {
            pass_str = file.readStringUntil('\n');
            // Serial.print("pass: ");
            // Serial.println(pass_str);
        } else if (line_ct == 3) {
            host_str = file.readStringUntil('\n');
            host_str.toCharArray(host, sizeof(host));
        }
        line_ct++;
    }
    file.close();

    WiFi.begin(strToChar(ssid_str), strToChar(pass_str));
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        M5.Lcd.print(".");
    }

    M5.Lcd.println("WiFi connected");
    M5.Lcd.print("IP address = ");
    M5.Lcd.println(WiFi.localIP());

    if (myCCS811.begin() == false) {
        Serial.print("CCS811 error. Please check wiring. Freezing...");
        M5.Lcd.print("CCS811 error. Please check wiring. Freezing...");
        while (1)
            ;
    }

    last_line_push = millis() / 1000 - LINE_PUSH_INTERVAL;
}
//---------------------------------------------------------------
void loop() {
    //  M5.Lcd.setCursor(0,0);
    if (myCCS811.dataAvailable()) {
        M5.Lcd.clear();
        M5.Lcd.setCursor(0, 100);
        myCCS811.readAlgorithmResults();
        updateSensor();
    } else if (myCCS811.checkForStatusError()) {
        printSensorError();
    }

    delay(5000);
}