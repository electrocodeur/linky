#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#define wifi_ssid "ssid_de_votre_wifi"
#define wifi_password "mdp_de_votre_wifi"

#define mqtt_server "192.168.1.XX" //celui d'home assistant
#define mqtt_user "login_mqtt"  //s'il a été configuré sur Mosquitto
#define mqtt_password "mdp_mqtt" //idem

const float VREF = 1.0;
const float RAPPORT = 5.28;
const float TENSION_SEUIL_DEMARRAGE = 3.5;//3.5
const float TENSION_BASSE = 3.4;//3.4
const unsigned long SOMMEIL_ECHEC_RESEAU_US = 30000000UL;
const uint8_t WIFI_MAX_TENTATIVES = 10;//10
const uint16_t WIFI_ATTENTE_MS = 500;
const uint8_t MQTT_MAX_TENTATIVES = 8;//8
const uint16_t MQTT_ATTENTE_MS = 750;
float tensionDemarrage = 0.0;

// ---- Valeurs TIC (-1 = non reçu, donc non publié) ----
int  PAPP = 0;                 // SINSTS  puissance apparente soutirée (VA)
int  IINST = -1;               // IRMS1   intensité (A)
int  ISOUSC = -1;              // PREF    converti en A
int  ADPS = -1;                // calculé
long HCHC = -1;                // EASF01
long HCHP = -1;                // EASF02
long EAST = -1;                // index total soutiré (Wh)
int  URMS1 = -1;               // tension efficace (V)
int  UMOY1 = -1;               // tension moyenne (V)
int  SMAXSN = -1;              // puissance max jour (VA)
int  SMAXSN1 = -1;             // puissance max veille (VA)
int  CCASN = -1;               // puissance active moyenne 10 min (W)
int  PCOUP = -1;               // puissance de coupure (kVA)
int  NTARF = -1;               // index tarifaire en cours
String DATE_LINKY;             // horodate compteur
String PTEC;                   // LTARF
String ADCO;                   // ADSC
String OPTARIF;                // NGTF
boolean teleInfoReceived;

char chksum(char *buff, uint8_t len);
boolean handleBuffer(char *buf, int sequenceNumnber);

int nbtrymqttcon = 0;

#define SERIAL_TIMEOUT 5000
#define MQTT_KEEP_ALIVE 60

String macToStr(const uint8_t* mac)
{
  String result;
  for (int i = 0; i < 6; ++i) {
    result += String(mac[i], 16);
    if (i < 5) result += ':';
  }
  return result;
}

String composeClientID() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  String clientId = "esp-";
  clientId += macToStr(mac);
  return clientId;
}

float measureVoltage() {
  uint16_t adc = analogRead(A0);
  float vadc = adc * VREF / 1023.0;
  return vadc * RAPPORT;
}

void TeleInfo() {
  PAPP = 0;
}

WiFiClient espClient;
PubSubClient client(espClient);

