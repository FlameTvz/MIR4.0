#include <Arduino.h>
#include <FS.h>
#include <Ethernet.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <FirebaseESP32.h>
#include <SPIFFS.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "vars.h"
#include "func.h"

//ijbihvih
// Variáveis para Firebase
FirebaseData firebaseData;
FirebaseAuth firebaseAuth;
FirebaseConfig firebaseConfig;



// Variáveis de conexão
WiFiManager wm;
bool reseta = 0;
bool count = 0;
bool ethernetConnected = false;
bool wifiConnected = false;
long tReset = 3000;
long timer,tAtual;

// Configuração do endereço MAC do Ethernet
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};

// Inicialização do servidor WiFi (quando Ethernet está conectado)
WiFiServer wifiServer(80);




void setup()
{
  Serial.begin(115200);
  pinMode(25, OUTPUT);
  pinMode(BTN_CONFIG, INPUT);
  digitalWrite(25, HIGH);

  for(int i = 0; i < qtdRele; i++){
  pinMode(rele[i].pino, OUTPUT);
  pinMode(rele[i].btn, INPUT);
  }

  // Tentar conectar ao Ethernet primeiro com DHCP
  if (Ethernet.begin(mac) == 0)
  { // Tenta obter IP via DHCP
    Serial.println("Falha ao conectar ao Ethernet. Tentando conectar via WiFi...");
    ethernetConnected = false;
  }
  else
  {
    Serial.print("Conectado ao Ethernet com IP: ");
    Serial.println(Ethernet.localIP());
    ethernetConnected = true;
  }

  // Se Ethernet falhar, tentar conectar via WiFiManager
  if (!ethernetConnected)
  {
    res = wm.autoConnect("AutoConnectAP", "password"); // Nome e senha do ponto de acesso
    if (res)
    {
      Serial.println("Conectado ao WiFi via WiFiManager.");
      wifiConnected = true;
    }
    else
    {
      Serial.println("Falha ao conectar via WiFiManager.");
      wifiConnected = false;
    }
  }

// if (!SPIFFS.begin()) {
//     Serial.println("Falha ao montar o sistema de arquivos!");
//     return;
//   }

//   Serial.println("Sistema de arquivos montado com sucesso!");

 

  // Configuração do Firebase
  firebaseConfig.api_key = API_KEY;
  firebaseConfig.database_url = DATABASE_URL;
  firebaseAuth.user.email = EMAIL;
  firebaseAuth.user.password = EMAIL_PASSWORD;
  Firebase.begin(&firebaseConfig, &firebaseAuth);
  Firebase.reconnectWiFi(true);

  // Iniciar o stream do Firebase
  if (!Firebase.beginStream(firebaseData, "/IdsESP/9999"))
  {
    Serial.println("Não foi possível iniciar o stream do Firebase");
    Serial.println(firebaseData.errorReason());
  }
  else
  {
    Serial.println("Stream do Firebase iniciado com sucesso.");
  }

  // Iniciar o servidor WiFi se estiver conectado via Ethernet
  if (ethernetConnected)
  {
    WiFi.softAP("ESP32_AP", "12345678"); // Configura um ponto de acesso WiFi no ESP32
    wifiServer.begin();
    Serial.println("Ponto de acesso WiFi criado com sucesso para Ethernet.");
  }
  DisplayInit();
}

