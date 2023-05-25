#include <Arduino.h>

#include <WiFi.h>
#include <PubSubClient.h>

#include <MFRC522.h>
#include <SPI.h>

#include <LiquidCrystal_I2C.h>

#include <Adafruit_Fingerprint.h>

#define PINO_RST 15
#define PINO_SS 5
#define LED_VERD 4
#define LED_VERM 26
#define BUZZER 33
#define RELE 32

// Senha leitor biometrico
const uint32_t fingerprintpw = 0x0;

Adafruit_Fingerprint fingerprint = Adafruit_Fingerprint(&Serial2, fingerprintpw);

/* CONFIGURAÇÕES DO WIFI */
const char *ssid = "HUAWEI-BX15F8"; // Nome da rede WiFi
const char *password = "h7bq6tni";  // Senha da rede WiFi

/* CONFIGURAÇÕES DO MQTT*/
const char *mqttServer = "broker.hivemq.com"; // Endereço do Broker MQTT
const int mqttPort = 1883;                    // Porta TCP do Broker MQTT
const char *mqttUser = "esp";                 // Usuário MQTT
const char *mqttPassword = "";                // Senha MQTT

WiFiClient espClient;               // Cliente de Rede WiFi
PubSubClient clientMqtt(espClient); // Cria uma instancia de um cliente MQTT

MFRC522 mfrc522(PINO_SS, PINO_RST);

LiquidCrystal_I2C lcd(0x27, 16, 2);

void callback(char *topic, byte *payload, unsigned int length);
void initConnection();

void authorized();
void refused();

void lcdPrint(String msg);
void setupFingerprintSensor();
void storeFingerprint();
void checkFingerprint();
void menu();
void checkTag();

void setup()
{

  Serial.begin(115200);
  SPI.begin();
  mfrc522.PCD_Init();
  lcd.init();
  lcd.backlight();

  initConnection();
  setupFingerprintSensor();
  // storeFingerprint();

  pinMode(LED_VERM, OUTPUT);
  pinMode(LED_VERD, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(RELE, OUTPUT);
  digitalWrite(RELE, 1);

  lcdPrint("Identifique-se");
}

void loop()
{
  clientMqtt.loop();
  // menu();
  checkTag();
  checkFingerprint();
}

void lcdPrint(String msg){
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(msg);
}

void checkTag()
{
  if (!mfrc522.PICC_IsNewCardPresent())
  {
    return;
  }

  if (!mfrc522.PICC_ReadCardSerial()){
    return;
  }

  String conteudo = "";

  for (byte i = 0; i < mfrc522.uid.size; i++)
  {
    conteudo.concat(String(mfrc522.uid.uidByte[i] < HEX ? " 0" : " "));
    conteudo.concat(String(mfrc522.uid.uidByte[i], HEX));
  }

  Serial.println("----------");
  Serial.println(conteudo.substring(1));
  Serial.println("----------");

  if (conteudo.substring(1) == "43 5d e6 b6")
  {
    authorized();
  }
  else
  {
    refused();
  }
}

void authorized()
{
  digitalWrite(RELE, 0);
  lcdPrint("Acesso Liberado");
  digitalWrite(BUZZER, HIGH);
  delay(250);
  digitalWrite(BUZZER, LOW);
  digitalWrite(LED_VERD, HIGH);
  delay(2000);
  digitalWrite(LED_VERD, LOW);
  digitalWrite(RELE, 1);
  lcdPrint("Identifique-se");
}

void refused()
{
  lcdPrint("Acesso Negado");
  for (int i = 0; i < 3; i++)
  {
    digitalWrite(LED_VERM, HIGH);
    digitalWrite(BUZZER, HIGH);
    delay(500);

    digitalWrite(LED_VERM, LOW);
    digitalWrite(BUZZER, LOW);
    delay(500);
  }
  lcdPrint("Identifique-se");
}

void callback(char *topic, byte *payload, unsigned int length)
{

  Serial.print("Uma mensagem chegou no tópico: ");
  Serial.println(topic);

  Serial.print("Payload: ");
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }

  if (payload[0] == '0')
  {
    Serial.println("\nTRAVANDO ACESSO");
    digitalWrite(RELE, 1);
  }

  if (payload[0] == '1')
  {
    Serial.println("\nLIBERANDO ACESSO");
    digitalWrite(RELE, 0);
  }

  Serial.println();
  Serial.println("-----------------------");
}

