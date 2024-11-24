#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

// Netzwerkdaten
const char* ssid = "ParkplatzAP";
const char* password = "12345678";

// Define button GPIO pins
const int requestButtonPin = 15;  // GPIO pin for the request button
const int releaseButtonPin = 16;  // GPIO pin for the release button

// Define relay GPIO pins
const int relaySlotHeldPin = 2;    // Relay for slot held status
const int relayInQueuePin = 4;     // Relay for in queue status
const int relayRequestablePin = 5; // Relay for able to request status

bool slotHeldPreviousState = false;
String RelayPreviousState = "";

// WebSocket
WebSocketsClient webSocket;
const char* websocket_server = "192.168.4.1"; // IP des ESP1 (Access Point IP)

// Benutzer-ID
String userId = "user456"; 

void setup() {
  Serial.begin(115200);

  // Initialize button pins
  pinMode(requestButtonPin, INPUT_PULLUP);
  pinMode(releaseButtonPin, INPUT_PULLUP);

  // Initialize relay pins
  pinMode(relaySlotHeldPin, OUTPUT);
  pinMode(relayInQueuePin, OUTPUT);
  pinMode(relayRequestablePin, OUTPUT);

// Set all relays off initially
  digitalWrite(relaySlotHeldPin, LOW);
  digitalWrite(relayInQueuePin, LOW);
  digitalWrite(relayRequestablePin, LOW);

  // Verbindung mit WLAN herstellen
  Serial.print("Verbinde mit WLAN: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("WLAN verbunden.");
  Serial.print("IP-Adresse: ");
  Serial.println(WiFi.localIP());

  // WebSocket-Verbindung starten
  webSocket.begin(websocket_server, 81, "/"); // Port 81 wird für WebSocket verwendet
  webSocket.onEvent(webSocketEvent); // Event-Funktion
  webSocket.setReconnectInterval(5000); // Automatische Wiederverbindung nach 5 Sekunden
}

void loop() {
  webSocket.loop(); // WebSocket-Verbindung aufrechterhalten

  // Handle button inputs
  static int lastRequestButtonState = HIGH;
  static int lastReleaseButtonState = HIGH;

  int requestButtonState = digitalRead(requestButtonPin);
  int releaseButtonState = digitalRead(releaseButtonPin);

  // Check if request button is pressed
  if (requestButtonState == LOW && lastRequestButtonState == HIGH) {
    Serial.println("Request button pressed");
    sendRequestParking();
  }
  lastRequestButtonState = requestButtonState;

  // Check if release button is pressed
  if (releaseButtonState == LOW && lastReleaseButtonState == HIGH) {
    Serial.println("Release button pressed");
    sendReleaseParking();
  }
  lastReleaseButtonState = releaseButtonState;

  delay(10); // Debounce delay
}

// WebSocket-Eventhandler

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("WebSocket Disconnected");
      break;
    case WStype_CONNECTED:
      Serial.println("WebSocket Connected");
      break;
    case WStype_TEXT: {
      String message = String((char *) payload);
      Serial.println("Received message: " + message);
      // Parse JSON message
      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, message);
      if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.f_str());
        return;
      }
      
      String messageType = doc["type"] | "";
      if (messageType == "update") {
        // Check if all slots are occupied
        bool allSlotsOccupied = true;
        JsonArray slots = doc["parkingSlots"];
        for (JsonVariant slot : slots) {
          if (!slot["occupied"]) {
            allSlotsOccupied = false;
            break;
          }
        }
        
        // Check if this user has a slot
        bool hasSlot = false;
        for (JsonVariant slot : slots) {
          if (slot["userId"] == userId) {
            hasSlot = true;
            if (hasSlot && slotHeldPreviousState == false)
            {
              slotAssignedBlink();
            }
            slotHeldPreviousState = true;
            break;
          }
        }
        
        // Check if user is in queue
        bool inQueue = false;
        JsonArray queue = doc["queue"];
        for (JsonVariant queueUser : queue) {
          if (queueUser.as<String>() == userId) {
            inQueue = true;
            slotHeldPreviousState = false;
            break;

          }
        }
        
        // Update relay status
        if (hasSlot) {
          updateRelays("slotHeld");
        } else if (inQueue) {
          updateRelays("inQueue");
        } else if (allSlotsOccupied) {
          updateRelays("held");  // New state for when all slots are occupied
        } else {
          updateRelays("ableToRequest");
        }
      }
      break;
    }
    default:
      break;
  }
}

// Funktion zur Verarbeitung von WebSocket-Nachrichten
void handleWebSocketMessage(String message) {
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, message);
  String type = doc["type"];
  
  if (type == "update") {
    // Verarbeite die Parkplatzzustände
    Serial.println("Parkplatzstatus aktualisiert.");
    JsonArray slots = doc["parkingSlots"];
    for (int i = 0; i < slots.size(); i++) {
      bool occupied = slots[i]["occupied"];
      String userId = slots[i]["userId"];
      Serial.printf("Slot %d: %s von %s\n", i+1, occupied ? "Belegt" : "Frei", userId.c_str());
    }
  } else if (type == "log") {
    String logMessage = doc["message"];
    Serial.println("Log: " + logMessage);
  }
}

// Funktion, um eine Parkplatzanforderung zu senden
void sendRequestParking() {
  DynamicJsonDocument doc(256);
  doc["action"] = "request";
  doc["userId"] = userId;
  String message;
  serializeJson(doc, message);
  webSocket.sendTXT(message);
}

void sendReleaseParking() {
  DynamicJsonDocument doc(256);
  doc["action"] = "release";
  doc["userId"] = userId;
  String message;
  serializeJson(doc, message);
  webSocket.sendTXT(message);
}


void slotAssignedBlink() {
  Serial.println("slotAssignedBlink called");
  for (int i = 0; i < 8; i++) {
    digitalWrite(relaySlotHeldPin, HIGH);
    digitalWrite(relayInQueuePin, HIGH);
    digitalWrite(relayRequestablePin, HIGH);
    delay(300);
    digitalWrite(relaySlotHeldPin, LOW);
    digitalWrite(relayInQueuePin, LOW);
    digitalWrite(relayRequestablePin, LOW);
    delay(300);
  }
}



void updateRelays(String status) {
    Serial.println("updateRelays called with status: " + status);

  if (status != RelayPreviousState)
  {
  
    // First turn all relays off
    digitalWrite(relaySlotHeldPin, LOW);
    digitalWrite(relayInQueuePin, LOW);
    digitalWrite(relayRequestablePin, LOW);
    Serial.println("All relays set to LOW");

    // Then set only the appropriate relay HIGH based on status
    if (status == "slotHeld") {
        digitalWrite(relaySlotHeldPin, HIGH);
        Serial.println("relaySlotHeldPin set to HIGH");
    }
    else if (status == "inQueue") {
        digitalWrite(relayInQueuePin, HIGH);
        Serial.println("relayInQueuePin set to HIGH");
    }
    else if (status == "ableToRequest") {
        digitalWrite(relayRequestablePin, HIGH);
        Serial.println("relayRequestablePin set to HIGH");
    }
    else {
        Serial.println("Unknown status: " + status);
    }
  }
  RelayPreviousState = status;
}


