// SMART PET BOWL

// Giulia Sellitto
// Lab of IoT project
// June 2020


// This code is configurable for your needs. 
// Please check lines 13 and 40


// --------------------------------------------------
// Please uncomment the following line if you want to see debug prints on the serial monitor
//#define DEBUG
// --------------------------------------------------
#ifdef DEBUG
  #define DEBUG_PRINTLN(x)  Serial.println(x)
  #define DEBUG_PRINT(x)    Serial.print(x)
#else
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINT(x)  
#endif


// --------------------------------------------------
// Libraries and includes
// --------------------------------------------------
#include "arduino_secrets.h"
#include <SPI.h>
#include <WiFiNINA.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <HX711.h>
#include <Firebase_Arduino_WiFiNINA.h>
#include <Servo.h>

// --------------------------------------------------
// Global variables
// --------------------------------------------------
// Configurables
const char* ntpServerName = "time.inrim.it";  // your zone's NTP server
const int time_zone = 2;     // time_zone = your time zone hour - Greenwich time zone hour
#define LOADCELL_DATA_PIN 4
#define LOADCELL_SCK_PIN 3
#define SERVO_MOTOR_PIN 9
#define BUTTON_PIN 12
// --------------------------------------------------
// Weight
HX711 load_sensor;
float units;
float scale_factor;
float full_weight;
float current_weight;
float fill_quantity;
// --------------------------------------------------
// Servo motor
Servo servo;
int servo_open_degrees = 30; // position of the servo to open the port
int time_to_stay_open_millis;
// --------------------------------------------------
// UDP 
const unsigned int localPort = 2390;      // local port to listen for UDP packets
WiFiUDP Udp; 
// --------------------------------------------------
// Time 
IPAddress time_server; // IP that will be looked up by name
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
time_t current_time;
unsigned long secsSince1900;
unsigned long highWord; // used in NTP conversion
unsigned long lowWord; // used in NTP conversion
int next_refill_h; // hour of the time the task execution is scheduled for
int next_refill_m; // minute of the time the task execution is scheduled for
int sleep_time_minutes; 
#define MAX_DELAY_MILLIS 10000
int num_delay_in_min = 60000 / MAX_DELAY_MILLIS; //number of delays due to sleep for a minute
// --------------------------------------------------
// Database
FirebaseData firebase_data; // data object
String json_str;
int id;
int tot_id;
// --------------------------------------------------
// Other variables
int i;
int j;
char str[30];


// --------------------------------------------------
// Setup function
// --------------------------------------------------
void setup() {
  // open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  
  Serial.println("---------- SMART PET BOWL -----------");
  Serial.println("          Giulia Sellitto");
  Serial.println("         Lab of IoT Project");
  Serial.println("             June 2020\n");
  
  Serial.println("Application setup starting...");

  pinMode(BUTTON_PIN, INPUT);
  
  initialize_servo();
  
  calibrate_load_sensor();

  set_full_weight();

  first_connection_to_wifi(WIFI_SSID, WIFI_PASSWORD);

  connect_db();

  Serial.println("Application setup completed!\n");
}


// --------------------------------------------------
// Loop function
// --------------------------------------------------
void loop() {
  setTime(get_time_function());
  Serial.println("Current time: " + String(hour()) + ":" + String(minute()));
  //DEBUG_PRINT(("Current time: "));
  //DEBUG_PRINT((hour())); DEBUG_PRINT((":")); DEBUG_PRINTLN((minute())); 
  
  get_next_refill_time();
  Serial.println("Next refill is scheduled for: " + String(next_refill_h) + ":" + String(next_refill_m));
  
  disconnect_db();
  WiFi.end();
  DEBUG_PRINTLN(("WiFi disconnected."));
  
  sleep_time_minutes = (next_refill_h * 60 + next_refill_m) - (hour() * 60 + minute());
  if (sleep_time_minutes < 0) {
    // next refill is scheduled for tomorrow
    sleep_time_minutes = (24 * 60) + sleep_time_minutes;
  }
  Serial.println("Entering waiting mode for " + String(sleep_time_minutes / 60) + " h " + String(sleep_time_minutes % 60) + " m");

  wait_for_sleep_time();
 
  Serial.println("\nSystem awake!");
  
  do_task();
}


// --------------------------------------------------
// Custom functions
// --------------------------------------------------
void do_task() {
  Serial.println("Starting task...");
  load_sensor.power_up();
  
  get_current_weight();

  connect_db();
  
  update_db_weight();

  if (current_weight < full_weight) {
    fill_quantity = full_weight - current_weight;
    Serial.println("Fill quantity: " + String(fill_quantity) + " g"); 
    time_to_stay_open_millis = (int) fill_quantity / 25 * 1000; // 25 g are put without staying steady open
    fill_bowl();
  }
  else {
    Serial.println("Bowl is full.");
  }

  get_current_weight();
  
  update_db_weight();

  load_sensor.power_down();  

  Serial.println("Task completed!\n");
}




