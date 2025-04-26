#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <Esp.h>

const char *ssid = "ToilettenAP";
const char *password = "12345678";
const String ADMIN_ID = "admin123";

void releaseParking(uint8_t num, String userId, bool isAdminRelease, int adminSlotIndex = -1);

WebSocketsServer webSocket = WebSocketsServer(81);
WiFiServer server(80);

const int totalSlots = 2;
struct ParkingSlot {
  bool occupied;
  String userId;
};
ParkingSlot parkingSlots[totalSlots];

struct QueueItem {
  String userId;
};
QueueItem queue[10];
int queueSize = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("Configuring access point...");

  if (!WiFi.begin(ssid, password)) {
    log_e("Soft AP creation failed.");
    while (1);
  }
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  webSocket.enableHeartbeat(15000, 3000, 2); // Heartbeat every 15 seconds

  for (int i = 0; i < totalSlots; i++) {
    parkingSlots[i] = {false, ""};
  }

  Serial.println("Server started");
}

void loop() {
  webSocket.loop();

  WiFiClient client = server.available();
  if (client) {
    Serial.println("New Client.");
    String currentLine = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        if (c == '\n') {
          if (currentLine.length() == 0) {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();
            client.print(htmlContent());
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }
    client.stop();
    Serial.println("Client Disconnected.");
  }
}

String htmlContent() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="de">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Toilettenverwaltung</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            background-color: #f5f5f5;
            margin: 0;
            padding: 0;
            display: flex;
            justify-content: center;
            align-items: center;
            height: 100vh;
            color: #333;
        }
        .container {
            background-color: #fff;
            padding: 20px;
            border-radius: 8px;
            box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1);
            width: 80%;
            max-width: 600px;
        }
        .header {
            text-align: center;
            margin-bottom: 20px;
        }
        .header h1 {
            margin: 0;
            font-size: 1.8em;
            color: #4CAF50;
        }
        .slots, .actions {
            margin-bottom: 20px;
        }
        .slots button, .actions button {
            width: 100%;
            padding: 10px;
            margin-bottom: 10px;
            font-size: 1em;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            transition: background-color 0.3s;
        }
        .slots > div {
            display: flex;
            justify-content: space-between;
            margin-bottom: 10px;
        }
        .slots div button {
            width: 48%;
        }
        .request {
            background-color: #4CAF50;
            color: white;
        }
        .release {
            background-color: #f44336;
            color: white;
        }
        .request:hover {
            background-color: #45a049;
        }
        .release:hover {
            background-color: #e53935;
        }
        .queue {
            text-align: center;
        }
        .queue h2 {
            margin: 0;
            margin-bottom: 10px;
            font-size: 1.5em;
        }
        .log {
            margin-top: 20px;
            background-color: #f5f5f5;
            padding: 10px;
            border: 1px solid #ddd;
            border-radius: 4px;
            height: 150px;
            overflow-y: auto;
        }
        .log-entry {
            margin-bottom: 5px;
            padding-bottom: 5px;
            border-bottom: 1px solid #ddd;
        }
        .admin-controls {
            margin-top: 20px;
            padding: 10px;
            background-color: #f0f0f0;
            border-radius: 5px;
        }
        .status {
            margin-top: 20px;
            padding: 10px;
            background-color: #f0f0f0;
            border-radius: 5px;
            text-align: center;
        }
        .restart {
            background-color: #ff9800;
            color: white;
            margin-top: 10px;
        }
        .restart:hover {
            background-color: #f57c00;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>Toilettenverwaltung</h1>
        </div>
        <div id="parking-slots" class="slots">
            <!-- Dynamisch generierte Toiletten erscheinen hier -->
        </div>
        <div class="actions">
            <button onclick="requestParking()" class="request">Toilette anfordern</button>
        </div>
        <div class="slot-select">
            <button onclick="releaseParking()" class="release">Toilette freigeben</button>
        </div>
        <div id="admin-controls" class="admin-controls" style="display: none;">
            <h2>Admin-Steuerung</h2>
            <select id="admin-release-slot-select">
                <!-- Alle belegten Toiletten erscheinen hier -->
            </select>
            <button onclick="adminReleaseParking()" class="release">Admin-Freigabe</button>
            <button onclick="restartServer()" class="restart">Server neustarten</button>
        </div>
        <div id="queue" class="queue">
            <h2>Warteschlange</h2>
            <ul id="queue-list">
                <!-- Dynamisch generierte Warteschlange erscheint hier -->
            </ul>
        </div>
        <div class="log" id="log">
            <!-- Log-Einträge erscheinen hier -->
        </div>
        <div id="status" class="status">
            Verbindungstatus: <span id="connection-status">Verbunden</span>
        </div>
        <button onclick="checkStatus()">Check Status</button>
    </div>

    <script>
var totalSlots = 5;
var userId = prompt("Bitte geben Sie Ihren Nutzer-ID ein:");
var isAdmin = (userId === "admin123");
var parkingSlots = new Array(totalSlots).fill(null);
var queue = [];
var socket;

// Add a variable to track the connection status
var connectionAlive = true;
var keepAliveInterval;

function connectWebSocket() {
    socket = new WebSocket('ws://' + window.location.hostname + ':81/');
    
    socket.onopen = function(event) {
        console.log('WebSocket Verbindung hergestellt');
        addLogEntry('Verbunden mit dem Toilettensystem');
        updateConnectionStatus(true);

        // Start sending keep-alive messages every 30 seconds
        keepAliveInterval = setInterval(function() {
            sendKeepAlive();
        }, 30000);
    };

    socket.onmessage = function(event) {
        var data = JSON.parse(event.data);
        if (data.type === 'update') {
            parkingSlots = data.parkingSlots;
            queue = data.queue;
            renderSlots();
            renderQueue();
        } else if (data.type === 'statusResponse') {
            var serverParkingSlots = data.parkingSlots;
            var serverQueue = data.queue;

            if (JSON.stringify(parkingSlots) !== JSON.stringify(serverParkingSlots) ||
                JSON.stringify(queue) !== JSON.stringify(serverQueue)) {
                addLogEntry('Local state is out of sync. Updating local state.');
                parkingSlots = serverParkingSlots;
                queue = serverQueue;
                renderSlots();
                renderQueue();
            } else {
                addLogEntry('Local state is in sync with the server.');
            }
        }
        else if (data.type === 'log') {
            addLogEntry(data.message);
        } else if (data.type === 'keepAlive') {
            // Server responded to keep-alive
            connectionAlive = true;
            updateConnectionStatus(true);
        }
    };

    socket.onclose = function(event) {
        console.log('WebSocket Verbindung geschlossen');
        addLogEntry('Verbindung verloren. Versuche erneut zu verbinden...');
        updateConnectionStatus(false);
        clearInterval(keepAliveInterval);
        setTimeout(connectWebSocket, 5000);
    };
}

function sendToServer(action, data) {
    if (socket.readyState === WebSocket.OPEN) {
        socket.send(JSON.stringify({
            action: action,
            userId: userId,
            isAdmin: isAdmin,
            data: data
        }));
    } else {
        console.log('WebSocket nicht verbunden. Versuche erneut zu verbinden...');
        connectWebSocket();
    }
}

// Function to send keep-alive message
function sendKeepAlive() {
    connectionAlive = false;
    sendToServer('keepAlive', {});
    // If no response within 10 seconds, consider connection lost
    setTimeout(function() {
        if (!connectionAlive) {
            updateConnectionStatus(false);
        }
    }, 10000);
}

// Function to update connection status on the page
function updateConnectionStatus(isConnected) {
    var statusElement = document.getElementById('connection-status');
    if (isConnected) {
        statusElement.textContent = 'Verbunden';
        statusElement.style.color = 'green';
    } else {
        statusElement.textContent = 'Verbindung verloren';
        statusElement.style.color = 'red';
    }
}

function requestParking() {
    sendToServer('request', {});
}

function releaseParking() {
    sendToServer('release', {});
}

function adminReleaseParking() {
    var slotIndex = document.getElementById('admin-release-slot-select').value;
    if (slotIndex !== "") {
        sendToServer('adminRelease', { slotIndex: parseInt(slotIndex) });
    } else {
        addLogEntry("Bitte wählen Sie eine Toilette zum Freigeben aus.");
    }
}

function restartServer() {
    if (confirm("Sind Sie sicher, dass Sie den Server neu starten möchten?")) {
        sendToServer('restart', {});
        addLogEntry("Server-Neustart initiiert...");
    }
}

function checkStatus() {
    sendToServer('checkStatus', {});
}

function renderSlots() {
    var parkingSlotsDiv = document.getElementById('parking-slots');
    parkingSlotsDiv.innerHTML = '';
    for (var i = 0; i < parkingSlots.length; i++) {
        var slot = parkingSlots[i];
        var slotDiv = document.createElement('div');
        slotDiv.className = 'parking-slot ' + (slot.occupied ? 'occupied' : 'free');
        slotDiv.innerHTML = 'Toilette ' + (i + 1) + ': ' + 
            (slot.occupied ? 'Belegt von ' + slot.userId : 'Frei');
        parkingSlotsDiv.appendChild(slotDiv);
    }
    updateAdminReleaseSlotSelect();
}

function updateAdminReleaseSlotSelect() {
    if (isAdmin) {
        document.getElementById('admin-controls').style.display = 'block';
        var select = document.getElementById('admin-release-slot-select');
        select.innerHTML = '<option value="">Wählen Sie eine Toilette</option>';
        for (var i = 0; i < parkingSlots.length; i++) {
            if (parkingSlots[i].occupied) {
                var option = document.createElement('option');
                option.value = i;
                option.textContent = 'Toilette ' + (i + 1) + ' - ' + parkingSlots[i].userId;
                select.appendChild(option);
            }
        }
    }
}

function renderQueue() {
    var queueList = document.getElementById('queue-list');
    queueList.innerHTML = '';
    queue.forEach(function(item, index) {
        var listItem = document.createElement('li');
        listItem.textContent = 'Nutzer: ' + item.userId;
        queueList.appendChild(listItem);
    });
}

function addLogEntry(message) {
    var logDiv = document.getElementById('log');
    var logEntry = document.createElement('div');
    logEntry.className = 'log-entry';
    logEntry.innerHTML = '<span style="font-weight: bold;">' + new Date().toLocaleString() + '</span>: ' + message;
    logDiv.appendChild(logEntry);
    logDiv.scrollTop = logDiv.scrollHeight;
}

// Initialisierung
connectWebSocket();
renderSlots();
renderQueue();

    </script>
</body>
</html>
  )rawliteral";
  return html;
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", num);
      break;
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
        sendUpdate(num);
      }
      break;
    case WStype_TEXT:
      {
        String text = String((char*) payload);
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, text);

        String action = doc["action"];
        String userId = doc["userId"];
        bool isAdmin = doc["isAdmin"];

        if (action == "request") {
            requestParking(num, userId);
        } else if (action == "release") {
            releaseParking(num, userId, false);
        } else if (action == "adminRelease" && isAdmin) {
            int slotIndex = doc["data"]["slotIndex"];
            releaseParking(num, userId, true, slotIndex);
        } else if (action == "keepAlive") {
            // Respond to keep-alive message
            DynamicJsonDocument responseDoc(256);
            responseDoc["type"] = "keepAlive";
            String response;
            serializeJson(responseDoc, response);
            webSocket.sendTXT(num, response);
        } else if (action == "checkStatus") {
            sendStatusResponse(num);
        } else if (action == "restart" && isAdmin) {
            restartServer();
        } else if (doc["type"] == "log") {
            // Handle log messages
            String logMessage = doc["message"];
            Serial.println("Log from client: " + logMessage);
            sendLog(logMessage);
        }
      }
      break;
  }
}

