/*
  WS2812FX Webinterface.
  
  Harm Aldick - 2016
  www.aldick.org

  
  FEATURES
    * Webinterface with mode, color, speed and brightness selectors


  LICENSE

  The MIT License (MIT)

  Copyright (c) 2016  Harm Aldick 

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.

  
  CHANGELOG
  2016-11-26 initial version
  
*/
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WS2812FX.h>

#include "index.html.h"
#include "main.js.h"

#define WIFI_SSID "***"
#define WIFI_PASSWORD "***"

//#define STATIC_IP                       // uncomment for static IP, set IP below
#ifdef STATIC_IP
  IPAddress ip(192,168,0,123);
  IPAddress gateway(192,168,0,1);
  IPAddress subnet(255,255,255,0);
#endif

// QUICKFIX...See https://github.com/esp8266/Arduino/issues/263
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))

#define LED_PIN D6                       // 0 = GPIO0, 2=GPIO2
#define LED_COUNT 50

#define WIFI_TIMEOUT 5000              // checks WiFi every ...ms. Reset after this time, if WiFi cannot reconnect.
#define HTTP_PORT 80

#define DEFAULT_COLOR 0xFF5900
#define DEFAULT_BRIGHTNESS 255
#define DEFAULT_SPEED 200
#define DEFAULT_MODE FX_MODE_STATIC

#define BRIGHTNESS_STEP 15              // in/decrease brightness by this amount per click
#define SPEED_STEP 10                   // in/decrease brightness by this amount per click

#define TIMER_MS 15000

byte demo_modes[][2] = {
  {FX_MODE_COLOR_WIPE_RANDOM, 2},
  {FX_MODE_SINGLE_DYNAMIC, 3},
  {FX_MODE_MULTI_DYNAMIC, 3},
  {FX_MODE_RAINBOW, 2},
  {FX_MODE_RAINBOW_CYCLE, 2},
  {FX_MODE_SCAN, 2},
  {FX_MODE_FADE, 2},
  {FX_MODE_THEATER_CHASE_RAINBOW, 2},
  {FX_MODE_TWINKLE_RANDOM, 3},
  {FX_MODE_TWINKLE_FADE_RANDOM, 3},
  {FX_MODE_CHASE_RANDOM, 2},
  {FX_MODE_CHASE_RAINBOW, 2},
  {FX_MODE_CHASE_RAINBOW_WHITE, 2},
  {FX_MODE_CHASE_BLACKOUT_RAINBOW, 2},
  {FX_MODE_COLOR_SWEEP_RANDOM, 3},
  {FX_MODE_RUNNING_RANDOM, 2},
  {FX_MODE_LARSON_SCANNER, 2},
  {FX_MODE_COMET, 2},
  {FX_MODE_FIREWORKS_RANDOM, 3},  
  {FX_MODE_FIRE_FLICKER, 1},
  {FX_MODE_MERRY_CHRISTMAS, 3} 
  };

int demo_modes_count = sizeof(demo_modes)/2;

unsigned long last_change = 0;
unsigned int cur_mode = 0;
bool demo_mode = true;

unsigned long last_wifi_check_time = 0;
String modes = "";


WS2812FX ws2812fx = WS2812FX(LED_COUNT, LED_PIN, NEO_RGB + NEO_KHZ800);
ESP8266WebServer server = ESP8266WebServer(HTTP_PORT);


void setup(){
  Serial.begin(115200);
  Serial.println();
  Serial.println();
  Serial.println("Starting...");

  modes.reserve(5000);
  modes_setup();

  Serial.println("WS2812FX setup");
  ws2812fx.init();
  //ws2812fx.setMode(DEFAULT_MODE);
  //ws2812fx.setColor(DEFAULT_COLOR);
  //ws2812fx.setSpeed(DEFAULT_SPEED);
  update_demo();
  ws2812fx.setBrightness(DEFAULT_BRIGHTNESS);
  ws2812fx.start();

  Serial.println("Wifi setup");
  wifi_setup();
 
  Serial.println("HTTP server setup");
  server.on("/", srv_handle_index_html);
  server.on("/main.js", srv_handle_main_js);
  server.on("/modes", srv_handle_modes);
  server.on("/set", srv_handle_set);
  server.onNotFound(srv_handle_not_found);
  server.begin();
  Serial.println("HTTP server started.");

  Serial.println("ready!");
}


