#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "airline_logos.h"

// Hardware constants
#define PIN_LAT 22
#define PIN_A 19
#define PIN_B 23
#define PIN_C 18
#define PIN_D 5
#define PIN_E 15
#define PIN_OE 16
#define PIN_R1 13
#define PIN_G1 27
#define PIN_B1 26
#define PIN_R2 12
#define PIN_G2 25
#define PIN_B2 17
#define PIN_CLK 14

#define PANEL_RES_X 80
#define PANEL_RES_Y 40
#define PANEL_CHAIN 1

// Software constants
#define USER_ID "1"
#define WIFI_SSID "maison"
#define WIFI_PASSWORD "maison609"
#define FLIGHT_URL "http://192.168.1.6:8080/flights"

SemaphoreHandle_t fetching_mutex = NULL;
TaskHandle_t rendering_task;
TaskHandle_t fetching_task;
MatrixPanel_I2S_DMA *display = nullptr;
uint16_t color_black, color_white, color_red, color_green, color_blue;
unsigned long last_fetch = 0;
unsigned long fetch_delay = 2500;
char* no_flights_text = "NONE";
char* flight_data = NULL;
char* flight_id = NULL;
char* flight_route = NULL;
char* aircraft_make = NULL;
char* aircraft_model = NULL;
char* flight_distance = NULL;
char* flight_altitude = NULL;

void init_wifi();
void init_display();
void rendering_loop(void* params);
void flight_fetching_loop(void* params);
void render_string(char* string, int start_x_pos, int start_y_pos, uint16_t color);
void render_logo(char* flight_id, int x_pos, int y_pos);
void fetch_flight();

// ESP32 functions

void setup() {
  Serial.begin(115200);

  init_display();
  init_wifi();

  fetching_mutex = xSemaphoreCreateMutex();

  if (fetching_mutex == NULL) {
    Serial.println("Failed to create mutex");
    while(1);
  }

  xTaskCreatePinnedToCore(
    rendering_loop,
    "Rendering",
    10000,
    NULL,
    1,
    &rendering_task,
    0
  );

  xTaskCreatePinnedToCore(
    flight_fetching_loop,
    "FlightFetching",
    10000,
    NULL,
    1,
    &fetching_task,
    1
  );
}

void loop() {
  
}

// Logic functions

