#include <Arduino.h>
#include <Ethernet.h>
#include <Firebase_ESP_Client.h>
#include <SPIFFS.h>
#include <NTPClient.h>
#include <WiFiManager.h>
#include "vars.h"

// Configuração do Firebase
const char *API_KEY = "AIzaSyCPV9DQPoPoEXSkDYazAehyugOhKm4NhQ0";
const char *DATABASE_URL = "https://poised-bot-443613-p6-default-rtdb.firebaseio.com/";
const char *EMAIL = "leandro.lopes@inovanex.com.br";
const char *EMAIL_PASSWORD = "Inova123NEX";
const char *PROJECT_ID = "poised-bot-443613-p6";
// Definição dos objetos do Firebase
char espUniqueId[13]; // Variable to hold the ESP32 unique ID
String databasePath;  // Variable to hold the database path
FirebaseData firebaseData;
FirebaseAuth auth;
FirebaseConfig config;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -3 * 3600, 60000);

String horaAtivacao = "";
String horaDesativacao = "";
String horarioAtual = "";

bool ethernetConnected = false, wifiConnected = false;
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
WiFiServer wifiServer(80);

// // Declaração das funções auxiliares
// String getTokenType(TokenInfo info);
// String getTokenStatus(TokenInfo info);

// Declaração da função tokenStatusCallback
// void tokenStatusCallback(TokenInfo info);

void salvarDados(const char *caminho, int dados)
{
    File arquivo = SPIFFS.open(caminho, FILE_WRITE);
    if (!arquivo)
    {
        Serial.println(F("Erro ao abrir arquivo!"));
        return;
    }
    arquivo.print(dados);
    arquivo.close();
}

void registrarEvento(const char *releStr, const char *evento)
{
    FirebaseJson content;

    // Adiciona o ID do ESP32 ao JSON
    content.set("fields/espId/stringValue", espUniqueId);

    // Adiciona as informações do relé e do evento
    content.set("fields/rele/stringValue", releStr);
    content.set("fields/evento/stringValue", evento);

    // Obter o epoch time do NTPClient
    time_t epochTime = timeClient.getEpochTime();
    struct tm *timeinfo = localtime(&epochTime);

    // Formatar data e hora
    char dataStr[11]; // "YYYY-MM-DD"
    strftime(dataStr, sizeof(dataStr), "%Y-%m-%d", timeinfo);

    char horaStr[9]; // "HH:MM:SS"
    strftime(horaStr, sizeof(horaStr), "%H:%M:%S", timeinfo);

    // Adicionar data e hora ao JSON
    content.set("fields/data/stringValue", dataStr);
    content.set("fields/hora/stringValue", horaStr);

    // Definir o caminho do documento no Firestore
    String documentPath = "eventos/" + String(random(0, 100000));

    // Enviar os dados para o Firestore
    if (Firebase.Firestore.createDocument(&firebaseData, PROJECT_ID, "", documentPath.c_str(), content.raw()))
    {
        Serial.println(F("Documento criado com sucesso no Firestore."));
    }
    else
    {
        Serial.print(F("Erro ao criar documento: "));
        Serial.println(firebaseData.errorReason());
    }
}

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

String processarHorarios(String dataPath, String jsonData, String jsonKey, String pathEsperado)
{
    if (dataPath != pathEsperado || jsonData.indexOf(jsonKey) == -1)
        return "";
    int startIndex = jsonData.indexOf(jsonKey) + jsonKey.length() + 2;
    int endIndex = jsonData.indexOf(",", startIndex);
    if (endIndex == -1)
        endIndex = jsonData.indexOf("}", startIndex);
    if (startIndex <= 0 || endIndex <= startIndex)
        return "";
    String horario = jsonData.substring(startIndex, endIndex);
    horario.replace("\"", "");
    horario.trim();
    return horario.length() > 8 ? horario.substring(horario.length() - 8) : horario;
}

void reiniciarStream() {
    Firebase.RTDB.endStream(&firebaseData); // Finaliza qualquer stream ativo

    // Reconfigura e tenta reiniciar o stream
    if (!Firebase.RTDB.beginStream(&firebaseData, databasePath.c_str())) {
        Serial.println("Falha ao reiniciar o stream: " + firebaseData.errorReason());
    } else {
        Serial.println("Stream reiniciado com sucesso.");
    }
     streamReiniciado = true; // Sucesso no reinício
}

