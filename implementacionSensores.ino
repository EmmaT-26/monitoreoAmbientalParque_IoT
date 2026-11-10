#include <Wire.h>
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <PubSubClient.h>

// --- Configuración de Sensores ---
#define DHTPIN 14
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define UV_PIN 32
#define MIC_PIN 33

// --- Configuración OLED ---
Adafruit_SSD1306 display(128, 64, &Wire, -1);

// --- Configuración Red (Access Point) ---
const char* ssid = "ESP32_Escuela";
const char* pass = "12345678";

// --- Configuración MQTT IP de PC al conectarse --- 
IPAddress mqtt_server(192, 168, 4, 2); 
WiFiClient espClient;
PubSubClient mqttClient(espClient);

unsigned long ultimoEnvio = 0;

void setup() {
  Serial.begin(115200);
  
  // 1. Iniciar Sensores y Pantalla
  dht.begin();
  analogReadResolution(12); // Rango analógico 0-4095
  
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("ERROR: Pantalla OLED no detectada.");
    for (;;);
  }
  display.setTextColor(WHITE);
  display.clearDisplay();

  // 2. Crear red Wi-Fi
  Serial.println("\nCreando red Wi-Fi...");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, pass);
  Serial.print("Red creada. IP de la ESP32: ");
  Serial.println(WiFi.softAPIP());

  // 3. Configurar servidor MQTT
  mqttClient.setServer(mqtt_server, 1883);
}

void reconectarMQTT() {
  // Bucle hasta que logre conectarse a la PC
  while (!mqttClient.connected()) {
    Serial.print("Buscando PC y Mosquitto en 192.168.4.2... ");
    
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 20);
    display.print("Esperando a PC...");
    display.setCursor(0, 40);
    display.print("IP: 192.168.4.2");
    display.display();

    // Intentar conectar con un ID único
    if (mqttClient.connect("ESP32_Sensores")) {
      Serial.println("¡CONECTADO!");
      display.clearDisplay();
      display.setCursor(0, 20);
      display.print("PC Conectada!");
      display.display();
      delay(1000);
    } else {
      Serial.print("Fallo rc=");
      Serial.print(mqttClient.state());
      Serial.println(" -> Reintentando en 3s");
      delay(3000);
    }
  }
}

void loop() {
  // Mantener conexión MQTT activa
  if (!mqttClient.connected()) {reconectarMQTT();}
  mqttClient.loop();

  // Enviar datos cada 5000 milisegundos (5 segundos)
  if (millis() - ultimoEnvio >= 5000) {
    ultimoEnvio = millis();

    // 1. Leer Sensores
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    int uv = analogRead(UV_PIN);
    int mic = analogRead(MIC_PIN);

    // 2. Actualizar Pantalla OLED
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 0);
    if (isnan(t)) {
      display.print("DHT Err");
    } else {
      display.print((int)t); display.print("C "); 
      display.print((int)h); display.print("%");
    }
    
    display.setTextSize(1);
    display.setCursor(0, 30);
    display.printf("UV: %d", uv);
    display.setCursor(0, 50);
    display.printf("Ruido: %d", mic);
    display.display();

    // 3. Empaquetar y enviar por MQTT (formato JSON)
    if (!isnan(t) && !isnan(h)) {
      String payload = "{\"temperatura\":" + String(t) + 
                       ",\"humedad\":" + String(h) + 
                       ",\"uv\":" + String(uv) + 
                       ",\"ruido\":" + String(mic) + "}";
                       
      // Publicar en el tema "escuela/sensores/datos"
      mqttClient.publish("escuela/sensores/datos", payload.c_str());
      Serial.println("Enviado: " + payload);
    }
  }
}