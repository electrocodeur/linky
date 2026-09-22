#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#define wifi_ssid "ssid_de_votre_wifi"
#define wifi_password "mdp_de_votre_wifi"

#define mqtt_server "192.168.1.XX" //celui d'home assistant
#define mqtt_user "login_mqtt"  //s'il a été configuré sur Mosquitto
#define mqtt_password "mdp_mqtt" //idem

//byte mac[6];
const float VREF = 1.0;      // ADC ESP8266 : 0 à 1 V
const float RAPPORT = 5.28;  // Pont diviseur modifié
const float TENSION_SEUIL_DEMARRAGE = 3.5;
const float TENSION_BASSE = 3.4;
const unsigned long SOMMEIL_COURT_US = 15000000UL;   // 15 s
const unsigned long SOMMEIL_LONG_US = 180000000UL;    // 3 min
const unsigned long SOMMEIL_ECHEC_RESEAU_US = 30000000UL; //30 s
const uint8_t WIFI_MAX_TENTATIVES = 10;
const uint16_t WIFI_ATTENTE_MS = 500;
const uint8_t MQTT_MAX_TENTATIVES = 8;
const uint16_t MQTT_ATTENTE_MS = 750;
float tensionDemarrage = 0.0;

int ISOUSC;               // intensité souscrite  
int IINST;                // intensité instantanée    en A
int PAPP;                 // puissance apparente      en VA
unsigned long HCHC;       // compteur Heures Creuses  en W
unsigned long HCHP;       // compteur Heures Pleines  en W
//unsigned long BASE;       // index BASE               en W
String PTEC;              // période tarif en cours
String ADCO;              // adresse du compteur
String OPTARIF;           // option tarifaire
int ADPS;
boolean teleInfoReceived;

char chksum(char *buff, uint8_t len);
boolean handleBuffer(char *bufferTeleinfo, int sequenceNumnber);

//Buffer qui permet de décoder les messages MQTT reçus
char message_buff[100];

long lastMsg = 0;   //Horodatage du dernier message publié sur MQTT
long lastRecu = 0;
bool debug = false;  //Affiche sur la console si True

int nbtrywificon = 0;

int maxtrymqttcon = 8;
int nbtrymqttcon = 0;

#define SERIAL_TIMEOUT 3000  // 3 secondes timeout pour les lectures série
#define MQTT_KEEP_ALIVE 60   // keep-alive MQTT

String macToStr(const uint8_t* mac)
{
  String result;
  for (int i = 0; i < 6; ++i) {
    result += String(mac[i], 16);
    if (i < 5)
      result += ':';
  }
  return result;
}

String composeClientID() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  String clientId;
  clientId += "esp-";
  clientId += macToStr(mac);
  return clientId;
}

float measureVoltage() {
  uint16_t adc = analogRead(A0);
  float vadc = adc * VREF / 1023.0;
  return vadc * RAPPORT;
}

// ---------------------------------------------- //
//        Basic constructor for LoKyTIC           //
void TeleInfo() {
  // variables initializations
//  ADCO = "000000000000";
//  OPTARIF = "----";
//  ISOUSC = 0;
//  HCHC = 0L;
//  HCHP = 0L;
//  BASE = 0L;
//  PTEC = "----";
//  HHPHC = '-';
//  IINST = 0;
//  IMAX = 0;
  PAPP = 0;
//  MOTDETAT = "------";
}
// ---------------------------------------------- //

WiFiClient espClient;
PubSubClient client(espClient);

void connectWifi() {
  WiFi.persistent(false);
  WiFi.setAutoConnect(false);
  WiFi.setAutoReconnect(false);
  WiFi.setOutputPower(10);
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_password);

  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < WIFI_MAX_TENTATIVES) {
    delay(WIFI_ATTENTE_MS);
    timeout++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    ESP.deepSleep(SOMMEIL_ECHEC_RESEAU_US);
  }
}

