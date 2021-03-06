#include <Arduino.h>
#include "Adafruit_WINC1500.h"
#include "Adafruit_WINC1500Client.h"

#define PIN_WINC_CS   8
#define PIN_WINC_IRQ  7
#define PIN_WINC_RST  4
#define PIN_WINC_EN   2

Adafruit_WINC1500 WiFi(PIN_WINC_CS, PIN_WINC_IRQ, PIN_WINC_RST);
Adafruit_WINC1500Client client;

const uint32_t ONE_MINUTE = 60 * 1000;
const char *SSID = "Cottonwood";
const char *PSK = "asdfasdf";
const char *SERVER = "conservify.page5of4.com";
const char *PATH = "/monitor/logs";

void http_post(const char *body) {
    if (client.connect(SERVER, 80)) {
        Serial.println("Connected, sending:");

        client.print("POST ");
        client.print(PATH);
        client.println(" HTTP/1.1");
        client.print("Host: "); client.println(SERVER);
        client.print("Content-Type: "); client.println("text");
        client.print("Content-Length: "); client.println(strlen(body));
        client.println("Connection: close");
        client.println();
        client.println(body);

        uint32_t started = millis();
        while (millis() - started < ONE_MINUTE) {
            delay(10);

            while (client.available()) {
                Serial.write(client.read());
            }

            if (!client.connected()) {
                Serial.println("Closed");
                client.stop();
                break;
            }
        }
    }
}

void setup() {
    Serial.begin(115200);
    Serial1.begin(9600);

    while (millis() < 1000 * 10) {
        if (Serial) {
            break;
        }
        delay(500);
    }

    pinMode(PIN_WINC_EN, OUTPUT);
    digitalWrite(PIN_WINC_EN, HIGH);

    if (WiFi.status() == WL_NO_SHIELD) {
        Serial.println("Wifi: missing");
        return;
    }

    int32_t status;
    uint32_t started = millis();
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print("Wifi: attempting ");
        Serial.println(SSID);

        status = WiFi.begin(SSID, PSK);

        uint8_t seconds = 10;
        while (seconds > 0 && (WiFi.status() != WL_CONNECTED)) {
            seconds--;
            delay(1000);
        }

        if (millis() - started > ONE_MINUTE) {
            break;
        }
    }

    Serial.println("Connected");

    http_post("Start");

    Serial.println("Ready");
}

const uint32_t LOGBUFFER_SIZE = 4096;
const uint32_t LOGBUFFER_IDLE_DELAY = 2000;

typedef struct log_buffer_t {
    char buffer[LOGBUFFER_SIZE];
    uint16_t position;
} log_buffer_t;

void logbuffer_create(log_buffer_t *buffer) {
    buffer->position = 0;
}

char *strrchr(char *src, char needle) {
    char *looking = src;
    char *found = nullptr;

    while (*looking != 0) {
        if (*looking == needle) {
            found = looking;
        }
        looking++;
    }

    return found;
}

void logbuffer_flush(log_buffer_t *buffer) {
    if (buffer->position > 0) {
        buffer->buffer[buffer->position] = 0;
        char *lastChar = strrchr(buffer->buffer, '\n');
        if (lastChar == nullptr) {
            lastChar = strrchr(buffer->buffer, '\r');
        }
        if (lastChar != nullptr) {
            size_t position = lastChar - buffer->buffer;

            // Post up the final full line to the server.
            buffer->buffer[position] = 0;
            http_post(buffer->buffer);

            // Terminate the remainder...
            char *ptr = buffer->buffer;
            char *tail = lastChar + 1;
            for (size_t i = position; i < buffer->position; ++i) {
                if (*tail != 0) {
                    *ptr++ = *tail++;
                }
            }

            size_t old = buffer->position;
            buffer->position = ptr - buffer->buffer;
            /*
            Serial.println("Done: ");
            Serial.println(old);
            Serial.println(buffer->position);
            Serial.println(position);
            Serial.print("'");
            Serial.print(buffer->buffer);
            Serial.print("'");
            Serial.println();
            */
        }
        else {
            /*
            Serial.println("No newline: ");
            Serial.println(buffer->position);
            Serial.print("'");
            Serial.print(buffer->buffer);
            Serial.print("'");
            Serial.println();
            */
        }
    }
}

void logbuffer_append(log_buffer_t *buffer, char c) {
    buffer->buffer[buffer->position] = c;
    buffer->position++;
    if (buffer->position == LOGBUFFER_SIZE - 2) {
        logbuffer_flush(buffer);
    }
}

void loop() {
    log_buffer_t logbuffer;
    logbuffer_create(&logbuffer);

    uint32_t lastActivity = millis();

    while (true) {
        // Stream *stream = Serial ? &Serial : (Stream *)&Serial1;
        Stream *stream = (Stream *)&Serial1;
        while (stream->available()) {
            char c = stream->read();
            Serial.write(c);
            lastActivity = millis();
            logbuffer_append(&logbuffer, c);
        }

        if (millis() - lastActivity > LOGBUFFER_IDLE_DELAY) {
            logbuffer_flush(&logbuffer);
            lastActivity = millis();
        }

        delay(10);
    }
}
