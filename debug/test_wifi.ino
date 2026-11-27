#include <ESP8266WiFi.h>

// Identifiants Wi-Fi
const char* ssid = "NOM_DU_WIFI";
const char* password = "MOT_DE_PASSE";

void setup() {
  Serial.begin(115200);

  // LED éteinte au démarrage
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  // Connexion Wi-Fi
  WiFi.begin(ssid, password);

  Serial.print("Connexion au Wi-Fi");

  unsigned long debut = millis();

  // Attente de 15 secondes maximum
  while (WiFi.status() != WL_CONNECTED &&
         millis() - debut < 15000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnexion réussie !");
    Serial.print("Adresse IP : ");
    Serial.println(WiFi.localIP());

    // Allumer la LED
    digitalWrite(LED_BUILTIN, LOW);
  } else {
    Serial.println("\nÉchec de connexion Wi-Fi !");

    // Garder la LED éteinte
    digitalWrite(LED_BUILTIN, HIGH);
  }
}

void loop() {
  // Rien à faire
}
