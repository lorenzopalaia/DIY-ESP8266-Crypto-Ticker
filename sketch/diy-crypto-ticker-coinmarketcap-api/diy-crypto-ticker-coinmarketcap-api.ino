#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <U8g2lib.h>

// config
const char* SSID = "";
const char* PASSWORD = "";

const String API_KEY = "";

const char* SYMBOLS[] = {"BTC",
                         "ETH",
                         "BNB"
                         "SOL",
                         "DOT",
                         "AVAX",
                         "LUNA",
                         "AXS",
                         "EGLD",
                         "ONE",
                         "MATIC",
                         "FTM",
                         "MANA"};
                         
const char* CONVERT_TO = "USD";

const long DEFAULT_DELAY = 1000 * 60 * 5; // wait 5 minutes between API request; +info https://pro.coinmarketcap.com/account/plan

const int JSON_DOCUMENT_SIZE = 3072; //calculated by ArduinoJson calculator

// network
WiFiClientSecure client;

const char* HOST = "pro-api.coinmarketcap.com";
const int HTTPS_PORT = 443;

// presentation
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

const int SEPARATOR = 2;
const String PERCENTAGE = "%";

int x = 0;
int y = 0;

String symbol = "BTC";
int symbolsCurrentPosition = -1;

String name = "";
float price = 0;
float percent24h = 0;
float volume = 0;
float cap = 0;
String lastUpdated = "";

void setup() {
  setupWifi();
  setupDisplay();
}

void loop() {
  if (getInfo()) {
    showInfo();
    // showInfoOnSerial();
  }
  delay(DEFAULT_DELAY);
}

void setupWifi() {  
  Serial.begin(115200);
  delay(100);
  Serial.print("Connecting to ");
  Serial.println(SSID);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected!");

  client.setInsecure();
}

void setupDisplay() {
  u8g2.begin();
  u8g2.setFontMode(0);
}

bool getInfo() {
  if (!client.connect(HOST, HTTPS_PORT)) {
    Serial.println("Connection failed!");
    return false;
  }

  symbol = getSymbol();
  String endpoint = "/v1/cryptocurrency/quotes/latest?CMC_PRO_API_KEY=" + API_KEY + "&symbol=" + symbol  + "&convert=" + CONVERT_TO;

  // Serial.print("Requesting: ");
  // Serial.println(endpoint);

  client.print(String("GET ") + endpoint + " HTTP/1.1\r\n" +
               "Host: " + HOST + "\r\n" +
               "User-Agent: ESP8266\r\n" +
               "Connection: close\r\n\r\n");

  if (client.println() == 0) {
    Serial.println("Failed to send request");
    return false;
  }

  // check HTTP response
  char httpStatus[32] = {0};
  client.readBytesUntil('\r', httpStatus, sizeof(httpStatus));
  if (strcmp(httpStatus, "HTTP/1.1 200 OK") != 0) {
    Serial.print("Unexpected response: ");
    Serial.println(httpStatus);
    return false;
  }

  // skip HTTP headers
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    //Serial.println(line);
    if (line == "\r") {
      break;
    }
  }

  // skip content length
  if (client.connected()) {
    String line = client.readStringUntil('\n');
    //Serial.println(line);
  }

  // get response
  String response = "";
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    //Serial.println(line);
    line.trim();
    if (line != "\r") {
      response += line;
    }
  }

  client.stop();

  // parse response
  DynamicJsonDocument jsonDocument(JSON_DOCUMENT_SIZE);
  DeserializationError error = deserializeJson(jsonDocument, response);
  if (error) {
    Serial.println("Deserialization failed");
    return false;
  }
  JsonObject root = jsonDocument.as<JsonObject>();

  // check API status
  JsonObject status = root["status"];
  int statusErrorCode = status["error_code"];
  if (statusErrorCode != 0) {
    String statusErrorMessage = status["error_message"];
    Serial.print("Error: ");
    Serial.println(statusErrorMessage);
    delay(DEFAULT_DELAY);
    return false;
  }

  JsonObject coin = root["data"][symbol];
  name = coin["name"].as<String>();
  lastUpdated = coin["last_updated"].as<String>();

  JsonObject quote = coin["quote"][CONVERT_TO];
  price = quote["price"];
  percent24h = quote["percent_change_24h"];
  volume = quote["volume_24h"];
  cap = quote["market_cap"];

  return true;
}

String getSymbol() {
  if (symbolsCurrentPosition == -1 || symbolsCurrentPosition + 1 > (sizeof(SYMBOLS) / sizeof(SYMBOLS[0])) - 1) {
    symbolsCurrentPosition = 0;
  } else {
    symbolsCurrentPosition++;
  }
  return SYMBOLS[symbolsCurrentPosition];
}

