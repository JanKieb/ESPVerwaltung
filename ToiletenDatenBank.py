import board
import busio
import socketpool
import wifi
import adafruit_minimqtt.adafruit_minimqtt as MQTT
import adafruit_requests
import ssl
import json
import adafruit_sqlite

# WiFi credentials
WIFI_SSID = "your_ssid"
WIFI_PASSWORD = "your_password"

# WebSocket server details
WS_HOST = "your_websocket_server"
WS_PORT = 443

# Database setup
DB_FILE = "messages.db"

def setup_wifi():
    print("Connecting to WiFi...")
    wifi.radio.connect(WIFI_SSID, WIFI_PASSWORD)
    print(f"Connected to {WIFI_SSID}")
    print(f"IP Address: {wifi.radio.ipv4_address}")

def setup_database():
    conn = adafruit_sqlite.connect(DB_FILE)
    cursor = conn.cursor()
    
    # Create table if it doesn't exist
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS messages (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            message TEXT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    """)
    conn.commit()
    return conn

def handle_message(client, message):
    try:
        # Parse message
        data = json.loads(message)
        
        # Store in database
        cursor = db_conn.cursor()
        cursor.execute(
            "INSERT INTO messages (message) VALUES (?)",
            (json.dumps(data),)
        )
        db_conn.commit()
        print("Message stored in database")
    except Exception as e:
        print(f"Error handling message: {e}")

# Main program
try:
    # Setup WiFi
    setup_wifi()
    
    # Setup database
    db_conn = setup_database()
    
    # Setup WebSocket
    pool = socketpool.SocketPool(wifi.radio)
    requests = adafruit_requests.Session(pool, ssl.create_default_context())
    
    # Setup MQTT client (using MQTT as WebSocket alternative)
    mqtt_client = MQTT.MQTT(
        broker=WS_HOST,
        port=WS_PORT,
        socket_pool=pool,
        ssl_context=ssl.create_default_context(),
    )
    
    # Set callback
    mqtt_client.on_message = handle_message
    
    # Connect and subscribe
    print("Connecting to MQTT broker...")
    mqtt_client.connect()
    mqtt_client.subscribe("messages/#")
    
    # Main loop
    while True:
        mqtt_client.loop()

except Exception as e:
    print(f"Error: {e}")
    
finally:
    if 'db_conn' in locals():
        db_conn.close()