void initConnection()
{
  WiFi.begin(ssid, password); // Configura o WiFi

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print("Tentando se conectar na rede: ");
    Serial.println(ssid);
  }

  Serial.println("Conectado na Rede WiFi.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  clientMqtt.setServer(mqttServer, mqttPort);
  clientMqtt.setCallback(callback);

  /* Verifica se o cliente está conectado ao Broker */
  while (!clientMqtt.connected())
  {

    Serial.println("Conectando ao Broker MQTT...");

    String clientId = "ESP32Client_" + String(random(0xffff), HEX);
    Serial.println("clientId = " + clientId);

    /* Conecta o cliente ao Broker MQTT.
       Configurações de credenciais e Last Will podem ser configuradas aqui*/
    if (clientMqtt.connect(clientId.c_str(), mqttUser, mqttPassword))
    {

      Serial.println("O cliente " + clientId + " foi conectado com sucesso");
    }
    else
    {

      // Estado do cliente MQTT. Quando a conexão falhar pode ser usado para obter informações sobre a falha
      int clientState = clientMqtt.state();

      Serial.print("Falha ao se conectar. ");
      Serial.println("Estado do cliente: " + (String)clientState);

      delay(2000);
    }
  }

  Serial.print("Tentando enviar a mensagem");

  clientMqtt.publish("dionisio/home/smartlock", "Hello from ESP32");
  clientMqtt.subscribe("dionisio/home/smartlock");
}

void setupFingerprintSensor()
{
  fingerprint.begin(57600);

  if (!fingerprint.verifyPassword())
  {
    Serial.println(F("Não foi possível conectar ao sensor."));
    while (true)
      ;
  }
}

String getCommand()
{
  while(!Serial.available()) delay(100);
  return Serial.readStringUntil('\n');
}

void storeFingerprint()
{
  Serial.println(F("Qual a posição para guardar a digital ? (1 a 149)"));

  int location = getCommand().toInt();

  if (location < 1 || location > 149)
  {
    Serial.println(F("Posição inválida"));
    return;
  }

  Serial.println(F("Encoste o dedo no sensor"));

  while (fingerprint.getImage() != FINGERPRINT_OK);

  if (fingerprint.image2Tz(1) != FINGERPRINT_OK)
  {
    Serial.println(F("Erro image2Tz 1"));
    return;
  }

  Serial.println(F("Tire o dedo do sensor"));

  delay(2000);

  while (fingerprint.getImage() != FINGERPRINT_NOFINGER);

  Serial.println(F("Encoste o mesmo dedo no sensor"));

  while (fingerprint.getImage() != FINGERPRINT_OK);

  if (fingerprint.image2Tz(2) != FINGERPRINT_OK)
  {
    Serial.println(F("Erro image2Tz 2"));
    return;
  }

  if (fingerprint.createModel() != FINGERPRINT_OK)
  {
    Serial.println(F("Erro createModel"));
    return;
  }

  if (fingerprint.storeModel(location) != FINGERPRINT_OK)
  {
    Serial.println(F("Erro storeModel"));
    return;
  }

  Serial.println(F("Sucesso!!!"));
}

void checkFingerprint()
{

  if (fingerprint.getImage() != FINGERPRINT_OK)
  {
    return;
  }

   Serial.println("PASSOU 1");

  if (fingerprint.image2Tz() != FINGERPRINT_OK)
  {
    Serial.println(F("Erro image2Tz"));
    return;
  }
  Serial.println("PASSOU 2");
  if (fingerprint.fingerFastSearch() == FINGERPRINT_OK)
  {
    Serial.print(F("Digital encontrada com confiança de "));
    Serial.print(fingerprint.confidence);
    Serial.print(F(" na posição "));
    Serial.println(fingerprint.fingerID);
    authorized();
    return;
  }

  Serial.println(F("Digital não encontrada"));
  refused();
}

void menu()
{
  if (Serial.available())
  {
    char option = Serial.read();

    if (option == '1')
    {
      storeFingerprint();
    }
    else if (option == '2')
    {
      // cadastrar nova tag
    }
  }
}