void get_current_weight() {
  DEBUG_PRINTLN(("Get current weight..."));
  current_weight = load_sensor.get_units(20);
  Serial.println("Current weight is: " + String(current_weight) + " g");
}




void fill_bowl() {
  DEBUG_PRINTLN(("Filling bowl..."));
  servo.attach(SERVO_MOTOR_PIN);
  //port opening
  for (i = 1; i < servo_open_degrees; i++) {
    servo.write(i);
    delay(100);
  }
  delay(time_to_stay_open_millis);
  //port closing
  for (; i >= 0; i--) {
    servo.write(i);
    delay(100);
  }
  servo.detach();
  DEBUG_PRINTLN(("Bowl filled."));
}




void initialize_servo() {
  DEBUG_PRINTLN(("Initializing servo motor..."));
  servo.attach(SERVO_MOTOR_PIN);
  servo.write(0);
  delay(5000);
  servo.detach();
  DEBUG_PRINTLN(("Servo motor initialized succesfully!"));
}




void calibrate_load_sensor() {
  Serial.println("Starting load sensor calibration...");
  load_sensor.begin(LOADCELL_DATA_PIN, LOADCELL_SCK_PIN);
  
  Serial.println("PLEASE EMPTY THE BOWL AND PRESS THE BUTTON.");
  while (digitalRead(BUTTON_PIN) != HIGH) {;} // waiting for the button to be pressed
  Serial.println("OK");
  
  load_sensor.set_scale();
  load_sensor.tare();
  Serial.println("PLEASE PLACE 100 G AND PRESS THE BUTTON.");
  while (digitalRead(BUTTON_PIN) != HIGH) {;} // waiting for the button to be pressed
  Serial.println("OK");
  units = load_sensor.get_units(10);
  scale_factor = units / 100; 
  load_sensor.set_scale(scale_factor);


  DEBUG_PRINTLN(("After setting up the load sensor:"));
  // print a raw reading
  DEBUG_PRINT(("read: \t\t")); DEBUG_PRINTLN((load_sensor.read())); 
  // print the average of 20 readings
  DEBUG_PRINT(("read average: \t")); DEBUG_PRINTLN((load_sensor.read_average(20)));  
  // print the average of 5 readings minus the tare weight
  DEBUG_PRINT(("get value: \t")); DEBUG_PRINTLN((load_sensor.get_value(5)));
  // print the average of 5 readings minus tare weight, divided by the scale parameter
  DEBUG_PRINT(("get units: \t")); DEBUG_PRINTLN((load_sensor.get_units(5)));        

  Serial.println("Load sensor calibrated succesfully!");
}





void set_full_weight() {
  Serial.println("PLEASE FILL THE BOWL AND PRESS THE BUTTON.");
  while (digitalRead(BUTTON_PIN) != HIGH) {;} // waiting for the button to be pressed
  Serial.println("OK");
  get_current_weight();
  full_weight = current_weight;
  Serial.println("Full bowl weight saved succesfully!");
}




void connect_db() {
  DEBUG_PRINTLN(("Connecting to online database..."));
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH, WIFI_SSID, WIFI_PASSWORD);
  Firebase.reconnectWiFi(true);
  DEBUG_PRINTLN(("Successfully connected to online database!")); 
}




void disconnect_db() {
  Firebase.reconnectWiFi(false);
  DEBUG_PRINTLN(("Online database disconnected."));
}




void update_db_weight() {
  DEBUG_PRINTLN(("Updating online database with weight data..."));
  // Retrieving id to write 
  if (Firebase.getInt(firebase_data, "bowl/id_to_write")) {
    if (firebase_data.dataType() == "int") {
      id = firebase_data.intData();
    }
  }
  else { id = 1; }
  
  // Writing weight data
  json_str = "{\"time_h\":"+String(hour())+", \"time_m\":"+String(minute())+", \"weight\":"+String(current_weight)+"}";
  if (Firebase.setJSON(firebase_data,"bowl/"+String(id), json_str)) {
    DEBUG_PRINTLN(("Successfully written:"));
    DEBUG_PRINTLN((json_str));
  }
  else {
    DEBUG_PRINTLN(("Couldn't write:"));
    DEBUG_PRINTLN((json_str));
    DEBUG_PRINT(("Reason: ")); DEBUG_PRINTLN((firebase_data.errorReason()));
  }

  // update next id to write
  id++;  
  json_str = "{\"id_to_write\": "+String(id)+"}";
  if (Firebase.updateNode(firebase_data, "bowl", json_str)) {
      DEBUG_PRINT(("Successfully updated next id to write with value: ")); DEBUG_PRINTLN((json_str));
  }
  else {
    DEBUG_PRINT(("Couldn't update next id to write value: ")); DEBUG_PRINTLN((json_str));
    DEBUG_PRINT(("Reason: ")); DEBUG_PRINTLN((firebase_data.errorReason()));
  }
}




