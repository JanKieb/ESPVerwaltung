#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

void sendLog(String message);

// Netzwerkdaten
const char* ssid = "ToilettenAP";
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
String previousSlotId = "";

static int lastRequestButtonState = HIGH;
static int lastReleaseButtonState = HIGH;

//checks for sync mismatches
unsigned long lastSyncCheck = 0;
const unsigned long SYNC_CHECK_INTERVAL = 30000; // Check every 30 seconds

// WebSocket
WebSocketsClient webSocket;
const char* websocket_server = "192.168.4.1"; // IP des ESP1 (Access Point IP)

// Benutzer-ID
String userId = "ESP2Client"; 

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
  webSocket.enableHeartbeat(15000, 3000, 2); // Heartbeat mit 15 Sekunden Intervall und 3x 3 Sekunden Timeout

}

void loop() {
  webSocket.loop(); // WebSocket-Verbindung aufrechterhalten

  // Handle button inputs
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

  // Check for sync mismatches
  unsigned long currentMillis = millis();
  if (currentMillis - lastSyncCheck >= SYNC_CHECK_INTERVAL) {
    lastSyncCheck = currentMillis;
    checkSyncState();
  }

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
      //sync message from ESP1
      String payloadStr = String((char*)payload);

      Serial.println("Received message: " + message);
      // Parse JSON message
      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, message);
      if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.f_str());
        return;
      }
      
      if (payloadStr.indexOf("statusResponse") > -1) {
        handleStateSync(payloadStr);
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
        String currentSlotId = "";
        for (JsonVariant slot : slots) {
            if (slot["userId"] == userId) {
                hasSlot = true;
                currentSlotId = slot["id"].as<String>();  // Assuming each slot has an ID
                
                // Blink if assigned to a different slot than before
                if (currentSlotId != previousSlotId) {
                    slotAssignedBlink();
                }
                
                previousSlotId = currentSlotId;
                break;
            }
        }
        // If no slot is assigned, reset the previousSlotId
        if (!hasSlot) {
            previousSlotId = "";
        }
        
        // Check if user is in queue
        bool inQueue = false;
        JsonArray queue = doc["queue"];
        for (JsonVariant slot : queue) {
          String slotUserId = slot["userId"].as<String>();
          if (slotUserId == userId) {
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
        digitalWrite(relaySlotHeldPin, HIGH);
        Serial.println("relayInQueuePin set to HIGH");
    }
    else if (status == "ableToRequest") {
        digitalWrite(relayRequestablePin, HIGH);
        Serial.println("relayRequestablePin set to HIGH");
    }
    else {
        digitalWrite(relaySlotHeldPin, HIGH);
        Serial.println("relaySlotHeldPin set to HIGH");
        Serial.println("Unknown status: " + status);
    }
  }
  RelayPreviousState = status;
}


void sendRestartLog(String reason) {

  Serial.println("Restart triggered: " + reason);

  String message = "Client " + userId + " restarting: " + reason;


  sendLog(message);
  delay(100); // Brief delay to ensure message is sent
  //ESP.restart();
}

void checkSyncState() {
  // Create JSON message to request server state
  DynamicJsonDocument doc(256);
  doc["action"] = "checkStatus";
  doc["userId"] = userId;

  String message;
  serializeJson(doc, message);
  webSocket.sendTXT(message);
}

void handleStateSync(String payload) {
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, payload);
  
  if (error) {
    Serial.println("Failed to parse state sync response");
    return;
  }

  // Check if it's a status response message
  if (doc["type"] == "statusResponse") {
    bool foundSlot = false;
    bool stateMatch = true;
    
    // Compare local state with server state
    JsonArray slots = doc["parkingSlots"];
    
    // Check if our slot held status matches server state
    for (JsonVariant slot : slots) {
      if (slot["userId"] == userId) {
        foundSlot = true;
        stateMatch = (slot["occupied"].as<bool>() == slotHeldPreviousState);
        break;
      }
    }

    // Only restart if we found our slot and states don't match
    if (foundSlot && !stateMatch) {
      String reason = "State mismatch - local: " + String(slotHeldPreviousState) + 
                     ", server: " + String(!slotHeldPreviousState);
      sendRestartLog(reason);
    }
  }
}


void sendLog(String message) {
  DynamicJsonDocument doc(256);
  doc["type"] = "log"; 
  doc["message"] = message;

  // Serialize JSON document to a string
  String output;
  serializeJson(doc, output);

  // Send the JSON string to the WebSocket server
  webSocket.sendTXT(output);

  // Local debug output
  Serial.println("Log sent: " + output);
}




