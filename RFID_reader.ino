/*  HTTPS on ESP8266 with follow redirects, chunked encoding support
 *  Version 2.1
 *  Author: Sujay Phadke
 *  Github: @electronicsguy
 *  Copyright (C) 2017 Sujay Phadke <electronicsguy123@gmail.com>
 *  All rights reserved.
 *
 *  Example Arduino program
 */

//To Do:
// Write something to handle repeated reads of RFID cards (Done)
//write something to handle incomplete reads, and indicate them
// write something to handle host connetion failure and connection status


#include <ESP8266WiFi.h>
#include "HTTPSRedirect.h"
#include "DebugMacros.h"

#define enablePin  2   // Connects to the RFID's ENABLE pin
#define BUFSIZE    11  // Size of receive buffer (in bytes) (10-byte unique ID + null character)
#define RFID_DATA_LENGTH 10 // size of RFID ID data length
#define RFID_START  0x0A  // RFID Reader Start and Stop bytes
#define RFID_STOP   0x0D
#define RFID_DATA_LENGTH 10

#define RECONNECTION_INTERVAL 10000

// for stack analytics
extern "C" {
#include <cont.h>
  extern cont_t g_cont;
}

//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// Fill ssid and password with your network credentials
//const char* ssid = "JJ Bean Cambie";
//const char* password = "railtown";
//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
const char* ssid = "basement949";
const char* password = "bluehouse949";

const char* host = "script.google.com";
//const char *GScriptId = "AKfycbzGWh0-1yZGfYaW3cN2tAdPePnUUHn4kioSM0ryER2kuG0lXwU"; //tim's test sheet 
const char *GScriptId = "AKfycbwouk1qPDc0V4I7qeNRaagVLp2FxFJzqOy8Z7fYCHRRA1jCP_C_"; //steven's actual sheet ID
const int httpsPort = 443;
String url = String("/macros/s/") + GScriptId + "/exec?value=-99";
HTTPSRedirect* client = nullptr;

unsigned long now = 0;
unsigned long previous_time = 0;

void setup() {
  
  pinMode(enablePin, OUTPUT);
  digitalWrite(enablePin, HIGH);  // disable RFID Reader
  
  Serial.begin(2400);
  Serial.flush();
  
  //free_heap_before = ESP.getFreeHeap();
  //free_stack_before = cont_get_free_stack(&g_cont);
  //Serial.printf("Free heap before: %u\n", free_heap_before);
  //Serial.printf("unmodified stack   = %4d\n", free_stack_before);
  
  Serial.println();
  Serial.print("Connecting to wifi: ");
  Serial.println(ssid);
  // flush() is needed to print the above (connecting...) message reliably, 
  // in case the wireless connection doesn't go through
  Serial.flush();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // Use HTTPSRedirect class to create a new TLS connection
  client = new HTTPSRedirect(httpsPort);
  client->setPrintResponseBody(true);
  client->setContentTypeHeader("application/json");
  
  Serial.print("Connecting to ");
  Serial.println(host);

  // Try to connect for a maximum of 5 times
  bool flag = false;
  for (int i=0; i<5; i++){
    int retval = client->connect(host, httpsPort);
    if (retval == 1) {
       flag = true;
       break;
    }
    else
      Serial.println("Connection failed. Retrying...");
  }

  if (!flag){
    Serial.print("Could not connect to server: ");
    Serial.println(host);
    Serial.println("Exiting...");
    return;
  }
  

  
  // Note: setup() must finish within approx. 1s, or the the watchdog timer
  // will reset the chip. Hence don't put too many requests in setup()
  // ref: https://github.com/esp8266/Arduino/issues/34
  

 

  // delete HTTPSRedirect object
  delete client;
  client = nullptr;
  now = millis();
}

