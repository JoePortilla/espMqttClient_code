/**
 * \file espMqttClient_sub main.cpp
 * \brief Suscribirse a un topico que controla un led, mediante la biblioteca espMqttClient.
 * \author Joseph Santiago Portilla. Ing. Electrónico - @JoePortilla
 */

// BIBLIOTECAS
#include <Arduino.h>
#include <WiFi.h>
#include <espMqttClient.h>
#include "secrets.hpp" // Credenciales de servicio y contraseñas de usuario.

// AJUSTES MQTT
// Creación de un objeto de la clase AsyncMqttClient
espMqttClient mqttClient;
// ID de microcontrolador en el broker
const char *MQTT_CLIENTID = "ESP32testing1";
// Tópicos
const char *TOPIC_STATUS = "ESP/status";
const char *TOPIC_CONTROL = "ESP/control";

// VARIABLES Y CONSTANTES
const uint8_t ledPin = 2;
uint32_t tActual = 0;                          // Almacena el tiempo actual de ejecución.
uint32_t tPrevioMqttReconnect = 0;             // Almacena el ultimo tiempo en el que se trato de conectar al broker.
const uint16_t intervaloMqttReconnect = 10000; // Cada cuanto tiempo se intenta reconectar al broker tras desconexión [ms].

// BANDERAS
bool mqttReconnect = false; // Bandera de reconexión al broker MQTT

// DECLARACIÓN DE FUNCIONES
// Funciones para AsyncMqttClient
void connectToWiFi();
void WiFiStatus(WiFiEvent_t event);
// Funciones para AsyncMqttClient
void connectToMqtt();
void onMqttConnect(bool sessionPresent);
void onMqttDisconnect(espMqttClientTypes::DisconnectReason reason);
void onMqttSubscribe(uint16_t packetId, const espMqttClientTypes::SubscribeReturncode *codes, size_t len);
void onMqttUnsubscribe(uint16_t packetId);
void onMqttPublish(uint16_t packetId);
void onMqttMessage(const espMqttClientTypes::MessageProperties &properties, const char *topic,
                   const uint8_t *payload, size_t len, size_t index, size_t total);

void setup()
{
  // Inicializar la comunicación serial a 115200 baudios.
  Serial.begin(115200);

  // Definir el GPIO del LED como salida.
  pinMode(ledPin, OUTPUT);
  // Inicializar el LED apagado.
  digitalWrite(ledPin, 0);

  // Deshabilitar conexión WiFi automatica del módulo al encenderse
  WiFi.setAutoConnect(false);
  // Habilitar reconexión automática de WiFi, si la conexión se pierde o interrumpe en algun momento.
  WiFi.setAutoReconnect(true);
  // Callback de eventos WiFi
  WiFi.onEvent(WiFiStatus);

  // Ajustes de broker MQTT
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  // mqttClient.setCredentials(MQTT_USER, MQTT_PASSWORD);
  mqttClient.setClientId(MQTT_CLIENTID);

  // Funciones de callback para AsyncMqttClient
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onSubscribe(onMqttSubscribe);
  mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.onPublish(onMqttPublish);

  // Conexión a WiFi y Broker MQTT
  connectToWiFi();
}

void loop()
{
  // Almacenar tiempo actual de ejecución.
  tActual = millis();

  if (mqttReconnect && (tActual - tPrevioMqttReconnect > intervaloMqttReconnect))
  {
    connectToMqtt();
  }
}

// IMPLEMENTACIÓN DE FUNCIONES

// Implementación de funciones para WiFi

// Función para conectar el microcontrolador a WiFi
void connectToWiFi()
{
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("\nConectandose a la red WiFi: %s\n", WIFI_SSID);
}

// Función de callback. Cada vez que haya un evento relacionado al WiFi se llama y ejecuta
// su operación segun el caso.
void WiFiStatus(WiFiEvent_t event)
{
  switch (event)
  {
  case SYSTEM_EVENT_STA_GOT_IP:
    // Imprimir la IP y la fuerza de la señal WiFi.
    Serial.print("WiFi conectado. IP:");
    Serial.print(WiFi.localIP());
    Serial.printf(" RSSI: %d\n", WiFi.RSSI());
    // Si el microcontrolador se conectó a WiFi, intentar conexión a Broker.
    connectToMqtt();
    break;

  case SYSTEM_EVENT_STA_DISCONNECTED:
    Serial.println("WiFi Desconectado");
    break;

  default:
    break;
  }
}

// Implementación de funciones para AsyncMqttClient

// Función para conectar el microcontrolador al broker
void connectToMqtt()
{
  Serial.println("Iniciando conexión MQTT.");

  if (!mqttClient.connect())
  {
    Serial.println("Conexión a MQTT fallada.");
    mqttReconnect = true;
    tPrevioMqttReconnect = millis();
  }
  else
  {
    mqttReconnect = false;
  }
}