void loop() {
  unsigned long now = millis();

  server.handleClient();
  ws2812fx.service();

  if(now - last_wifi_check_time > WIFI_TIMEOUT*30) {
    Serial.print("Checking WiFi... ");
    if(WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi connection lost. Reconnecting...");
      wifi_setup();
    } else {
      Serial.println("OK");
    }
    last_wifi_check_time = now;
  }

  if(demo_mode) {
    if(now - last_change > TIMER_MS) {
      update_demo();
      last_change = now;
      //Serial.print(F("Speed is: "));
      //Serial.println(ws2812fx.getSpeed());
      //Serial.print(F("Set mode to: "));
      //Serial.print(ws2812fx.getMode());
      //Serial.print(" - ");
      //Serial.println(ws2812fx.getModeName(ws2812fx.getMode()));

      //if (cur_mode > sizeof(demo_modes)-1) {
      //  cur_mode = 0;
      //}; 
      //cur_mode++;
    }
  }
}

void update_demo() {
  while (demo_modes[cur_mode][0] == ws2812fx.getMode() || cur_mode == 0){ //exclude repeat same mode
    cur_mode = random(demo_modes_count);
  };
  int r = random(256);int g = random(256);int b = random(256);
  ws2812fx.setMode(demo_modes[cur_mode][0]);
  ws2812fx.setSpeed(demo_modes[cur_mode][1] * 80);
  ws2812fx.setColor(r, g, b);  
}



/*
 * Connect to WiFi. If no connection is made within WIFI_TIMEOUT, ESP gets resettet.
 */
void wifi_setup() {
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  WiFi.mode(WIFI_STA);
  #ifdef STATIC_IP  
    WiFi.config(ip, gateway, subnet);
  #endif

  unsigned long connect_start = millis();
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");

    if(millis() - connect_start > WIFI_TIMEOUT) {
      Serial.println();
      Serial.print("Tried ");
      Serial.print(WIFI_TIMEOUT);
      //Serial.print("ms. Resetting ESP now.");
      //ESP.reset();
      return;
    }
  }

  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println();
}


/*
 * Build <li> string for all modes.
 */
void modes_setup() {
  modes = "";
  for(uint8_t i=0; i < ws2812fx.getModeCount(); i++) {
    modes += "<li><a href='#' class='m' id='";
    modes += i;
    modes += "'>";
    modes += ws2812fx.getModeName(i);
    modes += "</a></li>";
  }
}

/* #####################################################
#  Webserver Functions
##################################################### */

void srv_handle_not_found() {
  server.send(404, "text/plain", "File Not Found");
}

void srv_handle_index_html() {
  server.send_P(200,"text/html", index_html);
}

void srv_handle_main_js() {
  server.send_P(200,"application/javascript", main_js);
}

void srv_handle_modes() {
  server.send(200,"text/plain", modes);
}

void srv_handle_set() {
  for (uint8_t i=0; i < server.args(); i++){
    if(server.argName(i) == "c") {
      uint32_t tmp = (uint32_t) strtol(&server.arg(i)[0], NULL, 16);
      if(tmp >= 0x000000 && tmp <= 0xFFFFFF) {
        ws2812fx.setColor(tmp);
      }
    }

    if(server.argName(i) == "m") {
      uint8_t tmp = (uint8_t) strtol(&server.arg(i)[0], NULL, 10);
      ws2812fx.setMode(tmp % ws2812fx.getModeCount());
      demo_mode = false;
    }

    if(server.argName(i) == "b") {
      if(server.arg(i)[0] == '-') {
        ws2812fx.decreaseBrightness(BRIGHTNESS_STEP);
      } else {
        ws2812fx.increaseBrightness(BRIGHTNESS_STEP);
      }
    }

    if(server.argName(i) == "s") {
      if(server.arg(i)[0] == '-') {
        ws2812fx.decreaseSpeed(SPEED_STEP);
      } else {
        ws2812fx.increaseSpeed(SPEED_STEP);
      }
    }
    if(server.argName(i) == "d") {
      if(server.arg(i)[0] == '-') {
        demo_mode = false;
      } else {
        demo_mode = true;
      }
    }
  }
  server.send(200, "text/plain", "OK");
}
