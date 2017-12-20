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
#define LED_PIN    16
#define BUFSIZE    11  // Size of receive buffer (in bytes) (10-byte unique ID + null character)
#define RFID_DATA_LENGTH 10 // size of RFID ID data length
#define RFID_START  0x0A  // RFID Reader Start and Stop bytes
#define RFID_STOP   0x0D
#define RFID_DATA_LENGTH 10
//interval used to try reconnecting to wifi
#define RECONNECTION_INTERVAL 10000
//length the LEd is on after a succeful sheet upload
#define LED_BLINK_INTERVAL    500

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
unsigned long led_start_time = 0;

static int error_count = 0;
static int connect_count = 0;
const unsigned int MAX_CONNECT = 20;
static bool client_flag = false;
//records wether the led is on or off (true == on)
static bool led_flag = false;
// Buffer for incoming data
char rfidData[BUFSIZE]; 
// a temporary buffer to help with making sure noise hasnt been read by the reader
char temp_rfid[BUFSIZE];
//Buffer to save the last read RFID data
char prev_sheet_data[BUFSIZE];
//used to index the array holding RFID tags 
char offset = 0;

void setup() {
  
  //initialize Digital Port for confirmation LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  //initilize data arrays
  rfidData[0] = 0;         // Clear the buffer
  temp_rfid[0]=0;
  prev_sheet_data[0]=0;

  //initialize RFID reader
  pinMode(enablePin, OUTPUT);
  digitalWrite(enablePin, HIGH);  // disable RFID Reader
  
  Serial.begin(2400);
  Serial.flush();
  Serial.println("started");
  
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
  
  // enable the RFID Reader
  digitalWrite(enablePin, LOW);   
  
  if (!client_flag)
  {
    //free_heap_before = ESP.getFreeHeap();
    //free_stack_before = cont_get_free_stack(&g_cont);
    client = new HTTPSRedirect(httpsPort);
    client_flag = true;
    client->setPrintResponseBody(true);
    client->setContentTypeHeader("application/json");
  }

  if (client != nullptr)
  {
    if (!client->connected())
    {
      Serial.println("Client not connected. Reconnecting to client.");
      client->connect(host, httpsPort);
      
    }
  }else{
    DPRINTLN("Error creating client object!");
    error_count = 5;
  }
  
  if (connect_count > MAX_CONNECT)
  {
    //error_count = 5;
    connect_count = 0;
    client_flag = false;
    delete client;
    return;
  }
  
  //If the LED is on and it has been on longer than the blink interval, turn it off
  if(led_flag)
  {
    now = millis();
    if(now - led_start_time > LED_BLINK_INTERVAL)
    {
        led_flag = false;
        digitalWrite(LED_PIN, LOW);
    }
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
            while(WiFi.status() != WL_CONNECTED)
            {
                  delay(500);
                  Serial.print(".");
            }
            if (!client->connected())
            {
                 Serial.println("Client not connected. Reconnecting to client.");
                 client->connect(host, httpsPort);
            }
            previous_time = now;
        }
        //If the scanned code from RFID is 10 bytes long
        if(strlen(rfidData) == RFID_DATA_LENGTH)
        {
            //If the new RFID data is the same as the last read data then we know its not noise
            // and a good reading
            Serial.print("new tag:"); Serial.println(rfidData);       
            Serial.print("temp value:");Serial.println(prev_sheet_data);
            if(strcmp(rfidData, temp_rfid) == 0)
            {
                //If new rfid data is different than the last data uploaded to 
                //the sheet, so we want to upload it.
                if(strcmp(rfidData, prev_sheet_data) != 0 )
                {
                    //turn the LED on signaling a write to google sheets
                    digitalWrite(LED_PIN,HIGH);
                    led_start_time = millis();
                    led_flag = true;
                    
                    url = String("/macros/s/") + GScriptId + "/exec?value="+rfidData;
                    while(client->GET(url, host) != 1)
                    {
                      Serial.println("Client not connected. Reconnecting to client.");
                      client->connect(host, httpsPort);
                     }

                     
                      
                    // The rfidData string should now contain the tag's unique ID with a null termination, so display it on the Serial Monitor
                    Serial.print("new value to write to sheet:"); Serial.println(rfidData);       
                    Serial.print("last value written to sheet:");Serial.println(prev_sheet_data);
                    Serial.flush();
                    //save the new tag into the previous tag buffer
                    strcpy(prev_sheet_data, rfidData); 
                }
                //save newest RFID code into temp array
                strcpy(temp_rfid,rfidData);
            }else{
                //save newest RFID code into temp array
                strcpy(temp_rfid,rfidData);
            }
         }else{
             Serial.println("ESP misread tag value.");
         }
         break;// Break out of the while(1); 
      }
      // Increment offset into array
      offset++;
      // If the incoming data string is longer than our buffer, wrap around to avoid going out-of-bounds  
      if (offset >= BUFSIZE) offset = 0; 
    }
  }

  
  
 
  
  // In my testing on a ESP-01, a delay of less than 1500 resulted 
  // in a crash and reboot after about 50 loop runs.
  delay(200);
                          
}