void sendUpdate(uint8_t num) {
  DynamicJsonDocument doc(1024);
  doc["type"] = "update";
  
  JsonArray slotsArray = doc.createNestedArray("parkingSlots");
  for (int i = 0; i < totalSlots; i++) {
    JsonObject slotObj = slotsArray.createNestedObject();
    slotObj["occupied"] = parkingSlots[i].occupied;
    slotObj["userId"] = parkingSlots[i].userId;
  }
  
  JsonArray queueArray = doc.createNestedArray("queue");
  for (int i = 0; i < queueSize; i++) {
    JsonObject queueItem = queueArray.createNestedObject();
    queueItem["userId"] = queue[i].userId;
  }
  
  String output;
  serializeJson(doc, output);
  webSocket.sendTXT(num, output);
}

void sendLog(String message) {
  DynamicJsonDocument doc(256);
  doc["type"] = "log";
  doc["message"] = message;
  
  String output;
  serializeJson(doc, output);
  webSocket.broadcastTXT(output);
}

bool userCanRequestSlot(String userId) {
  for (int i = 0; i < totalSlots; i++) {
    if (parkingSlots[i].occupied && parkingSlots[i].userId == userId) {
      return false;
    }
  }

  for (int i = 0; i < queueSize; i++) {
    if (queue[i].userId == userId) {
      return false;
    }
  }

  return true;
}

