/*******************************************************************
    Getting stats from Instagram and using the second core
    of the ESP32 to drive a MAX7219 LED Matrix

    Written by Brian Lough
    https://www.youtube.com/channel/UCezJOfu7OtqGzd5xrP3q6WA
 *******************************************************************/

// ----------------------------
// Standard Libraries - Already Installed if you have ESP32 set up
// ----------------------------

#include <SPI.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>


#include "secret.h"

// ----------------------------
// Additional Libraries - each one of these will need to be installed.
// ----------------------------

#include "LedMatrix.h"
// The driver for the LED Matrix Display
// Using my fork of Daniel Eichorn's library (For support for software SPI and rotation)
// https://github.com/witnessmenow/MAX7219LedMatrix

#include "InstagramStats.h"
// Fetches follower count from Instagram
// Should be available on the library manager (search for "Instagram"), if not download from Github
// https://github.com/witnessmenow/arduino-instagram-stats

#include <YoutubeApi.h>
// Fetches YouTube API data
// Should be available on the library manager (search for "YouTube")

#include <InstructablesApi.h>

#include <CoinMarketCapApi.h>

#include <UniversalTelegramBot.h>

#include <GoogleMapsApi.h>

#include <ArduinoJson.h>

#include "JsonStreamingParser.h"
// Required by the Instagram library, used for parsing large JSON payloads
// Available on the library manager (search for "Json Streaming Parser" by Daniel Eichorn)
// https://github.com/squix78/json-streaming-parser

//------- Replace the following! ------
char ssid[] = WIFI_NAME;       // your network SSID (name)
char password[] = WIFI_PASS;  // your network key

//Inputs
String userName = "brian_lough"; // from their instagram url https://www.instagram.com/brian_lough/


WiFiClientSecure client;
WiFiClient unsecureClient;

InstagramStats instaStats(client);

YoutubeApi api(API_KEY, client);

InstructablesApi instructablesApi(unsecureClient);

CoinMarketCapApi cryptoApi(client);

UniversalTelegramBot bot(TELEGRAM_BOT_TOKEN, client);

GoogleMapsApi mapsApi(MAPS_API_KEY, client);

#define CHANGE_MODE_BUTTON_PIN 0

unsigned long delayBetweenChecks = 60000; //mean time between api requests
unsigned long whenDueToCheck = 0;

int currentMode = 0;
#define MAX_MODES 5
// ------ LED Matrix ------

#define NUMBER_OF_DEVICES 4

// Wiring that works with ESP32
#define CS_PIN 15
#define CLK_PIN 14
#define MISO_PIN 2 //we do not use this pin just fill to match constructor
#define MOSI_PIN 12

#define CHANNEL_ID "UCezJOfu7OtqGzd5xrP3q6WA"
#define SCREEN_NAME "witnessmenow"

LedMatrix ledMatrix = LedMatrix(NUMBER_OF_DEVICES, CLK_PIN, MISO_PIN, MOSI_PIN, CS_PIN);

bool newData = false;
int instagramFollowers = 0;
int youtubeSubs = 0;
int instructablesFollowers = 0;

int bitcoinPrice = 0;
int ethPrice = 0;

String telegramMessage;

TaskHandle_t Task1;

void driveDisplay(void * parameter) {
  while (true) {
    ledMatrix.clear();
    ledMatrix.scrollTextLeft();
    ledMatrix.drawText();
    ledMatrix.commit();
    delay(35);

    if (newData) {
      newData = false;
      switch (currentMode) {
        case 0:
          ledMatrix.setNextText("Instagram: " + String(instagramFollowers));
          break;
        case 1:
          ledMatrix.setNextText("YouTube: " + String(youtubeSubs));
          break;
        case 2:
          ledMatrix.setNextText("Instructables: " + String(instructablesFollowers));
          break;
        case 3:
          ledMatrix.setNextText("BTC: $" + String(bitcoinPrice) + "   ETH: $" + String(ethPrice));
          break;
        case 4:
          ledMatrix.setText(telegramMessage);
          break;
        default:
          break;
      }
    }
  }
}

void setup() {

  Serial.begin(115200);

  pinMode(CHANGE_MODE_BUTTON_PIN, INPUT_PULLUP);

  ledMatrix.init();
  ledMatrix.setRotation(true);
  ledMatrix.setText("API Demo Display");
  ledMatrix.setIntensity(8);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  IPAddress ip = WiFi.localIP();
  Serial.println(ip);

  xTaskCreatePinnedToCore(
    driveDisplay,            /* Task function. */
    "DisplayDrive",                 /* name of task. */
    1000,                    /* Stack size of task */
    NULL,                     /* parameter of the task */
    1,                        /* priority of the task */
    &Task1,                   /* Task handle to keep track of created task */
    0);                       /* Core */
}