void loop() {
  
  static int error_count = 0;
  static int connect_count = 0;
  const unsigned int MAX_CONNECT = 20;
  static bool flag = false;
  // Wait for a response from the RFID Reader
  // See Arduino readBytesUntil() as an alternative solution to read data from the reader
  char rfidData[BUFSIZE];  // Buffer for incoming data
  char prev_data[BUFSIZE]; //Buffer to save the last read RFID data
  char offset = 0;         // Offset into buffer
  rfidData[0] = 0;         // Clear the buffer
  //prev_data[0] = 0;        // Clear prev data buffer
  
  digitalWrite(enablePin, LOW);   // enable the RFID Reader
  
  if (!flag){
    //free_heap_before = ESP.getFreeHeap();
    //free_stack_before = cont_get_free_stack(&g_cont);
    client = new HTTPSRedirect(httpsPort);
    flag = true;
    client->setPrintResponseBody(true);
    client->setContentTypeHeader("application/json");
  }

  if (client != nullptr){
    if (!client->connected()){
      Serial.println("Client not connected. Reconnecting to client.");
      client->connect(host, httpsPort);
      
    }
  }
  else{
    DPRINTLN("Error creating client object!");
    error_count = 5;
  }
  
  if (connect_count > MAX_CONNECT){
    //error_count = 5;
    connect_count = 0;
    flag = false;
    delete client;
    return;
  }
  
  
  while(1)
  { 
    if(Serial.available() > 0) // If there are any bytes available to read, then the RFID Reader has probably seen a valid tag
    {
      rfidData[offset] = Serial.read();  // Get the byte and store it in our buffer
      if (rfidData[offset] == RFID_START)    // If we receive the start byte from the RFID Reader, then get ready to receive the tag's unique ID
      {
        offset = -1;     // Clear offset (will be incremented back to 0 at the end of the loop)
      }
      else if (rfidData[offset] == RFID_STOP)  // If we receive the stop byte from the RFID Reader, then the tag's entire unique ID has been sent
      {
        rfidData[offset] = 0; // Null terminate the string of bytes we just received
        
        now = millis();
        if(now - previous_time > RECONNECTION_INTERVAL||previous_time == 0){
            // check for wifi connetcion. if lost try to reconnect untill connection found
            if(WiFi.status() != WL_CONNECTED) {
                  Serial.println("Lost wifi connection. Reconnecting.");
                  WiFi.begin(ssid, password);
                  
            }
            while(WiFi.status() != WL_CONNECTED) {
                  delay(500);
                  Serial.print(".");
            }
    
            //
            if (!client->connected()){
                 Serial.println("Client not connected. Reconnecting to client.");
                 client->connect(host, httpsPort);
            }
            previous_time = now;
        }
        //If the scanned code from RFID is 10 bytes long
        if(strlen(rfidData) == RFID_DATA_LENGTH){
            //If the scanned data is different than the Last scanned data then try and upload it to 
            //the sheet
            if(strcmp(rfidData, prev_data) != 0 ){
                url = String("/macros/s/") + GScriptId + "/exec?value="+rfidData;
                // post data apended to the url
                //if (!client->connected()){
                // Serial.println("Client not connected. Reconnecting to client.");
                // client->connect(host, httpsPort);
               // }
                while(client->GET(url, host) != 1){
                  Serial.println("Client not connected. Reconnecting to client.");
                  client->connect(host, httpsPort);
                 }
                Serial.print("incomming tag:"); Serial.println(rfidData);       // The rfidData string should now contain the tag's unique ID with a null termination, so display it on the Serial Monitor
                Serial.print("previous tag:");Serial.println(prev_data);
                Serial.flush();
                strcpy(prev_data, rfidData); //save the new tag into the previous tag buffer
            }
         }else{
            Serial.println("ESP misread tag value.");
         }
         break;// Break out of the while(1); 
      }
      offset++;  // Increment offset into array
      if (offset >= BUFSIZE) offset = 0; // If the incoming data string is longer than our buffer, wrap around to avoid going out-of-bounds
    }
  }

  
  
 
  
  // In my testing on a ESP-01, a delay of less than 1500 resulted 
  // in a crash and reboot after about 50 loop runs.
  delay(200);
                          
}