void connectWifi() {
  WiFi.persistent(false);
  WiFi.setAutoConnect(false);
  WiFi.setAutoReconnect(false);
  WiFi.setOutputPower(10);//10
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

void reconnect() {
  while (!client.connected()) {
    String clientnumber = composeClientID();
    if (!client.connect(clientnumber.c_str(), mqtt_user, mqtt_password)) {
      delay(MQTT_ATTENTE_MS);
      nbtrymqttcon++;
      if (nbtrymqttcon >= MQTT_MAX_TENTATIVES) {
        ESP.deepSleep(SOMMEIL_ECHEC_RESEAU_US);
      }
    }
  }
}

void updateParameters() {
  Serial.begin(9600, SERIAL_7E1);  // MODE STANDARD
  teleInfoReceived = readTeleInfo();
}

// ---------------------------------------------- //
//           Capture de la trame TIC standard     //
boolean readTeleInfo() {
#define startFrame  0x02
#define endFrame    0x03
#define startLine   0x0A
#define endLine     0x0D
#define maxFrameLen 2500

  int comptChar = 0;
  char charIn = 0;
  char bufferTeleinfo[128] = "";
  int bufferLen = 0;
  bool skipLine = false;
  int checkSum;
  int sequenceNumnber = 0;
  unsigned long startTime = millis();
  unsigned long lastLoopTime = millis();

  while (charIn != startFrame) {
    if (millis() - startTime > SERIAL_TIMEOUT) return false;
    if (millis() - lastLoopTime > 100) {
      client.loop();
      lastLoopTime = millis();
    }
    if (Serial.available()) charIn = Serial.read() & 0x7F;
  }

  while (charIn != endFrame) {
    if (millis() - startTime > SERIAL_TIMEOUT) return false;
    if (millis() - lastLoopTime > 100) {
      client.loop();
      lastLoopTime = millis();
    }
    if (Serial.available()) {
      charIn = Serial.read() & 0x7F;
      comptChar++;

      if (charIn == startLine) {
        bufferLen = 0;
        skipLine = false;
      } else if (skipLine) {
        continue;
      }

      if (bufferLen >= (int)sizeof(bufferTeleinfo) - 1) {
        skipLine = true;
        bufferLen = 0;
        continue;
      }
      bufferTeleinfo[bufferLen] = charIn;

      if (charIn == endLine) {
        checkSum = bufferTeleinfo[bufferLen - 1];
        if (chksum(bufferTeleinfo, bufferLen) == checkSum) {
          memmove(&bufferTeleinfo[0], &bufferTeleinfo[1], bufferLen - 3);
          bufferTeleinfo[bufferLen - 3] = 0x00;
          sequenceNumnber++;
          handleBuffer(bufferTeleinfo, sequenceNumnber);
        } else {
          return false;
        }
      } else {
        bufferLen++;
      }
    }
    if (comptChar > maxFrameLen) return false;
  }
  return true;
}

// ---------------------------------------------- //
//        Analyse d'une ligne (mode standard)     //
boolean handleBuffer(char *buf, int sequenceNumnber) {
  (void)sequenceNumnber;

  char *fields[3] = {NULL, NULL, NULL};
  int n = 0;
  char *p = buf;
  fields[n++] = p;
  while (*p && n < 3) {
    if (*p == '\t') {
      *p = 0;
      fields[n++] = p + 1;
    }
    p++;
  }
  if (n < 2) return true;

  const char *label = fields[0];
  char *v = fields[1];                          // valeur (lignes sans horodate)
  char *vh = (n == 3) ? fields[2] : fields[1];  // valeur (lignes horodatées)

  // --- Identification ---
  if (strcmp(label, "ADSC") == 0) ADCO = v;
  else if (strcmp(label, "NGTF") == 0) { OPTARIF = v; OPTARIF.trim(); }
  else if (strcmp(label, "LTARF") == 0) { PTEC = v; PTEC.trim(); }
  else if (strcmp(label, "DATE") == 0) DATE_LINKY = v;
  else if (strcmp(label, "NTARF") == 0) NTARF = atoi(v);
  else if (strcmp(label, "PREF") == 0) ISOUSC = atoi(v) * 5;  // kVA -> A
  else if (strcmp(label, "PCOUP") == 0) PCOUP = atoi(v);

  // --- Index d'énergie ---
  else if (strcmp(label, "EAST") == 0) EAST = atol(v);
  else if (strcmp(label, "EASF01") == 0) HCHC = atol(v);
  else if (strcmp(label, "EASF02") == 0) HCHP = atol(v);

  // --- Mesures instantanées ---
  else if (strcmp(label, "IRMS1") == 0) IINST = atoi(v);
  else if (strcmp(label, "URMS1") == 0) URMS1 = atoi(v);
  else if (strcmp(label, "SINSTS") == 0) PAPP = atoi(v);

  // --- Valeurs horodatées (valeur dans le 3e champ) ---
  else if (strcmp(label, "UMOY1") == 0 && n == 3) UMOY1 = atoi(vh);
  else if (strcmp(label, "SMAXSN") == 0 && n == 3) SMAXSN = atoi(vh);
  else if (strcmp(label, "SMAXSN-1") == 0 && n == 3) SMAXSN1 = atoi(vh);
  else if (strcmp(label, "CCASN") == 0 && n == 3) CCASN = atoi(vh);

  // Dépassement : déduit
  if (ISOUSC > 0 && IINST >= 0) ADPS = (IINST > ISOUSC) ? IINST : 0;

  return true;
}

// Checksum MODE STANDARD
char chksum(char *buff, uint8_t len) {
  int sum = 0;
  for (uint8_t i = 1; i < (len - 1); i++) {
    sum += buff[i];
  }
  sum = (sum & 0x3F) + 0x20;
  return (char)sum;
}

// ---------------------------------------------- //
//                  Publication MQTT              //
String topicPrefix;

void pubLong(const char *name, long value) {
  if (value < 0) return;   // non reçu
  String t = topicPrefix + name;
  client.publish(t.c_str(), String(value).c_str(), true);
}

void pubStr(const char *name, const String &value) {
  if (value.length() == 0) return;
  String t = topicPrefix + name;
  client.publish(t.c_str(), value.c_str(), true);
}

void publishVoltage(const char *name, float voltage) {
  String t = topicPrefix + name;
  client.publish(t.c_str(), String(voltage, 2).c_str(), true);
}

void loop() {
  TeleInfo();
  updateParameters();

  if (PAPP <= 0) {
    client.disconnect();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    ESP.deepSleep(25e6);
  }

  connectWifi();

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  float tensionFin = measureVoltage();
  topicPrefix = "winky/linky/";

  // Tension batterie / alimentation de l'ESP
  publishVoltage("tension_debut", tensionDemarrage);
  publishVoltage("tension_fin", tensionFin);

  // Topics historiques (inchangés)
  pubLong("power", PAPP);
  pubLong("hchc", HCHC);
  pubLong("hchp", HCHP);
  pubStr("adco", ADCO);
  pubStr("optarif", OPTARIF);
  pubLong("isousc", ISOUSC);
  pubStr("ptec", PTEC);
  pubLong("iinst", IINST);
  pubLong("adps", ADPS);

  // Topics supplémentaires
  pubLong("urms1", URMS1);
  pubLong("umoy1", UMOY1);
  pubLong("east", EAST);
  pubLong("smaxsn", SMAXSN);
  pubLong("smaxsn_1", SMAXSN1);
  pubLong("ccasn", CCASN);
  pubLong("pcoup", PCOUP);
  pubLong("ntarf", NTARF);
  pubStr("date", DATE_LINKY);

  client.loop();
  client.disconnect();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(100);
  ESP.deepSleep(55e6);
}
