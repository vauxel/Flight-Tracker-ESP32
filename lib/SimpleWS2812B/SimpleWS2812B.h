#ifndef SimpleWS2812B_h
#define SimpleWS2812B_h

#include <Arduino.h>

class SimpleWS2812B {
  public:
    struct LEDColor {
      byte r, g, b;
    };

    SimpleWS2812B(gpio_num_t pin, int led_count);
    ~SimpleWS2812B();

    void fill_color(LEDColor color);
    void set_color(int pos, LEDColor color);
    void update_leds();

  private:
    gpio_num_t pin;
    int num_leds;
    LEDColor* colors;

    void write_byte(byte byte_to_write);
    void send_one();
    void send_zero();
};

#endif