void setup() {
  tensionDemarrage = measureVoltage();

  if (tensionDemarrage <= TENSION_SEUIL_DEMARRAGE) {
    long vinMillivolts = (long)(tensionDemarrage * 1000.0);
    long sommeilMillis = map(vinMillivolts, (long)(TENSION_BASSE * 1000.0), (long)(TENSION_SEUIL_DEMARRAGE * 1000.0), 180000L, 15000L);
    sommeilMillis = constrain(sommeilMillis, 15000L, 180000L);
    ESP.deepSleep((uint32_t)sommeilMillis * 1000UL);
  }

  client.setServer(mqtt_server, 1883);
  client.setKeepAlive(MQTT_KEEP_ALIVE);
}


//Reconnexion
void reconnect() {
  //Boucle jusqu'à obtenur une reconnexion
  while (!client.connected()) {
    String clientnumber = composeClientID();
    if (client.connect(clientnumber.c_str(), mqtt_user, mqtt_password)) {
      //do nothing
    } else {
      delay(MQTT_ATTENTE_MS);
      nbtrymqttcon = nbtrymqttcon +1;
      if (nbtrymqttcon >= MQTT_MAX_TENTATIVES) {
        ESP.deepSleep(SOMMEIL_ECHEC_RESEAU_US);
      }
    }
  }
}

// ---------------------------------------------- //
//   Update new values from TIC for the next TX   //
void updateParameters() {
  Serial.begin(1200, SERIAL_7E1);  // Historique: 1200 bauds, 7E1
  teleInfoReceived = readTeleInfo();
  //Serial.end(); // Important!!! -> STOP LoKyTIC to send packet.
}
// ---------------------------------------------- //

// ---------------------------------------------- //
//           TIC frame capture from Linky         //
boolean readTeleInfo()  {
//boolean readTeleInfo(bool TIC_state)  {
#define startFrame  0x02
#define endFrame    0x03
#define startLine   0x0A
#define endLine     0x0D
#define maxFrameLen 280

  int comptChar=0;  // variable de comptage des caractères reçus 
  char charIn=0;    // variable de mémorisation du caractère courant en réception
  char bufferTeleinfo[64] = "";
  int bufferLen = 0;
  int checkSum;
  int sequenceNumnber = 0;    // number of information group
  unsigned long startTime = millis();
  unsigned long lastLoopTime = millis();

  //--- wait for starting frame character with timeout
  while (charIn != startFrame)
  { // "Start Text" STX (002 h) is the beginning of the frame
    if (millis() - startTime > SERIAL_TIMEOUT) {
      return false;  // timeout
    }
    if (millis() - lastLoopTime > 100) {
      client.loop();  // keep MQTT connection alive
      lastLoopTime = millis();
    }
    if (Serial.available())
     charIn = Serial.read()& 0x7F;
  } // fin while (tant que) pas caractère 0x02
  
  //--- wait for the ending frame character 
  while (charIn != endFrame)
  { // tant que des octets sont disponibles en lecture : on lit les caractères
    if (millis() - startTime > SERIAL_TIMEOUT) {
      return false;  // timeout
    }
    if (millis() - lastLoopTime > 100) {
      client.loop();  // keep MQTT connection alive
      lastLoopTime = millis();
    }
    if (Serial.available()) {
      charIn = Serial.read()& 0x7F;
      // incrémente le compteur de caractère reçus
      comptChar++;
      if (charIn == startLine)  bufferLen = 0;
      if (bufferLen >= (int)sizeof(bufferTeleinfo) - 1) {
        return false; // overflow ligne
      }
      bufferTeleinfo[bufferLen] = charIn;
      // on utilise une limite max pour éviter String trop long en cas erreur réception
      // ajoute le caractère reçu au String pour les N premiers caractères
      if (charIn == endLine)  {
        checkSum = bufferTeleinfo[bufferLen -1];
        if (chksum(bufferTeleinfo, bufferLen) == checkSum)  {// we clear the 1st character
          strncpy(&bufferTeleinfo[0], &bufferTeleinfo[1], bufferLen -3);
          bufferTeleinfo[bufferLen -3] = 0x00;
          sequenceNumnber++;
          if (! handleBuffer(bufferTeleinfo, sequenceNumnber))  {
           // Serial.println(F("Sequence error ..."));
            return false;
          }
        }
        else  {
         // Serial.println(F("Checksum error!"));
          return false;
        }
      }
      else 
        bufferLen++;
    }
    if (comptChar > maxFrameLen)  {
     // Serial.println(F("Overflow error ..."));
      return false;
    }
  }
  return true;
}
// ---------------------------------------------- //