void tiposBots()
{
      if (!Firebase.RTDB.readStream(&firebaseData)) {
        if (!streamReiniciado) { // Só tenta reiniciar se ainda não foi reiniciado
            Serial.println("Stream desconectado. Tentando reiniciar...");
            reiniciarStream();
        }
        return; // Aguarde a próxima iteração do loop para evitar processamento excessivo
    }

    if (!firebaseData.streamAvailable())
        return;
    streamReiniciado = false;
    String jsonData = firebaseData.to<String>();
    String dataPath = firebaseData.dataPath();
    Serial.println("Dados recebidos: " + jsonData);
         
    if (dataPath == "/keeplive") { // Certifique-se de que o caminho está correto
        // Extraia o valor do campo 'atualizado' recebido
        int startIndex = jsonData.indexOf("\"hora\":\"") + 7;
        int endIndex = jsonData.indexOf("\"", startIndex);
        String horarioAtualizado = jsonData.substring(startIndex, endIndex);

        Serial.println("Hora atualizada recebida: " + horarioAtualizado);
        // Aqui você pode usar o valor recebido como necessário
    }


    for (int i = 0; i < 5; i++)
    {
        String pathRele = "/rele" + String(i + 1);
        if (dataPath == pathRele)
        {
            if (jsonData.indexOf("\"status\":true") != -1)
            {
                digitalWrite(rele[i].pino, HIGH);
                registrarEvento(("Relé " + String(i + 1)).c_str(), "Ativado");
            }
            else if (jsonData.indexOf("\"status\":false") != -1)
            {
                digitalWrite(rele[i].pino, LOW);
                registrarEvento(("Relé " + String(i + 1)).c_str(), "Desativado");
            }
        }
    }
    if (jsonData.indexOf("\"horaAtivacao\":") != -1 && dataPath == "/rele1")
    {
        horaAtivacao = processarHorarios(dataPath, jsonData, "\"horaAtivacao\":", "/rele1");
        Serial.println("Horário de ativação capturado: " + horaAtivacao);
    }
    if (jsonData.indexOf("\"horaDesativacao\":") != -1 && dataPath == "/rele1")
    {
        horaDesativacao = processarHorarios(dataPath, jsonData, "\"horaDesativacao\":", "/rele1");
        Serial.println("Horário de desativação capturado: " + horaDesativacao);
    }
}

void setup()
{
    Serial.begin(115200);
    pinMode(BTN_CONFIG, INPUT);
    digitalWrite(25, HIGH);

    uint64_t chipid = ESP.getEfuseMac();
    sprintf(espUniqueId, "%04X%08X", (uint16_t)(chipid >> 32), (uint32_t)chipid);
    Serial.print("ID único do ESP32: ");
    Serial.println(espUniqueId);

    // Construir o caminho do banco de dados usando o ID único
    databasePath = "/IdsESP/" + String(espUniqueId);

    // Inicializa os pinos dos relés e botões
    for (int i = 0; i < 4; i++)
    {
        pinMode(rele[i].pino, OUTPUT);
        digitalWrite(rele[i].pino, LOW);
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
        WiFiManager wm;
        bool res = wm.autoConnect("AutoConnectAP", "password"); // Nome e senha do ponto de acesso
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
    Serial.println("cheguedsajdsa");
    // Configuração do Firebase
    config.api_key = API_KEY;
    config.database_url = DATABASE_URL; // Adicione esta linha
    auth.user.email = EMAIL;
    auth.user.password = EMAIL_PASSWORD;
    // config.token_status_callback = tokenStatusCallback;

    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);

    FirebaseJson content;
    content.set("fields/espId/stringValue", espUniqueId);

    String documentPath = "eventos/" + String(espUniqueId); // Documento com o ID único
    if (!Firebase.Firestore.createDocument(&firebaseData, PROJECT_ID, "", documentPath.c_str(), content.raw()))
    {
        Serial.print("Erro ao registrar ID no Firestore: ");
        Serial.println(firebaseData.errorReason());
    }
    else
    {
        Serial.println("ID registrado no Firestore com sucesso.");
    }

    // Configurar caminho para o Realtime Database
    databasePath = "/IdsESP/" + String(espUniqueId);

    // Iniciar o stream no Realtime Database
    if (!Firebase.RTDB.beginStream(&firebaseData, databasePath.c_str()))
    {
        Serial.println("Erro ao iniciar o stream do Realtime Database:");
        Serial.println(firebaseData.errorReason());
    }
    else
    {
        Serial.println("Stream do Realtime Database iniciado com sucesso.");
    }

    if (!SPIFFS.begin())
    {
        Serial.println("Falha ao montar o sistema de arquivos!");
        return;
    }

    Serial.println("Sistema de arquivos montado com sucesso!");
}

void loop()
{
    tiposBots();

    // Atualizar o horário NTP
    static unsigned long previousMillis = 0;
    const long interval = 1000;
    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= interval)
    {
        previousMillis = currentMillis;
        timeClient.update();
        horarioAtual = timeClient.getFormattedTime();
        Serial.println("Hora Atual: " + horarioAtual);
    }

    // Verificar horários
    if (horaAtivacao.length() == 8 && horaDesativacao.length() == 8)
    {
        int horaAtualSegundos = timeClient.getHours() * 3600 + timeClient.getMinutes() * 60 + timeClient.getSeconds();
        int horarioAtivacaoSegundos = horaAtivacao.substring(0, 2).toInt() * 3600 +
                                      horaAtivacao.substring(3, 5).toInt() * 60 +
                                      horaAtivacao.substring(6, 8).toInt();

        int horarioDesativacaoSegundos = horaDesativacao.substring(0, 2).toInt() * 3600 +
                                         horaDesativacao.substring(3, 5).toInt() * 60 +
                                         horaDesativacao.substring(6, 8).toInt();

        // Ativar relé no horário correto
        if (horaAtualSegundos == horarioAtivacaoSegundos)
        {
            digitalWrite(rele[0].pino, HIGH);
            Serial.println("Relé 1 ativado!");
        }

        // Desativar relé no horário correto
        if (horaAtualSegundos == horarioDesativacaoSegundos)
        {
            digitalWrite(rele[0].pino, LOW);
            Serial.println("Relé 1 desativado!");
        }
    }

    delay(100); // Pequeno delay para evitar loops excessivos
}
