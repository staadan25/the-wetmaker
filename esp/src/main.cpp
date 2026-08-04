#include <Arduino.h>
#include <WiFi.h>
#include <conn_info.h>
#include <WireGuard-ESP32.h>
#include "time.h"

WiFiClient client;

int receive(int buff_size, byte* data);
bool connect_to_server();
bool connect_to_wifi();

//pin inputs and outputs
#define PUMP 5

//THESE PINS MUST MUST MUST HAVE THE CORRECT ASSOCIATED ORDER FOR THE SOLENOIDS AND MOISTURE MEASURMENTS
//IT WILL DROWN AND STARVE YOUR PLANTS IF IT IS NOT SYNCED
const int moist_sensors[] = {18};
const int sol_pins[] = {6};
const int water_times[] = {10000};//10 sec
const int water_delays[] = {48};//min num of hours between watering events
const int num_plants = 1;



//A0 - A4 is GPIO 18-14
//A5 is GPIO 8

const int timout_wifi = 60;//number of retries when connecting to wifi
const int wifi_retry_time = 1000;//timeout duration

const int timeout_server = 30;//number of retries when connecting to server
const int server_retry_time = 100;//timeout duration

const int timeout_data = 10;//number of retries when sending data
const int data_retry_time = 500;//timeout duration

const int sent_packet_size = 128;
const int recv_packet_size = 128;

time_t t = 0;

const int data_collect_size = 100; //how many measurements should be taken when measuring moisture
const int dat_collect_interval = 10; //delay between individual moisture measurements


class plant {
  public:
  int plant_num;
  int measure_pin;
  int sol_pin;
  int water_time;
  int water_delay;

  int last_water_time = -1;
  int moist_level = -1;

  //constructors
  plant():
  plant_num(-1), measure_pin(-1), sol_pin(-1), water_time(-1), water_delay(-1){};

  plant(int iplant_num, int imeasure_pin, int isol_pin, int iwater_time, int iwater_delay)
  : plant_num(iplant_num), measure_pin(imeasure_pin), sol_pin(isol_pin), water_time(iwater_time), water_delay(iwater_delay){
  }

  int measure(){
    int sum = 0;//hopefully this won't overflow

    for(int i = 0; i < data_collect_size; i++){//measures a specified number of times and returns the average
      int d = analogRead(measure_pin);
      if(sum+d > INT_MAX){
        Serial.println("DATA OVERFLOWING IN MEASURE");
        if(client.connected()){
          client.print("LDATA OVERFLOWING IN MEASURE");
        }
      }
      sum += d;
      delay(dat_collect_interval);
    }
    moist_level = sum/data_collect_size;
    if(moist_level > 3300 || moist_level < 1000){
      Serial.printf("ABNORMAL MOISTURE MEASUREMENT IN PLANT %n, : %n", plant_num, moist_level);
      if(client.connected()){
        client.printf("LABNORMAL MOISTURE MEASUREMENT IN PLANT %n, : %n", plant_num, moist_level);
      }
    }
    //low measure 1000, high measure 3300
    return moist_level; //maybe this would be better with a float, but whatever
  }

  void water(){
    digitalWrite(sol_pin, HIGH);
    delay(100);
    digitalWrite(PUMP, HIGH);
    delay(water_time);
    digitalWrite(PUMP,LOW);
    delay(100);
    digitalWrite(sol_pin,LOW);
    time(&t);
    last_water_time = t;
  }
};

plant* plants[num_plants];

void setup() {
  Serial.begin(115200);

  connect_to_wifi();
  connect_to_server();

  //i could sync this up with a time server, but i don't think it matters that much
  // tv.tv_sec = 0;
  // tv.tv_usec = 0;


  pinMode(PUMP, OUTPUT);
  
  for(int i = 0; i< num_plants; i++){
    pinMode(sol_pins[i], OUTPUT);
    pinMode(moist_sensors[i], INPUT);
    
    plants[i] = new plant(i, moist_sensors[i], sol_pins[i], water_times[i], water_delays[i]);
    plants[i]->measure();
  }
}

