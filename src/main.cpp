#include <Arduino.h>
#include <FS.h>
#include <Ethernet.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <FirebaseESP32.h>
#include <vars.h>
#include <SPIFFS.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// Variáveis para Firebase
FirebaseData firebaseData;
FirebaseAuth firebaseAuth;
FirebaseConfig firebaseConfig;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -3 * 3600, 60000);

// Variáveis de conexão
WiFiManager wm;
bool ethernetConnected = false;
bool wifiConnected = false;

// Configuração do endereço MAC do Ethernet
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};

// Inicialização do servidor WiFi (quando Ethernet está conectado)
WiFiServer wifiServer(80);

void salvarDados(const char *caminho, int dados)
{
    File arquivo = SPIFFS.open(caminho, FILE_WRITE);
    if (!arquivo)
    {
        Serial.println("Falha ao abrir o arquivo para escrita!");
        return;
    }

    if (arquivo.print(dados))
    {
        Serial.println("Dados salvos com sucesso!");
    }
    else
    {
        Serial.println("Falha ao escrever no arquivo!");
    }
    arquivo.close(); // Fechar o arquivo após a escrita
}

void setup()
{
    Serial.begin(115200);
    pinMode(25, OUTPUT);
    pinMode(BTN_CONFIG, INPUT);
    digitalWrite(25, HIGH);
    pinMode(rele[0].pino, OUTPUT);
    pinMode(rele[1].pino, OUTPUT);
    pinMode(rele[2].pino, OUTPUT);
    pinMode(rele[3].pino, OUTPUT);

    pinMode(rele[0].btn, INPUT);
    pinMode(rele[1].btn, INPUT);
    pinMode(rele[2].btn, INPUT);
    pinMode(rele[3].btn, INPUT);

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

    // Configuração do Firebase
    firebaseConfig.api_key = API_KEY;
    firebaseConfig.database_url = DATABASE_URL;
    firebaseAuth.user.email = EMAIL;
    firebaseAuth.user.password = EMAIL_PASSWORD;
    Firebase.begin(&firebaseConfig, &firebaseAuth);
    Firebase.reconnectWiFi(true);

    // Iniciar o stream do Firebase
    if (!Firebase.beginStream(firebaseData, "/IdsESP/123456"))
    {
        Serial.println("Não foi possível iniciar o stream do Firebase");
        Serial.println(firebaseData.errorReason());
    }
    else
    {
        Serial.println("Stream do Firebase iniciado com sucesso.");
    }

    if (!SPIFFS.begin())
    {
        Serial.println("Falha ao montar o sistema de arquivos!");
        return;
    }

    Serial.println("Sistema de arquivos montado com sucesso!");

    // Iniciar o servidor WiFi se estiver conectado via Ethernet
    if (ethernetConnected)
    {
        WiFi.softAP("ESP32_AP", "123456"); // Configura um ponto de acesso WiFi no ESP32
        wifiServer.begin();
        Serial.println("Ponto de acesso WiFi criado com sucesso para Ethernet.");
    }
}

String processarhorarios(String dataPath, String jsonData, String jsonKey, String pathEsperado)
{
    if (jsonData.indexOf(jsonKey) != -1 && dataPath == pathEsperado)
    {
        int comecoIndex1 = jsonData.indexOf(jsonKey) + jsonKey.length() + 2;
        int finalIndex1 = jsonData.indexOf(",", comecoIndex1);

        if (finalIndex1 == -1)
        {
            finalIndex1 = jsonData.indexOf("}", comecoIndex1);
        }

        String horario = jsonData.substring(comecoIndex1, finalIndex1);
        horario.replace("\"", "");
        horario.trim();
        return horario;
    }
    return "nada";
}

// Função auxiliar para alternar o estado de um relé
void toggleRele(int pino)
{
    if (digitalRead(pino) == LOW)
    {
        digitalWrite(pino, HIGH);
    }
    else
    {
        digitalWrite(pino, LOW);
    }
    delay(200); // Evitar ativação múltipla rápida
}