void getInstagramStatsForUser() {
  Serial.println("Getting instagram user stats for " + userName );
  InstagramUserStats response = instaStats.getUserStats(userName);
  Serial.println("Response:");
  Serial.print("Number of followers: ");
  Serial.println(response.followedByCount);

  newData = true;

  instagramFollowers = response.followedByCount;
}

void getYoutubeStats() {
  if (api.getChannelStatistics(CHANNEL_ID))
  {
    Serial.println("---------Stats---------");
    Serial.print("Subscriber Count: ");
    Serial.println(api.channelStats.subscriberCount);
    Serial.println("------------------------");

    youtubeSubs = api.channelStats.subscriberCount;

    newData = true;

  }
}

void getInstructablesStats() {
  instructablesAuthorStats stats;
  stats = instructablesApi.getAuthorStats(SCREEN_NAME);
  if (stats.error.equals(""))
  {
    Serial.println("---------Author Stats---------");
    Serial.print("Followers: ");
    Serial.println(stats.followersCount);
    Serial.println("------------------------");

    instructablesFollowers = stats.followersCount;

    newData = true;
  }
}

void getCryptoStats() {
  CMCTickerResponse response = cryptoApi.GetTickerInfo("bitcoin", "usd");
  if (response.error == "")
  {
    bitcoinPrice = response.price_usd;
  }

  delay(500);

  response = cryptoApi.GetTickerInfo("ethereum", "usd");
  if (response.error == "")
  {
    ethPrice = response.price_usd;
  }

  newData = true;
}

String getTravelTime(String origin) {
  String responseString = mapsApi.distanceMatrix(origin, "Galway,Ireland", "now", "best_guess");
  DynamicJsonBuffer jsonBuffer;
  JsonObject& response = jsonBuffer.parseObject(responseString);
  if (response.success()) {
    if (response.containsKey("rows")) {
      JsonObject& element = response["rows"][0]["elements"][0];
      String status = element["status"];
      if (status == "OK") {

        String distance = element["distance"]["text"];
        String duration = element["duration"]["text"];
        String durationInTraffic = element["duration_in_traffic"]["text"];

        return durationInTraffic;

      }
      else {
        Serial.println("Got an error status: " + status);
      }
    } else {
      Serial.println("Reponse did not contain rows");
    }
  } else {
    Serial.println("Failed to parse Json");
  }

  return "Error getting TravelTime";
}

void getTelegramData() {
  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  if (numNewMessages) {
    if (bot.messages[0].type == "callback_query") {
      telegramMessage = bot.messages[0].text;
      newData = true;
    } else if (bot.messages[0].longitude != 0 || bot.messages[0].latitude != 0) {
      String origin = String(bot.messages[0].latitude, 7) + "," + String(bot.messages[0].longitude, 7);
      Serial.println(origin);
      telegramMessage = getTravelTime(origin);
      newData = true;
    } else
    {
      String text = bot.messages[0].text;
      String chat_id = String(bot.messages[0].chat_id);
      if (text == "/options") {
        String keyboardJson = "[[{ \"text\" : \"This button will say Hi\", \"callback_data\" : \"Hi\" }],[{ \"text\" : \"This will say Bye\", \"callback_data\" : \"Bye\" }]]";
        bot.sendMessageWithInlineKeyboard(chat_id, "Choose from one of the following options", "", keyboardJson);
      } else {
        telegramMessage = text;
        newData = true;
      }
    }
  }
}

unsigned long buttonDueCheck = 0;

void loop() {
  unsigned long timeNow = millis();
  if (timeNow > buttonDueCheck) {
    if (digitalRead(CHANGE_MODE_BUTTON_PIN) == LOW) {
      Serial.println("ButtonPress");
      currentMode ++;
      if (currentMode >= MAX_MODES)
      {
        currentMode = 0;
      }
      whenDueToCheck = 0;
      buttonDueCheck = timeNow + 500;
      if (currentMode == 4) {
        ledMatrix.setText("Waiting For Telegram");
      } else {
        ledMatrix.setText("...");
      }
    }
  }

  timeNow = millis();
  if ((timeNow > whenDueToCheck))  {
    switch (currentMode) {
      case 0:
        getInstagramStatsForUser();
        break;
      case 1:
        getYoutubeStats();
        break;
      case 2:
        getInstructablesStats();
        break;
      case 3:
        getCryptoStats();
        break;
      case 4:
        getTelegramData();
        break;
      default:
        break;
    }
    // Need to check Telegram more frequently
    if (currentMode == 4) {
      whenDueToCheck = timeNow + 1000;
    } else {
      whenDueToCheck = timeNow + delayBetweenChecks;
    }
  }
}