void loop() {
  if(WiFi.status() != WL_CONNECTED){
    connect_to_wifi();
  }
  if(WiFi.status() == WL_CONNECTED && !client.connected()){
    connect_to_server();
  }

  for(int i = 0; i< num_plants; i++){
    //nominal dry measurement ~ 3300
    // wet measurement ~1000
    plants[i]->measure();
    // if(plants[i].last_water_time + plants[i].water_delay < ){
      
    // }
    time(&t);
    Serial.println(t);
  }

  if(client.connected()){
    client.print("sup");

    byte recv[recv_packet_size];
    byte n = 0;
    for(int i = 0; i < recv_packet_size; i++){//remember to initialize your arrays kids
      recv[i] = 0;
    }
    if(receive(recv_packet_size, recv) == -1){
      Serial.println("data reception failed");
    }
    else{//responding to reception
      char type = (char)recv[0];
      switch (type)
      {
        case 'L'://L for log
          for(int i = 1; i<= recv_packet_size && recv[i] != 0; i++){
            Serial.printf("%c", (char)recv[i]);
          }
          Serial.println("");
          break;
        case 'C'://C for continue
          Serial.println("recieved sig C, continuing");
          break;
        case 'W'://W for water (set water level)
          break;
        case 'M'://M for measure
          n = recv[1];
          if(n > num_plants){
            client.print("Lrequested plant exceeds num of plants");
          }
          else{
            Serial.printf("plant %n requested, last moist level: %n", n, plants[n]->moist_level);
            client.print("N");
            client.print(plants[n]->moist_level);
          }
          break;
        default:
          Serial.printf("unrecognized signal recieved: %c\n",recv[0]);
          break;
      }
      
    }
  }
  delay(1000);
}


//look, i know this isn't secure, i don't really care
//waits for response from server and writes it to data
//returns size of data recieved, -1 if fail
int receive(int buff_size, byte* data){//buffer size is the size of the array returned
    if(!client.connected()){
      Serial.println("Dude, you're not even connected to the server");
      return -1;
    }

    int size = 0;//size of data recieved
    for(int i = 0;i<=timeout_data;i++){ //waiting for resp loop
      delay(data_retry_time);
      size = client.available();
      if(size > 0){//response recieved
        Serial.printf("packet of %d bytes recieved\n", size);
        break;
      }
      else{
        Serial.print("waiting for resp from server: ");
        Serial.println(i);
      }
    }

    if(!client.available()){
      Serial.println("No response from server");
      return -1;
    }

    else{
      if(client.available() > buff_size){
        Serial.println("WARN: PACKET SIZE EXCEEDS BUFFER SIZE");
      }

      for(int i = 0; i < buff_size && client.available(); i++){//reading data loop
        int d = client.read();
        if(d == -1){
          Serial.println("somehow data is unavailable");
          return -1;
        }
        else{
        data[i] = (byte)d;
        //this fucking function reads a byte and returns an int
        //i'm going to kill someone
        //it could have at least been a short
        }
      }
    }
    return size;
}

//connect to server, returns true if connection succeeded, false if fail
bool connect_to_server(){
  for(int i = 0; i <= timeout_server;i++){
    Serial.printf("attempting to connect to server: %d\n", i);
    if(client.connect(SERVER,PORT)){
      return true;
    }
    delay(server_retry_time);
  }
  Serial.println("Failed to connect to server");
  return false;
}

//attempts to connect to wifi, returns true if succeed, false if fail
bool connect_to_wifi(){
  for (int i = 0; WiFi.status() != WL_CONNECTED && i < timout_wifi;i++){
    Serial.println("connecting to wifi...");
    WiFi.begin(SSID, PWD);
    Serial.printf("attempting to connect to wifi: %d\n", i);
    delay(wifi_retry_time);
  }
  if(WiFi.status() == WL_CONNECTED){
    Serial.println("Connected to WiFi");
    Serial.println(WiFi.localIP().toString());
    return true;
  }
  else{
    Serial.println("WiFi connection Failed");
    return false;
  }
}


