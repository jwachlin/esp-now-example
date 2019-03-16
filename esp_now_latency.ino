/*
  MIT License

  Copyright (c) 2019 by Jacob Wachlin

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

// ESP-NOW latency and throughput testing

#include <WiFi.h>
#include <esp_now.h>
#include <stdint.h>
#include <string.h>

#define WIFI_CHANNEL                        (1)
#define PAYLOAD_EXTRA_BYTES                 (242) // max 242
#define MESSAGE_SEND_INTERVAL               (200)
#define MESSAGES_TO_SEND                    (1000)

// SENDER and RECEIVER are arbitrary with ESP-NOW, this is solely for this example
#define SENDER
#ifndef SENDER
#define RECEIVER
#endif

static uint8_t broadcast_mac[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

static uint32_t g_counter = 0;
static int32_t error_counter = 0;
static uint32_t send_time_ms = 0;
static uint32_t recv_back_time_ms = 0;

typedef struct __attribute__((packed)) esp_now_msg_t
{
  uint32_t address;
  uint32_t counter;
  uint8_t padding[PAYLOAD_EXTRA_BYTES];
} esp_now_msg_t;



static void handle_error(esp_err_t err)
{
  switch (err)
  {
    case ESP_ERR_ESPNOW_NOT_INIT:
      Serial.println("Not init");
      break;

    case ESP_ERR_ESPNOW_ARG:
      Serial.println("Argument invalid");
      break;

    case ESP_ERR_ESPNOW_INTERNAL:
      Serial.println("Internal error");
      break;

    case ESP_ERR_ESPNOW_NO_MEM:
      Serial.println("Out of memory");
      break;

    case ESP_ERR_ESPNOW_NOT_FOUND:
      Serial.println("Peer is not found");
      break;

    case ESP_ERR_ESPNOW_IF:
      Serial.println("Current WiFi interface doesn't match that of peer");
      break;

    default:
      break;
  }
}

static void msg_recv_cb(const uint8_t *mac_addr, const uint8_t *data, int len)
{
  if (len == sizeof(esp_now_msg_t))
  {
    esp_now_msg_t msg;
    memcpy(&msg, data, len);

#ifdef RECEIVER
    // Immediately send back same message
    send_msg(&msg);
#endif

#ifdef SENDER
    // We just received the same message back
    // OK to do slow printing here, since outside of timer
    error_counter--;
    recv_back_time_ms = millis();
    Serial.print(PAYLOAD_EXTRA_BYTES+8);
    Serial.print(",");
    Serial.print(recv_back_time_ms-send_time_ms);
    Serial.print(",");
    Serial.print(g_counter);
    Serial.print(",");
    Serial.println(error_counter);
#endif

  }
}


static void send_msg(esp_now_msg_t * msg)
{
  // Pack
  uint16_t packet_size = sizeof(esp_now_msg_t);
  uint8_t msg_data[packet_size];
  memcpy(&msg_data[0], msg, sizeof(esp_now_msg_t));

  esp_err_t status = esp_now_send(broadcast_mac, msg_data, packet_size);
  if (ESP_OK != status)
  {
    Serial.println("Error sending message");
    handle_error(status);
  }
#ifdef SENDER
  send_time_ms = millis();
#endif
}

static void network_setup(void)
{
  //Puts ESP in STATION MODE
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != 0)
  {
    return;
  }

  esp_now_peer_info_t peer_info;
  peer_info.channel = WIFI_CHANNEL;
  memcpy(peer_info.peer_addr, broadcast_mac, 6);
  peer_info.ifidx = ESP_IF_WIFI_STA;
  peer_info.encrypt = false;
  esp_err_t status = esp_now_add_peer(&peer_info);
  if (ESP_OK != status)
  {
    Serial.println("Could not add peer");
    handle_error(status);
  }

  // Set up callback
  status = esp_now_register_recv_cb(msg_recv_cb);
  if (ESP_OK != status)
  {
    Serial.println("Could not register receive callback");
    handle_error(status);
  }

  // No send callback, may effect latency
}

void setup() {
  Serial.begin(115200);

  network_setup();
}

void loop() {

  if (g_counter < MESSAGES_TO_SEND)
  {
    delay(MESSAGE_SEND_INTERVAL);

#ifdef SENDER
    // Increment error counter here, decrement when receive back
    // If not decremented, that message was missed 
    error_counter++;
    esp_now_msg_t msg;
    msg.address = 0;
    msg.counter = g_counter++;
    send_msg(&msg);
#endif

  }
  else
  {
    delay(1000);
    Serial.println("Done");
    delay(1000000);
  }
}
