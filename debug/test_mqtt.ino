#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// ---------------- WIFI ----------------
#define wifi_ssid "ssid_de_votre_wifi"
#define wifi_password "mdp_de_votre_wifi"

// ---------------- MQTT ----------------
#define mqtt_server "192.168.1.XX"
#define mqtt_user "login_mqtt"
#define mqtt_password "mdp_mqtt"

#define MQTT_PORT 1883

// ---------------- CONFIGURATION ----------------
const char* mqtt_topic = "winky/linky/power";

const uint8_t WIFI_MAX_TENTATIVES = 15;
const uint16_t WIFI_ATTENTE_MS = 1000;

const uint8_t MQTT_MAX_TENTATIVES = 15;
const uint16_t MQTT_ATTENTE_MS = 1000;

const uint32_t SOMMEIL_ECHEC_US = 30000000UL; // 30 secondes
const uint32_t SOMMEIL_NORMAL_US = 55000000UL; // 55 secondes

// LED intégrée
#define LED_PIN LED_BUILTIN

WiFiClient espClient;
PubSubClient client(espClient);


// ---------------- DEEP SLEEP ----------------
void goToSleep(uint32_t duration) {

  // LED éteinte
  digitalWrite(LED_PIN, HIGH);

  // Déconnexion MQTT
  if (client.connected()) {
    client.disconnect();
  }

  // Arrêt du Wi-Fi
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  delay(100);

  // Mise en sommeil
  ESP.deepSleep(duration);
}


// ---------------- CONNEXION WIFI ----------------
bool connectWifi() {

  WiFi.persistent(false);
  WiFi.setAutoConnect(false);
  WiFi.setAutoReconnect(false);
  WiFi.setOutputPower(10);
  WiFi.mode(WIFI_STA);

  WiFi.begin(wifi_ssid, wifi_password);

  uint8_t timeout = 0;

  while (WiFi.status() != WL_CONNECTED &&
         timeout < WIFI_MAX_TENTATIVES) {

    delay(WIFI_ATTENTE_MS);
    timeout++;
  }

  return WiFi.status() == WL_CONNECTED;
}


// ---------------- CONNEXION MQTT ----------------
bool connectMQTT() {

  client.setServer(mqtt_server, MQTT_PORT);

  // Identifiant unique basé sur le chip ID
  String clientID = "Wemos-";
  clientID += String(ESP.getChipId(), HEX);

  uint8_t tentatives = 0;

  while (!client.connected() &&
         tentatives < MQTT_MAX_TENTATIVES) {

    if (client.connect(
          clientID.c_str(),
          mqtt_user,
          mqtt_password)) {

      return true;
    }

    tentatives++;
    delay(MQTT_ATTENTE_MS);
  }

  return client.connected();
}


// ---------------- GENERATION PUISSANCE ----------------
int generatePower() {

  // Initialisation du générateur aléatoire
  randomSeed(ESP.getCycleCount() ^ micros() ^ ESP.getChipId());

  // Puissance aléatoire entre 100 et 3000 VA
  return random(100, 3001);
}


// ---------------- SETUP ----------------
void setup() {

  // LED éteinte au démarrage
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  // Génération de la puissance apparente
  int PAPP = generatePower();

  // Connexion Wi-Fi
  if (!connectWifi()) {
    goToSleep(SOMMEIL_ECHEC_US);
    return;
  }

  // Connexion MQTT
  if (!connectMQTT()) {
    goToSleep(SOMMEIL_ECHEC_US);
    return;
  }

  // Conversion de la puissance en texte
  char payload[12];
  snprintf(payload, sizeof(payload), "%d", PAPP);

  // Publication MQTT avec conservation de la dernière valeur
  bool publicationOK = client.publish(
    mqtt_topic,
    payload,
    true
  );

  // Si publication réussie
  if (publicationOK) {

    // LED allumée pendant 3 secondes
    digitalWrite(LED_PIN, LOW);
    delay(3000);

    digitalWrite(LED_PIN, HIGH);
  }

  // Déconnexion et Deep Sleep
  goToSleep(SOMMEIL_NORMAL_US);
}


// ---------------- LOOP ----------------
void loop() {
  // Inutilisé : tout est exécuté dans setup()
}