void tiposBots()
{
     if (!Firebase.readStream(firebaseData))
  {
    Serial.println("Erro no stream /IdsESP/123456, aguardando próximo ciclo...");
    Firebase.endStream(firebaseData);
    if (Firebase.beginStream(firebaseData, "/IdsESP/123456"))
    {
      Serial.println("Contador de loop: ");
      Serial.println("Firebase stream reconectado com sucesso para /IdsESP/123456.");
    }
    else
    {
      Serial.println("Contador de loop: ");
      Serial.println("Não foi possível iniciar o stream do Firebase para /IdsESP/123456");
      Serial.println(firebaseData.errorReason());
    }
  }

  if (firebaseData.streamAvailable())
  {
    Serial.println("Dados atualizados recebidos para /IdsESP/123456:");
    String jsonData = firebaseData.stringData();
    Serial.println(jsonData);
    Serial.println(firebaseData.dataPath());

    if ((jsonData.indexOf("\"status\":true") != -1) && firebaseData.dataPath() == "/rele1")
    {
      Serial.println("Ativando pulso no rele1");
      digitalWrite(rele[0].pino, HIGH);
      delay(200);
      digitalWrite(rele[0].pino, LOW);
      rele[0].status = LOW;

      } else if (jsonData.indexOf("\tipoBotao\":switch") != -1 && firebaseData.dataPath() == "/rele1") {
        Serial.println("ativando modo switch");
        digitalWrite(rele[0].pino, HIGH);
        rele[0].status = HIGH;
    }

    else if ((jsonData.indexOf("\"status\":true") != -1) && (firebaseData.dataPath() == "/rele2"))
    {
      Serial.println("Ativando pulso no rele2");
      digitalWrite(rele[1].pino, HIGH);
      delay(200);
      digitalWrite(rele[1].pino, LOW);
      rele[1].status = LOW;
    }
    else if (jsonData.indexOf("\tipoBotao\":switch") != -1 && firebaseData.dataPath() == "/rele2") {
      Serial.println("ativando modo switch");
      digitalWrite(rele[1].pino, HIGH);
      rele[1].status = HIGH;
    }
    else if ((jsonData.indexOf("\"status\":true") != -1) && (firebaseData.dataPath() == "/rele3"))
    {
      Serial.println("Ativando pulso no rele3");
      digitalWrite(rele[2].pino, HIGH);
      delay(200);
      digitalWrite(rele[2].pino, LOW);
      rele[2].status = LOW;
      } else if (jsonData.indexOf("\tipoBotao\":switch") != -1 && firebaseData.dataPath() == "/rele3") {
        Serial.println("ativando modo switch");
        digitalWrite(rele[2].pino, HIGH);
        rele[2].status = HIGH;
    }
    else if ((jsonData.indexOf("\"status\":true") != -1) && (firebaseData.dataPath() == "/rele4"))
    {
      Serial.println("Ativando pulso no rele4");
      digitalWrite(rele[3].pino, HIGH);
      delay(200);
      digitalWrite(rele[3].pino, LOW);
      rele[3].status = LOW;
    }
    else if (jsonData.indexOf("\tipoBotao\":switch") != -1 && firebaseData.dataPath() == "/rele4") {
      Serial.println("ativando modo switch");
      digitalWrite(rele[3].pino, HIGH);
      rele[3].status = HIGH;
    }
        else if ((jsonData.indexOf("\"status\":true") != -1) && (firebaseData.dataPath() == "/rele5"))
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

        // Variáveis persistentes para horários
        static String horaAtivacao = "";
        static String horaDesativacao = "";
        String horarioAtual = timeClient.getFormattedTime();

        // Capturar horários do Firebase
        if ((jsonData.indexOf("\"horaAtivacao\":") != -1) && firebaseData.dataPath() == "/rele1")
        {
            horaAtivacao = processarhorarios(firebaseData.dataPath(), jsonData, "\"horaAtivacao\":", "/rele1");
            Serial.println("Horário de ativação capturado do Firebase: " + horaAtivacao);
        }

        if (jsonData.indexOf("\"horaDesativacao\":") != -1 && firebaseData.dataPath() == "/rele1")
        {
            horaDesativacao = processarhorarios(firebaseData.dataPath(), jsonData, "\"horaDesativacao\":", "/rele1");
            Serial.println("Horário de desativação capturado do Firebase: " + horaDesativacao);
        }

        // Normalizar strings
        horaAtivacao.trim();
        horaDesativacao.trim();
        horarioAtual.trim();

        // Verificar horários
        if (horaAtivacao.length() == 8 && horaDesativacao.length() == 8)
        {
            int horaAtiva = horaAtivacao.substring(0, 2).toInt();
            int minutoAtiva = horaAtivacao.substring(3, 5).toInt();
            int segundoAtiva = horaAtivacao.substring(6, 8).toInt();

            int horaDesativa = horaDesativacao.substring(0, 2).toInt();
            int minutoDesativa = horaDesativacao.substring(3, 5).toInt();
            int segundoDesativa = horaDesativacao.substring(6, 8).toInt();

            int horaAtual = timeClient.getHours();
            int minutoAtual = timeClient.getMinutes();
            int segundoAtual = timeClient.getSeconds();

            int horarioAtualSegundos = horaAtual * 3600 + minutoAtual * 60 + segundoAtual;
            int horarioAtivacaoSegundos = horaAtiva * 3600 + minutoAtiva * 60 + segundoAtiva;
            int horarioDesativacaoSegundos = horaDesativa * 3600 + minutoDesativa * 60 + segundoDesativa;

            // Verificar ativação
            if (abs(horarioAtualSegundos - horarioAtivacaoSegundos) <= 5)
            {
                digitalWrite(rele[0].pino, HIGH);
                Serial.println("Relé 1 ativado!");
            }

            // Verificar desativação
            if (abs(horarioAtualSegundos - horarioDesativacaoSegundos) <= 5)
            {
                digitalWrite(rele[0].pino, LOW);
                Serial.println("Relé 1 desativado!");
            }
        }
    }
}




unsigned long previousMillis = 0;
const long interval = 1000;

void loop()
{
    if (!Firebase.readStream(firebaseData))
    {
        Serial.println("Erro no stream /IdsESP/123456, aguardando próximo ciclo...");
        Firebase.endStream(firebaseData);
        if (Firebase.beginStream(firebaseData, "/IdsESP/123456"))
        {
            Serial.println("Firebase stream reconectado com sucesso para /IdsESP/123456.");
        }
        else
        {
            Serial.println("Não foi possível iniciar o stream do Firebase para /IdsESP/123456");
            Serial.println(firebaseData.errorReason());
        }
    }

    tiposBots();
    // Atualizar o horário NTP
    static unsigned long previousMillis = 0;
    const long interval = 1000;
    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= interval)
    {
        previousMillis = currentMillis;
        timeClient.update();
        Serial.println("Hora atual do NTP: " + timeClient.getFormattedTime());
    }

    delay(100); // Pequeno delay para evitar loops excessivos
}
