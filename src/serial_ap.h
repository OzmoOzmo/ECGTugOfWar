#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>


#define UART1_RX_PIN 16
#define UART2_RX_PIN 17
#define UART1_TX_PIN -1 //unused...
#define UART2_TX_PIN -1 //unused...

//brain parsers
#include "Brain.h"
extern Brain brainA;
extern Brain brainB;

#define MAX_BUFFER_SIZE 36 // Set for 36-byte packets

// Task handle for the BLE task
static TaskHandle_t bt_loop_task_handle = NULL;

// Client and characteristic objects for each device
int bDebug = -1;

void bt_loop();
void DumpToLog(size_t length, uint8_t *pData);
void DumpNewReadToLog();
void bt_loop_task(void *pvParameters);

void bt_setup() {
  // Initialize UART1 on specified pins, 9600, 8N1
  Serial1.begin(9600, SERIAL_8N1, UART1_RX_PIN, UART1_TX_PIN);
  Serial2.begin(9600, SERIAL_8N1, UART2_RX_PIN, UART2_TX_PIN);
  // small pause so driver settles
  vTaskDelay(pdMS_TO_TICKS(50));

  // Create the BLE task / polling task (keeps existing behaviour)
  xTaskCreatePinnedToCore(
    bt_loop_task,          // Task function
    "SerialAB",             // Task name
    2048,                  // Stack size (4KB)
    NULL,                  // Task parameters
    2,                     // Priority
    &bt_loop_task_handle,  // Task handle
    1                      // Core 1
  );
}

void bt_loop() {
  // Read all bytes available on UART1 and forward in one chunk to dataArrived_A
  boolean gotNewData = false;
  while (Serial1.available() > 0) {
    uint8_t c = (uint8_t)Serial1.read();
    gotNewData |= brainA.update(c);
  }
  while (Serial2.available() > 0) {
    uint8_t c = (uint8_t)Serial2.read();
    gotNewData |= brainB.update(c);
  }
  if (gotNewData) {
    //Serial.print("Data! ");
    // If we got new data, dump it to log
    DumpNewReadToLog();
    //DumpToLog(
  }
  // allow other tasks to run
  vTaskDelay(pdMS_TO_TICKS(1));
}

void bt_loop_task(void *pvParameters) {
  // Run loop forever
  for (;;) {
    bt_loop();
    // Small delay to ensure task yields
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void DumpNewReadToLog()
{
  #ifdef VERBOSE
    logln(brain.readErrors());
    logln(brain.readCSV());
    int att = brain.readAttention();
    logln(att);
  #endif
  
  // Build one formatted line for both brains and print once
  char buf[40];
  snprintf(buf, sizeof(buf),
    "A:%3d,%3d,%3d  B:%3d,%3d,%3d",
    brainA.signalQuality, brainA.attention, brainA.getAverage(),
    brainB.signalQuality, brainB.attention, brainB.getAverage());
  //logln(buf);
  Serial.println(buf);
}

void DumpToLog(size_t length, uint8_t *pData)
{
  log("Received data (length: ");
  log(length);
  log("): ");
  for (size_t i = 0; i < length; i++)
  {
    if (pData[i] < 0x10)
      log("0");
    log(pData[i], HEX);
    log(" ");
  }
  logln();
}




// Callback for characteristic notifications for device 1
// void dataArrived_A(uint8_t* pData, size_t length) {
//   #ifdef VERBOSE
//     DumpToLog(length, pData);
//   #endif

//   // parse the binary data here
//   if (brainA.update(pData, length)) {
//       DumpNewReadToLog(brainA);
//   }
// }

// // Callback for characteristic notifications for device 2
// void dataArrived_B(uint8_t* pData, size_t length) {

//   #ifdef VERBOSE
//     DumpToLog(length, pData);
//   #endif

//   // parse the binary data here
//   if (brainB.update(pData, length))
//   {
//       DumpNewReadToLog(brainB);
//   }
// }