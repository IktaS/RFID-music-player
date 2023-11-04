#include <string>

// -------------------------- Debug Settings -----------------------------------
// Debug output is disabled if any of the IR pins are on the TX (D1) pin.
// See `isSerialGpioUsedByIr()`.
// Note: Debug costs ~6k of program space.
#ifndef DEBUG
#define DEBUG true // Change to 'true' for serial debug output.
#endif             // DEBUG

// MQTT topic stuff
// Name of the json config file in SPIFFS.
const String write_mode_topic = "write_mode";
const String write_value_topic = "write_value";
const String play_topic = "play";
const String will_topic = "status";
const String topic_prefix = "homeassistant/music-player";
const String device_id = "38beb38e-5b9e-4bf5-b8b1-dd414cdae9fd";

// MQTT Stuff
const char *const mqtt_server = "192.168.18.52";
const uint16_t mqtt_port = 1883;
const char *const mqtt_user = "mosquitto";
const char *const mqtt_password = "mosquitto-client";

// RC522 Setup
#define SS_PIN 5
#define RST_PIN 21