void get_next_refill_time() {
  DEBUG_PRINTLN(("Getting next scheduled refill time..."));

  if (Firebase.getInt(firebase_data, "schedule/id_to_read")) {
    if (firebase_data.dataType() == "int") {
      id = firebase_data.intData();
    }
  }
  else { id = 1; }

  if (Firebase.getString(firebase_data, "schedule/tot_id")) {
    if (firebase_data.dataType() == "string") { //due to saving by mobile app
      firebase_data.stringData().toCharArray(str, 30);
      tot_id = atoi(str);
    }
  }
  else { tot_id = 1; }
  
  // read next scheduled refill time 
  if (Firebase.getString(firebase_data, "schedule/"+String(id))) {
    if (firebase_data.dataType() == "string") { //due to saving by mobile app
      // data is like: "{\"time_h\":xx,\"time_m\":yy}"
      firebase_data.stringData().toCharArray(str, 30);
      sscanf(str, "{\\\"time_h\\\":%d,\\\"time_m\\\":%d}", &next_refill_h, &next_refill_m);
    }
  }
  else { 
    next_refill_h = (hour() + 1) % 24; 
    next_refill_m = minute();
  }

  //update id to read
  id = (id % tot_id) + 1;
  json_str = "{\"id_to_read\": "+String(id)+"}";
  if (!Firebase.updateNode(firebase_data, "schedule", json_str)) {
    DEBUG_PRINT(("Couldn't update next id to read value: ")); DEBUG_PRINTLN((json_str));
    DEBUG_PRINT(("Reason: ")); DEBUG_PRINTLN((firebase_data.errorReason()));
  }
}




void wait_for_sleep_time() {
  // i stands for already sleepen minutes
  for (i = 0; i < sleep_time_minutes; i++) {
    for (j = 0; j < num_delay_in_min; j++) {
      delay(MAX_DELAY_MILLIS);
    }
  }
}




time_t get_time_function() {
  Udp.begin(localPort);
  DEBUG_PRINTLN(("Synchronizing time by calling NTP server..."));
  
  WiFi.hostByName(ntpServerName, time_server); 
  DEBUG_PRINT(("NTP server address: ")); DEBUG_PRINTLN((time_server));
  send_ntp_packet(time_server); // send an NTP packet to a time server
  // wait to see if a reply is available
  delay(1000);
    if (Udp.parsePacket()) {
      DEBUG_PRINTLN(("NTP packet received."));
      Udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
      //the timestamp starts at byte 40 of the received packet and is four bytes, or two words, long. First, esxtract the two words:
  
      highWord = word(packetBuffer[40], packetBuffer[41]);
      lowWord = word(packetBuffer[42], packetBuffer[43]);
      // combine the four bytes (two words) into a long integer
      // this is NTP time (seconds since Jan 1 1900):
      secsSince1900 = highWord << 16 | lowWord;
      DEBUG_PRINT(("Seconds since Jan 1 1900 = ")); DEBUG_PRINTLN((secsSince1900));
      current_time = secsSince1900 - 2208988800UL + time_zone * SECS_PER_HOUR;
      Udp.stop();
      return current_time;
    }
    else {
      Udp.stop();
      return 0;
    }
}




unsigned long send_ntp_packet(IPAddress& address) {
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
  
  DEBUG_PRINTLN(("NTP packet sent."));
}




void first_connection_to_wifi(char* ssid, char* pass) {
  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    return false;
  }
  // check for the firmware version
  String fv = WiFi.firmwareVersion();
  if (fv < "1.0.0") {
    Serial.println("Please upgrade the firmware!");
  }

  // attempt to connect to Wifi network:
  while (WiFi.status() != WL_CONNECTED) {
    // connect to network
    Serial.println("Attempting to connect to WiFi...");
    WiFi.begin(ssid, pass);
    // wait 10 seconds for connection:
    delay(10000);
  }

  Serial.println("Connected to WiFi:");
  printWifiStatus();
  Serial.println("Succesfully connected to WiFi!");
}



 
void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());
  // print your board's IP address:
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  // print the received signal strength:
  Serial.print("Signal strength (RSSI): ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");
}


// END OF FILE