void rendering_loop(void* params) {
  for (;;) {
    if (xSemaphoreTake(fetching_mutex, portMAX_DELAY) == pdTRUE) {
      int curr_x_pos = 1;
      int curr_y_pos = 1;

      display->clearScreen();

      if (flight_data != NULL) {
        render_logo(flight_id, curr_x_pos, curr_y_pos);

        curr_x_pos += 26; // logo size + margin

        render_string(flight_id, curr_x_pos, curr_y_pos, color_white);
        display->drawLine(curr_x_pos - 1, curr_y_pos + 7, 78, curr_y_pos + 7, color_red);

        if (flight_route != NULL) {
          curr_y_pos += 8;
          render_string(flight_route, curr_x_pos, curr_y_pos, color_white);
          display->drawLine(curr_x_pos - 1, curr_y_pos + 7, 78, curr_y_pos + 7, color_red);
        }

        if (aircraft_make != NULL) {
          curr_y_pos += 8;
          render_string(aircraft_make, curr_x_pos, curr_y_pos, color_white);
        }

        if (aircraft_model != NULL) {
          curr_y_pos += 8;
          render_string(aircraft_model, curr_x_pos, curr_y_pos, color_white);
          display->drawLine(curr_x_pos - 1, curr_y_pos + 7, 78, curr_y_pos + 7, color_red);
        }

        curr_x_pos = 1;
        curr_y_pos = 33;

        render_string(flight_distance, curr_x_pos, curr_y_pos, color_white);

        curr_x_pos += (strlen(flight_distance) * 6) + 4;
        
        render_string(flight_altitude, curr_x_pos, curr_y_pos, color_white);
      } else {
        render_string("...", 0, 0, color_white);
      }

      xSemaphoreGive(fetching_mutex);
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void flight_fetching_loop(void* params) {
  Serial.print("Flight fetching running on core ");
  Serial.println(xPortGetCoreID());

  for (;;) {
    if ((millis() - last_fetch) > fetch_delay) {
      if (xSemaphoreTake(fetching_mutex, portMAX_DELAY) == pdTRUE) {
        fetch_flight();
        last_fetch = millis();
        xSemaphoreGive(fetching_mutex);
      }

      if (flight_id != NULL) {
        Serial.print("Flight ID:");
        Serial.println(flight_id);
      }

      if (flight_route != NULL) {
        Serial.print("Route:");
        Serial.println(flight_route);
      }

      if (aircraft_make != NULL) {
        Serial.print("Aircraft Make:");
        Serial.println(aircraft_make);
      }

      if (aircraft_model != NULL) {
        Serial.print("Aircraft Model:");
        Serial.println(aircraft_model);
      }

      if (flight_distance != NULL) {
        Serial.print("Distance:");
        Serial.println(flight_distance);
      }

      if (flight_altitude != NULL) {
        Serial.print("Altitude:");
        Serial.println(flight_altitude);
      }
    }
  } 
}

void init_wifi() {
  byte status_flip = 1;
  char status_a[] = "WiFi..";
  char status_b[] = "WiFi...";

  display->clearScreen();
  render_string(status_a, 0, 0, color_white);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi ..");

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');

    display->clearScreen();

    if (status_flip == 0) {
      render_string(status_a, 0, 0, color_white);
    } else {
      render_string(status_b, 0, 0, color_white);
    }

    status_flip ^= 1;

    delay(250);
  }

  Serial.println(WiFi.localIP());
}

void init_display() {
  HUB75_I2S_CFG::i2s_pins _pins={PIN_R1, PIN_G1, PIN_B1, PIN_R2, PIN_G2, PIN_B2, PIN_A, PIN_B, PIN_C, PIN_D, PIN_E, PIN_LAT, PIN_OE, PIN_CLK};
  HUB75_I2S_CFG mxconfig(
    PANEL_RES_X,
    PANEL_RES_Y,
    PANEL_CHAIN,
    _pins
  );

  mxconfig.clkphase = false;
  mxconfig.driver = HUB75_I2S_CFG::FM6126A;
  mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_20M;
  mxconfig.min_refresh_rate = 1;

  display = new MatrixPanel_I2S_DMA(mxconfig);
  display->begin();
  display->setBrightness8(200);
  display->clearScreen();

  color_black = display->color565(0, 0, 0);
  color_white = display->color565(255, 255, 255);
  color_red = display->color565(255, 0, 0);
  color_green = display->color565(0, 255, 0);
  color_blue = display->color565(0, 0, 255);

  display->fillScreen(color_white);
}

void render_string(char* string, int start_x_pos, int start_y_pos, uint16_t color) {
  display->setCursor(start_x_pos, start_y_pos);
  display->setTextColor(color, color_black);
  display->print(string);
}

void render_logo(char* flight_id, int x_pos, int y_pos) {
  char airline_code[4];

  memcpy(airline_code, flight_id, 3);
  airline_code[3] = '\0';

  const unsigned short* bitmap = LOGO_UNKNOWN;

  if (strcmp(airline_code, "AAL") == 0) {
    bitmap = LOGO_AAL;
  } else if (strcmp(airline_code, "UAL") == 0) {
    bitmap = LOGO_UAL;
  } else if (strcmp(airline_code, "GJS") == 0) {
    bitmap = LOGO_UAL;
  } else if (strcmp(airline_code, "DAL") == 0) {
    bitmap = LOGO_DAL;
  } else if (strcmp(airline_code, "SKW") == 0) {
    bitmap = LOGO_SKW;
  } else if (strcmp(airline_code, "RPA") == 0) {
    bitmap = LOGO_RPA;
  } else if (strcmp(airline_code, "FFT") == 0) {
    bitmap = LOGO_FFT;
  } else if (strcmp(airline_code, "ENY") == 0) {
    bitmap = LOGO_ENY;
  } else if (strcmp(airline_code, "DLH") == 0) {
    bitmap = LOGO_DLH;
  }

  display->drawRGBBitmap(x_pos, y_pos, bitmap, 24, 24);
}

void fetch_flight() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Could not fetch flight data due to no WiFi");
    return;
  }

  if (flight_data != NULL) {
    free(flight_data);
    flight_data = NULL;
    flight_id = NULL;
    flight_route = NULL;
    aircraft_make = NULL;
    aircraft_model = NULL;
    flight_distance = NULL;
    flight_altitude = NULL;
  }

  HTTPClient http;
  http.begin(FLIGHT_URL);
  http.addHeader("X-User-Id", USER_ID);
  int response_code = http.GET();

  if (response_code > 0) {
    if (response_code == 200) {
      const char* payload = http.getString().c_str();
      Serial.print("Successfully fetch flight data: ");
      Serial.println(payload);

      flight_data = (char*)malloc(strlen(payload) + 1); 
      strcpy(flight_data, payload);

      char* token = strtok(flight_data, ":");
      int index = 0;

      // FLIGHT_ID:ROUTE:AIRCRAFT_MAKE:AIRCRAFT_MODEL:DISTANCE:ALTITUDE

      while (token != NULL) {
        if (token[0] == '#') {
          token = NULL;
        }

        if (index == 0) {
          flight_id = token;
        } else if (index == 1) {
          flight_route = token;
        } else if (index == 2) {
          aircraft_make = token;
        } else if (index == 3) {
          aircraft_model = token;
        } else if (index == 4) {
          flight_distance = token;
        } else if (index == 5) {
          flight_altitude = token;
        }

        token = strtok(NULL, ":");
        index++;
      }
    }
  } else {
    Serial.print("Failed to fetch flight data: ");
    Serial.println(response_code);
  }

  http.end();
}