// ---------------------------------------------- //
//               TIC frame parsing                //
boolean handleBuffer(char *bufferTeleinfo, int sequenceNumnber) {
  (void)sequenceNumnber; // plus utilise

  char *label = strtok(bufferTeleinfo, " ");
  char *value = strtok(NULL, " ");
  if (!label || !value) return false;

  if (strcmp(label, "ADCO") == 0) ADCO = value;
  else if (strcmp(label, "OPTARIF") == 0) OPTARIF = value;
  else if (strcmp(label, "ISOUSC") == 0) ISOUSC = atoi(value);
  //else if (strcmp(label, "BASE") == 0) BASE = atol(value);
  else if (strcmp(label, "HCHC") == 0) HCHC = atol(value);
  else if (strcmp(label, "HCHP") == 0) HCHP = atol(value);
  else if (strcmp(label, "PTEC") == 0) PTEC = value;
  else if (strcmp(label, "IINST") == 0) IINST = atoi(value);
  else if (strcmp(label, "PAPP") == 0) PAPP = atoi(value);
  else if (strcmp(label, "ADPS") == 0) ADPS = atoi(value); // optionnel, depassement

  return true; // inconnus ignores sans erreur
}
// ---------------------------------------------- //
char chksum(char *buff, uint8_t len) {
  char sum = 0;
  for (uint8_t i = 1; i < (len - 2); i++) {
    sum += buff[i];
  }
  sum = (sum & 0x3F) + 0x20;
  return sum;
}
// ---------------------------------------------- //
void publishVoltage(const String &topic, float voltage) {
  String voltageText = String(voltage, 2);
  client.publish(topic.c_str(), voltageText.c_str(), true);
}

void loop() {
  TeleInfo();

  // Read TeleInfo data while Wi-Fi stays off
  updateParameters();

  if (PAPP <= 0) {
    client.disconnect();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    ESP.deepSleep(55e6);
  }

  connectWifi();

  // Reconnect to MQTT only after the Linky values are known
  if (!client.connected()) {
    reconnect();
  }

  // Process MQTT keep-alive
  client.loop();

  float tensionFin = measureVoltage();

  String adressmacesp = composeClientID();
  String topic_tension_debut = "winky/" + adressmacesp + "/tension_debut";
  String topic_tension_fin = "winky/" + adressmacesp + "/tension_fin";
  publishVoltage(topic_tension_debut, tensionDemarrage);
  publishVoltage(topic_tension_fin, tensionFin);

  if (PAPP > 0) {
    // Publish data to MQTT
    String topic_power = "winky/" + adressmacesp + "/power";
    //String topic_base = "winky/" + adressmacesp + "/base";
    String topic_hchc = "winky/" + adressmacesp + "/hchc";
    String topic_hchp = "winky/" + adressmacesp + "/hchp";

    String topic_adco = "winky/" + adressmacesp + "/adco";
    String topic_optarif = "winky/" + adressmacesp + "/optarif";
    String topic_isousc = "winky/" + adressmacesp + "/isousc";
    String topic_ptec = "winky/" + adressmacesp + "/ptec";
    String topic_iinst = "winky/" + adressmacesp + "/iinst";
    String topic_adps = "winky/" + adressmacesp + "/adps";

    client.publish(topic_power.c_str(), String(PAPP).c_str(), true);
    //client.publish(topic_base.c_str(), String(BASE).c_str(), true);
    client.publish(topic_hchc.c_str(), String(HCHC).c_str(), true);
    client.publish(topic_hchp.c_str(), String(HCHP).c_str(), true);

    client.publish(topic_adco.c_str(), ADCO.c_str(), true);
    client.publish(topic_optarif.c_str(), OPTARIF.c_str(), true);
    client.publish(topic_isousc.c_str(), String(ISOUSC).c_str(), true);
    client.publish(topic_ptec.c_str(), PTEC.c_str(), true);
    client.publish(topic_iinst.c_str(), String(IINST).c_str(), true);
    client.publish(topic_adps.c_str(), String(ADPS).c_str(), true);
  }

  client.disconnect();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(100); // Ensure all data is sent before sleeping
  ESP.deepSleep(55e6);
}