void requestParking(uint8_t num, String userId) {
  if (!userCanRequestSlot(userId)) {
    sendLog("Nutzer " + userId + " hat bereits einen Toilettenplatz belegt oder angefordert.");
    sendUpdate(num);
    return;
  }

  int slotIndex = -1;
  for (int i = 0; i < totalSlots; i++) {
    if (!parkingSlots[i].occupied) {
      slotIndex = i;
      break;
    }
  }

  if (slotIndex != -1) {
    parkingSlots[slotIndex].occupied = true;
    parkingSlots[slotIndex].userId = userId;
    sendLog("Nutzer " + userId + " hat Toilettenplatz " + String(slotIndex + 1) + " zugewiesen bekommen.");
  } else {
    if (queueSize < 10) {
      queue[queueSize] = {userId};
      queueSize++;
      sendLog("Nutzer " + userId + " wurde zur Warteschlange hinzugefügt.");
    } else {
      sendLog("Warteschlange ist voll. Nutzer " + userId + " konnte nicht hinzugefügt werden.");
    }
  }

  for (uint8_t i = 0; i < webSocket.connectedClients(); i++) {
    sendUpdate(i);
  }
}

void releaseParking(uint8_t num, String userId, bool isAdminRelease, int adminSlotIndex) {
  // First check if user is in queue
  bool userRemovedFromQueue = false;
  for (int i = 0; i < queueSize; i++) {
    if (queue[i].userId == userId) {
      // Remove user from queue by shifting remaining entries
      for (int j = i; j < queueSize - 1; j++) {
        queue[j] = queue[j + 1];
      }
      queueSize--;
      userRemovedFromQueue = true;
      sendLog("Nutzer " + userId + " wurde aus der Warteschlange entfernt.");
      break;
    }
  }

  // If user was in queue we're done after removing them
  if (userRemovedFromQueue) {
    for (uint8_t i = 0; i < webSocket.connectedClients(); i++) {
      sendUpdate(i);
    }
    return;
  }

  // Existing parking slot release logic
  int slotIndex = -1;
  if (isAdminRelease && adminSlotIndex >= 0) {
    slotIndex = adminSlotIndex;
  } else {
    for (int i = 0; i < totalSlots; i++) {
      if (parkingSlots[i].occupied && parkingSlots[i].userId == userId) {
        slotIndex = i;
        break;
      }
    }
  }

  if (slotIndex >= 0 && slotIndex < totalSlots) {
    if (parkingSlots[slotIndex].occupied && (parkingSlots[slotIndex].userId == userId || isAdminRelease)) {
      String releasedUserId = parkingSlots[slotIndex].userId;
      parkingSlots[slotIndex].occupied = false;
      parkingSlots[slotIndex].userId = "";

      if (isAdminRelease) {
        sendLog("Admin hat Toilettenplatz " + String(slotIndex + 1) + " für Nutzer " + releasedUserId + " freigegeben.");
      } else {
        sendLog("Nutzer " + userId + " hat Toilettenplatz " + String(slotIndex + 1) + " freigegeben.");
      }

      // Reassign slot to the next person in the queue
      if (queueSize > 0) {
        parkingSlots[slotIndex].occupied = true;
        parkingSlots[slotIndex].userId = queue[0].userId;
        sendLog("Toilettenplatz " + String(slotIndex + 1) + " wurde neu zugewiesen an Nutzer: " + queue[0].userId);

        // Remove the assigned user from the queue
        for (int k = 0; k < queueSize - 1; k++) {
          queue[k] = queue[k + 1];
        }
        queueSize--;
      }
    } else {
      sendLog("Fehler: Nutzer " + userId + " kann Toilettenplatz " + String(slotIndex + 1) + " nicht freigeben, da er ihn nicht belegt.");
    }
  } else if (!userRemovedFromQueue) {
    sendLog("Fehler: Kein gültiger Toilettenplatz oder Warteschlangenplatz gefunden für die Freigabe.");
  }

  for (uint8_t i = 0; i < webSocket.connectedClients(); i++) {
    sendUpdate(i);
  }
}

void sendStatusResponse(uint8_t num) {
    DynamicJsonDocument doc(2048);
    doc["type"] = "statusResponse";

    JsonArray parkingSlotsArray = doc.createNestedArray("parkingSlots");
    for (int i = 0; i < totalSlots; i++) {
        JsonObject slotObj = parkingSlotsArray.createNestedObject();
        slotObj["occupied"] = parkingSlots[i].occupied;
        slotObj["userId"] = parkingSlots[i].userId;
    }

    JsonArray queueArray = doc.createNestedArray("queue");
    for (int i = 0; i < queueSize; i++) {
        JsonObject queueObj = queueArray.createNestedObject();
        queueObj["userId"] = queue[i].userId;
    }

    String jsonString;
    serializeJson(doc, jsonString);
    webSocket.sendTXT(num, jsonString);
}

void restartServer() {
    // Broadcast a message about the restart
    sendLog("Admin initiated server restart. Rebooting in 3 seconds...");
    delay(3000);
    ESP.restart();
}