void showInfo() {
  u8g2.firstPage(); // + info https://github.com/olikraus/u8glib/wiki/tpictureloop
  do {
    // symbol
    u8g2.setFont(u8g2_font_fub14_tf);
    x = (128 - getStringWidth(symbol)) / 2;
    y = 14; // from below, capital A size. +info https://github.com/olikraus/u8g2/wiki/fntgrpfreeuniversal
    u8g2print(symbol);

    // price
    u8g2.setFont(u8g2_font_fub11_tf);
    String priceFormatted = String(price, 2);
    x = (128 - getStringWidth(priceFormatted) - SEPARATOR - getStringWidth(CONVERT_TO)) / 2;
    y += 11 + SEPARATOR;
    u8g2print(priceFormatted);
    x += getStringWidth(priceFormatted) + SEPARATOR;
    u8g2.setFont(u8g2_font_fur11_tf);
    u8g2print(CONVERT_TO);

    // percentages
    String percent24hFormatted = String(percent24h, 1) + PERCENTAGE;

    u8g2.setFont(u8g2_font_fub11_tf);
    x = 0;
    y += 11 + SEPARATOR;
    u8g2print("24h:");

    x = 128 - getStringWidth(percent24hFormatted);
    u8g2print(percent24hFormatted);

    //x += getStringWidth(percent24hFormatted) + SEPARATOR;
    //u8g2print(percent7dFormatted);

    // cap
    u8g2.setFont(u8g2_font_profont15_tf);
    x = 0;
    y += 9 + SEPARATOR;
    String capFormatted = formatDollars(String(cap, 0));
    u8g2print("Mkt. Cap:");
    x = 128 - getStringWidth(capFormatted);
    u8g2print(capFormatted);

    // volume
    String volumeFormatted = formatDollars(String(volume, 0));
    u8g2.setFont(u8g2_font_profont15_tf);
    y += 9 + SEPARATOR;
    x = 0;
    u8g2print("Vol. 24h:");
    x = 128 - getStringWidth(volumeFormatted);
    u8g2print(volumeFormatted);
    
  } while ( u8g2.nextPage() );
}

int getStringWidth(String string) {
  char *charString = const_cast<char*>(string.c_str());
  return u8g2.getStrWidth(charString);
}

void u8g2print(String string) {
  u8g2.setCursor(x, y);
  u8g2.print(string);
}

String convertToTime(String timestamp) {
  return timestamp.substring(11, 16); // extract HH:mm
}

//Hardcoded for 15 digits length
String formatDollars(String usd) {
  String cp = usd;
  if (cp.length() % 3 == 0) {
    if (cp.length() == 3)
      return (cp + "$");
    else if (cp.length() == 6)
      return (cp.substring(0, 3) + "." + cp.substring(3, 5) + "K$");
    else if (cp.length() == 9)
      return (cp.substring(0, 3) + "." + cp.substring(3, 5) + "M$");
    else if (cp.length() == 12)
      return (cp.substring(0, 3) + "." + cp.substring(3, 5) + "B$"); 
    else if (cp.length() == 15)
      return (cp.substring(0, 3) + "." + cp.substring(3, 5) + "T$"); 
  }
  else if (cp.length() % 2== 0) {
    if (cp.length() == 2)
      return (cp + "$");
    else if (cp.length() == 4)
      return (cp.substring(0, 1) + "." + cp.substring(1, 3) + "K$");
    else if (cp.length() == 8)
      return (cp.substring(0, 2) + "." + cp.substring(2, 4) + "M$");
    else if (cp.length() == 10)
      return (cp.substring(0, 1) + "." + cp.substring(1, 3) + "B$");
    else if (cp.length() == 14)
      return (cp.substring(0, 2) + "." + cp.substring(2, 4) + "T$");
  }
  else {
    if (cp.length() == 1)
      return (cp + "$");
    else if (cp.length() == 5)
      return (cp.substring(0, 2) + "." + cp.substring(2, 4) + "K$");
    else if (cp.length() == 7)
      return (cp.substring(0, 1) + "." + cp.substring(1, 3) + "M$");
    else if (cp.length() == 11)
      return (cp.substring(0, 2) + "." + cp.substring(2, 4) + "B$");
    else if (cp.length() == 13)
      return (cp.substring(0, 1) + "." + cp.substring(1, 3) + "T$");
  }
  return cp;
}

void showInfoOnSerial() {
  Serial.print("Coin: ");
  Serial.println(symbol);
  Serial.print("Price: ");
  Serial.print(price);
  Serial.println("$");
  Serial.print("MarketCap: ");
  Serial.print(cap);
  Serial.println("$");
  Serial.print("Volume: ");
  Serial.print(volume);
  Serial.println("$");
  Serial.print("24h Change: ");
  Serial.print(percent24h);
  Serial.println("%");
}