/**
 * Función callback de conexión a MQTT
 * Cada vez que el microcontrolador se conecte al broker se llama a esta función,
 * aqui se envia un mensaje de bienvenida y se suscribe a los topicos definidos por el usuario.
 */
void onMqttConnect(bool sessionPresent)
{
  // sessionPresent es una variable booleana que indica si el microcontrolador ha
  // encontrado una sesión previa con el broker al que se está conectando
  Serial.printf("%s conectado a MQTT. (Estado sesión previa=%d)\n", MQTT_CLIENTID, sessionPresent);

  // Se envia un mensaje de bienvenida para confirmar la conexión del ESP32 al broker.
  // El mensaje contiene "Nombre definido para identificar el microcontrolador + conectado".
  char WELCOME_MSG[50];
  strcpy(WELCOME_MSG, MQTT_CLIENTID);
  strcat(WELCOME_MSG, " conectado");
  const uint8_t QOS_STATUS = 1;
  // Publicar WELCOME_MSG en el topic TOPIC_STATUS
  mqttClient.publish(TOPIC_STATUS, QOS_STATUS, false, WELCOME_MSG);

  // Suscripción a tópicos
  mqttClient.subscribe(TOPIC_CONTROL, 1);
}

/**
 * Función callback de desconexión a MQTT
 * Cada vez que el microcontrolador se desconecte del broker se llama a esta función. Se brindan
 * los codigos de la razon de desconexión que se enumeran en AsyncMqttClientDisconnectReason.
 */
void onMqttDisconnect(espMqttClientTypes::DisconnectReason reason)
{
  Serial.printf("MQTT Desconectado [%u].\n", static_cast<uint8_t>(reason));

  if (WiFi.isConnected())
  {
    mqttReconnect = true;
    tPrevioMqttReconnect = millis();
  }
}

/**
 * Función callback de suscripción a un tópico.
 * Cada vez que el microcontrolador se suscriba a un topico se llama a esta función. Si el id del
 * paquete es 0 indica que hubo un error en la suscripción.
 */
void onMqttSubscribe(uint16_t packetId, const espMqttClientTypes::SubscribeReturncode *codes, size_t len)
{
  if (packetId != 0)
  {
    Serial.printf("Suscripción correcta. ");
    for (size_t i = 0; i < len; ++i)
    {
      Serial.printf("QoS: ");
      Serial.println(static_cast<uint8_t>(codes[i]));
    }
  }

  if (packetId == 0)
    Serial.printf("Error en la suscripción.");
}

/**
 * Función callback de desuscripción a un tópico.
 * Cada vez que el microcontrolador se desuscriba a un topico se llama a esta función. Si el id del
 * paquete es 0 indica que hubo un error en la desuscripción.
 */
void onMqttUnsubscribe(uint16_t packetId)
{
  if (packetId != 0)
    Serial.printf("Suscripción cancelada.\n");
  if (packetId == 0)
    Serial.printf("Error en la cancelación de suscripción\n");
}

/**
 * Función callback de publicación a un tópico.
 * Cada vez que el microcontrolador publique hacia un topico se llama a esta función. Si el id del
 * paquete es 0 indica que hubo un error en la publicación.
 */
void onMqttPublish(uint16_t packetId)
{
  if (packetId != 0)
    Serial.printf("Publicación correcta.\n");
  if (packetId == 0)
    Serial.printf("Error en la publicación.\n");
}

/**
 * Función callback de recepción de mensaje de un tópico.
 * Cada vez que el microcontrolador reciba un mensaje de un topico al que se suscribió al conectarse
 * al broker se llama a esta función.
 */
void onMqttMessage(const espMqttClientTypes::MessageProperties &properties, const char *topic,
                   const uint8_t *payload, size_t len, size_t index, size_t total)
{
  // (void)payload;
  // Se crea un string vacio para recibir los mensajes
  String msg = "";

  // Se formatea correctamente la información, concatenando los caracteres del payload entrante.
  for (size_t i = 0; i < len; ++i)
    msg += (char)payload[i];

  // Se eliminan espacios innecesarios al inicio o al final del mensaje
  msg.trim();

  // Se presenta el mensaje recibido, su topico y qos correspondiente.
  Serial.printf("Mensaje recibido [%s] (QoS:%d): %s.\n", topic, properties.qos, msg.c_str());

  // Operaciones a seguir en el programa segun el mensaje recibido.
  if (String(topic) == TOPIC_CONTROL)
  {
    if (msg == "0")
    {
      digitalWrite(ledPin, 0);
      Serial.printf("LED OFF\n");
    }

    if (msg == "1")
    {
      digitalWrite(ledPin, 1);
      Serial.printf("LED ON\n");
    }
  }
}