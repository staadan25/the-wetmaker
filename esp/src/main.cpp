#include <Arduino.h>
#include <WiFi.h>
#include <conn_info.h>


//pin inputs and outputs
#define LED 5
#define AREAD 6



const int timout_wifi = 60;//number of retries when connecting to wifi
const int wifi_retry_time = 1000;//timeout duration

const int timeout_server = 30;//number of retries when connecting to server
const int server_retry_time = 100;//timeout duration

const int timeout_data = 10;//number of retries when sending data
const int data_retry_time = 500;//timeout duration

const int sent_packet_size = 128;
const int recv_packet_size = 128;

WiFiClient client;

int receive(int buff_size, byte* data);
bool connect_to_server();
bool connect_to_wifi();


void setup() {
  Serial.begin(115200);

  connect_to_wifi();
  connect_to_server();

  pinMode(LED, OUTPUT);
  pinMode(AREAD, INPUT);
}

void loop() {
  if(WiFi.status() != WL_CONNECTED){
    connect_to_wifi();
  }
  if(!client.connected()){
    connect_to_server();
  }

  if(client.connected()){
    char data[sent_packet_size];//declare the initial data array

    client.print("sup");

    byte recv[recv_packet_size];
    for(int i = 0; i < recv_packet_size; i++){//remember to initialize your arrays kids
      recv[i] = 0;
    }
    if(receive(recv_packet_size, recv) == -1){
      Serial.println("data reception failed");
    }

    else{//responding to reception
      char type = (char)recv[0];
      Serial.printf("recieved type: %c\n", type);
      switch (type)
      {
        case 'S':
          for(int i = 1; i<= recv_packet_size && recv[i] != 0; i++){
            Serial.printf("%c", (char)recv[i]);
          }
          Serial.println("");
          break;
        case 'C':
          Serial.println("recieved sig C, continuing");
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