void loop()
{
  entradaNaSaida();
  delay(1);

  if (!Firebase.readStream(firebaseData))
  {
    Serial.println("Erro no stream /IdsESP/9999, aguardando próximo ciclo...");
    Firebase.endStream(firebaseData);
    if (Firebase.beginStream(firebaseData, "/IdsESP/9999"))
    {
      Serial.println("Contador de loop: ");
      Serial.println("Firebase stream reconectado com sucesso para /IdsESP/9999.");
    }
    else
    {
      Serial.println("Contador de loop: ");
      Serial.println("Não foi possível iniciar o stream do Firebase para /IdsESP/9999");
      Serial.println(firebaseData.errorReason());
    }
  }

  if (firebaseData.streamAvailable())
  {
    Serial.println("Dados atualizados recebidos para /IdsESP/9999:");
    String jsonData = firebaseData.stringData();
    Serial.println(jsonData);

    if (jsonData.indexOf("\"status\":true") != -1 && firebaseData.dataPath() == "/rele1")
    {
      Serial.println("Ativando pulso no rele1");
      digitalWrite(rele[0].pino, HIGH);
      delay(200);
      digitalWrite(rele[0].pino, LOW);
      rele[0].status = LOW;

      // } else if (jsonData.indexOf("\tipoBotao\":switch") != -1 && firebaseData.dataPath() == "/rele1") {
      //   Serial.println("ativando modo switch");
      //   digitalWrite(rele[0].pino, HIGH);
      //   rele[0].status = HIGH;
    }

    else if (jsonData.indexOf("\"status\":true") != -1 && firebaseData.dataPath() == "/rele2")
    {
      Serial.println("Ativando pulso no rele2");
      digitalWrite(rele[1].pino, HIGH);
      delay(200);
      digitalWrite(rele[1].pino, LOW);
      rele[1].status = LOW;
    }
    // else if (jsonData.indexOf("\tipoBotao\":switch") != -1 && firebaseData.dataPath() == "/rele2") {
    //   Serial.println("ativando modo switch");
    //   digitalWrite(rele[1].pino, HIGH);
    //   rele[1].status = HIGH;
    // }
    else if (jsonData.indexOf("\"status\":true") != -1 && firebaseData.dataPath() == "/rele3")
    {
      Serial.println("Ativando pulso no rele3");
      digitalWrite(rele[2].pino, HIGH);
      delay(200);
      digitalWrite(rele[2].pino, LOW);
      rele[2].status = LOW;
      // } else if (jsonData.indexOf("\tipoBotao\":switch") != -1 && firebaseData.dataPath() == "/rele3") {
      //   Serial.println("ativando modo switch");
      //   digitalWrite(rele[2].pino, HIGH);
      //   rele[2].status = HIGH;
    }
    else if (jsonData.indexOf("\"status\":true") != -1 && firebaseData.dataPath() == "/rele4")
    {
      Serial.println("Ativando pulso no rele4");
      digitalWrite(rele[3].pino, HIGH);
      delay(200);
      digitalWrite(rele[3].pino, LOW);
      rele[3].status = LOW;
    }
    // else if (jsonData.indexOf("\tipoBotao\":switch") != -1 && firebaseData.dataPath() == "/rele4") {
    //   Serial.println("ativando modo switch");
    //   digitalWrite(rele[3].pino, HIGH);
    //   rele[3].status = HIGH;
    // }
        else if (jsonData.indexOf("\"status\":true") != -1 && firebaseData.dataPath() == "/rele5")
    {
      Serial.println("Ativando pulso no rele5");
      digitalWrite(rele[4].pino, HIGH);
      delay(200);
      digitalWrite(rele[4].pino, LOW);
      rele[4].status = LOW;
    }
    else
    {
      Serial.println("Nenhuma correspondência de atualização de status para os relés.");
    }
  }
  // Manter o ponto de acesso para dispositivos conectados ao WiFi quando Ethernet está ativo
  // if (ethernetConnected)
  // {
  //   WiFiClient client = wifiServer.available();
  //   if (client)
  //   {
  //     Serial.println("Cliente conectado ao ponto de acesso WiFi via Ethernet.");
  //     client.println("Conectado ao ESP32 via Ethernet como ponto de acesso WiFi!");
  //     client.stop();
  //   }
  // }
  timer = millis();
  
  if(digitalRead(BTN_CONFIG) == HIGH){
    Serial.println(timer - tAtual);
    if(count == 0){
      Serial.print("apertou BT");
      tAtual=millis();
      count = 1;
    }
    else if((timer - tAtual) > tReset){
          // wm.erase();
          // delay(1000);
          // wm.autoConnect();
          // digitalWrite(rele[3].pino, HIGH);
          // delay(200);
          // digitalWrite(rele[3].pino, LOW);
          // count = 0;
    }
  }
  if(digitalRead(BTN_CONFIG) == LOW){
    count = 0;
    // Serial.println("sortou");
  }
  delay(100); // Delay para evitar loops excessivos
}

/**/