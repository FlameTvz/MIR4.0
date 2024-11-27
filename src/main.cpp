#include <Arduino.h>
#include <Ethernet.h>
#include <Firebase_ESP_Client.h>
#include <SPIFFS.h>
#include <NTPClient.h>
#include <WiFiManager.h>
#include "vars.h"

// Configuração do Firebase
const char *API_KEY = "AIzaSyCSISjoMLyNpbfHNN3RS06WkMx4L21GjTU";
const char *DATABASE_URL = "https://firestore-esp32-de37c-default-rtdb.firebaseio.com/";
const char *EMAIL = "renan.hdk.sgr@gmail.com";
const char *EMAIL_PASSWORD = "180931er";
const char* PROJECT_ID = "firestore-esp32-de37c";

// Definição dos objetos do Firebase
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

// Declaração das funções auxiliares
String getTokenType(TokenInfo info);
String getTokenStatus(TokenInfo info);

// Declaração da função tokenStatusCallback
void tokenStatusCallback(TokenInfo info);

void salvarDados(const char* caminho, int dados) {
    File arquivo = SPIFFS.open(caminho, FILE_WRITE);
    if (!arquivo) {
        Serial.println(F("Erro ao abrir arquivo!"));
        return;
    }
    arquivo.print(dados);
    arquivo.close();
}

void registrarEvento(const char* releStr, const char* evento) {
    FirebaseJson content;
    content.set("fields/rele/stringValue", releStr);
    content.set("fields/evento/stringValue", evento);

    String documentPath = "eventos/" + String(random(0, 100000)); // Cria um ID de documento aleatório

    if (Firebase.Firestore.createDocument(&firebaseData, PROJECT_ID, "", documentPath.c_str(), content.raw())) {
        Serial.println(F("Documento criado com sucesso no Firestore."));
    } else {
        Serial.print(F("Erro ao criar documento: "));
        Serial.println(firebaseData.errorReason());
    }
}

void toggleRele(int pino) {
    if (digitalRead(pino) == LOW) {
        digitalWrite(pino, HIGH);
    } else {
        digitalWrite(pino, LOW);
    }
    delay(200); // Evitar ativação múltipla rápida
}

String processarHorarios(String dataPath, String jsonData, String jsonKey, String pathEsperado) {
    if (dataPath != pathEsperado || jsonData.indexOf(jsonKey) == -1) return "";
    int startIndex = jsonData.indexOf(jsonKey) + jsonKey.length() + 2;
    int endIndex = jsonData.indexOf(",", startIndex);
    if (endIndex == -1) endIndex = jsonData.indexOf("}", startIndex);
    if (startIndex <= 0 || endIndex <= startIndex) return "";
    String horario = jsonData.substring(startIndex, endIndex);
    horario.replace("\"", "");
    horario.trim();
    return horario.length() > 8 ? horario.substring(horario.length() - 8) : horario;
}

void tiposBots() {
    if (!Firebase.RTDB.readStream(&firebaseData)) {
        Serial.println(F("Erro no stream /IdsESP/123456, aguardando próximo ciclo..."));
        Firebase.RTDB.endStream(&firebaseData);
        if (Firebase.RTDB.beginStream(&firebaseData, "/IdsESP/123456"))
            Serial.println(F("Firebase stream reconectado."));
        else
            Serial.println("Falha ao reconectar stream: " + firebaseData.errorReason());
        return;
    }
    if (!firebaseData.streamAvailable()) return;
    String jsonData = firebaseData.to<String>();
    String dataPath = firebaseData.dataPath();
    Serial.println("Dados atualizados recebidos: " + jsonData);
    for (int i = 0; i < 5; i++) {
        String pathRele = "/rele" + String(i + 1);
        if (dataPath == pathRele) {
            if (jsonData.indexOf("\"status\":true") != -1) {
                digitalWrite(rele[i].pino, HIGH);
                registrarEvento(("Relé " + String(i + 1)).c_str(), "Ativado");
            } else if (jsonData.indexOf("\"status\":false") != -1) {
                digitalWrite(rele[i].pino, LOW);
                registrarEvento(("Relé " + String(i + 1)).c_str(), "Desativado");
            }
        }
    }
    if (jsonData.indexOf("\"horaAtivacao\":") != -1 && dataPath == "/rele1") {
        horaAtivacao = processarHorarios(dataPath, jsonData, "\"horaAtivacao\":", "/rele1");
        Serial.println("Horário de ativação capturado: " + horaAtivacao);
    }
    if (jsonData.indexOf("\"horaDesativacao\":") != -1 && dataPath == "/rele1") {
        horaDesativacao = processarHorarios(dataPath, jsonData, "\"horaDesativacao\":", "/rele1");
        Serial.println("Horário de desativação capturado: " + horaDesativacao);
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(BTN_CONFIG, INPUT);
    digitalWrite(25, HIGH);

    // Inicializa os pinos dos relés e botões
    for (int i = 0; i < 5; i++) {
        pinMode(rele[i].pino, OUTPUT);
        digitalWrite(rele[i].pino, LOW);
        pinMode(rele[i].btn, INPUT);
    }

    // Tentar conectar ao Ethernet primeiro com DHCP
    if (Ethernet.begin(mac) == 0) { // Tenta obter IP via DHCP
        Serial.println("Falha ao conectar ao Ethernet. Tentando conectar via WiFi...");
        ethernetConnected = false;
    } else {
        Serial.print("Conectado ao Ethernet com IP: ");
        Serial.println(Ethernet.localIP());
        ethernetConnected = true;
    }

    // Se Ethernet falhar, tentar conectar via WiFiManager
    if (!ethernetConnected) {
        WiFiManager wm;
        bool res = wm.autoConnect("AutoConnectAP", "password"); // Nome e senha do ponto de acesso
        if (res) {
            Serial.println("Conectado ao WiFi via WiFiManager.");
            wifiConnected = true;
        } else {
            Serial.println("Falha ao conectar via WiFiManager.");
            wifiConnected = false;
        }
    }

    // Configuração do Firebase
    config.api_key = API_KEY;
    config.database_url = DATABASE_URL; // Adicione esta linha
    auth.user.email = EMAIL;
    auth.user.password = EMAIL_PASSWORD;
    config.token_status_callback = tokenStatusCallback;

    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);

    // Iniciar o stream do Firebase
    if (!Firebase.RTDB.beginStream(&firebaseData, "/IdsESP/123456")) {
        Serial.println("Não foi possível iniciar o stream do Firebase");
        Serial.println(firebaseData.errorReason());
    } else {
        Serial.println("Stream do Firebase iniciado com sucesso.");
    }

    if (!SPIFFS.begin()) {
        Serial.println("Falha ao montar o sistema de arquivos!");
        return;
    }

    Serial.println("Sistema de arquivos montado com sucesso!");
}

void loop() {
    tiposBots();

    // Atualizar o horário NTP
    static unsigned long previousMillis = 0;
    const long interval = 1000;
    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        timeClient.update();
        horarioAtual = timeClient.getFormattedTime();
        Serial.println("Hora Atual: " + horarioAtual);
    }

    // Verificar horários
    if (horaAtivacao.length() == 8 && horaDesativacao.length() == 8) {
        int horaAtualSegundos = timeClient.getHours() * 3600 + timeClient.getMinutes() * 60 + timeClient.getSeconds();
        int horarioAtivacaoSegundos = horaAtivacao.substring(0, 2).toInt() * 3600 +
                                      horaAtivacao.substring(3, 5).toInt() * 60 +
                                      horaAtivacao.substring(6, 8).toInt();

        int horarioDesativacaoSegundos = horaDesativacao.substring(0, 2).toInt() * 3600 +
                                         horaDesativacao.substring(3, 5).toInt() * 60 +
                                         horaDesativacao.substring(6, 8).toInt();

        // Ativar relé no horário correto
        if (horaAtualSegundos == horarioAtivacaoSegundos) {
            digitalWrite(rele[0].pino, HIGH);
            Serial.println("Relé 1 ativado!");
        }

        // Desativar relé no horário correto
        if (horaAtualSegundos == horarioDesativacaoSegundos) {
            digitalWrite(rele[0].pino, LOW);
            Serial.println("Relé 1 desativado!");
        }
    }

    delay(100); // Pequeno delay para evitar loops excessivos
}

// Função para monitorar o status do token
void tokenStatusCallback(TokenInfo info) {
    Serial.printf("Token info: tipo = %s, status = %s\n", getTokenType(info).c_str(), getTokenStatus(info).c_str());
}

// Funções auxiliares para obter o tipo e status do token
String getTokenType(TokenInfo info) {
    switch (info.type) {
        case token_type_undefined:
            return "Undefined";
        case token_type_legacy_token:
            return "Legacy token";
        case token_type_id_token:
            return "ID token";
        case token_type_custom_token:
            return "Custom token";
        case token_type_oauth2_access_token:
            return "OAuth2 access token";
        default:
            return "Unknown";
    }
}

String getTokenStatus(TokenInfo info) {
    switch (info.status) {
        case token_status_uninitialized:
            return "Uninitialized";
        case token_status_on_initialize:
            return "On initialize";
        case token_status_on_signing:
            return "On signing";
        case token_status_on_request:
            return "On request";
        case token_status_on_refresh:
            return "On refresh";
        case token_status_ready:
            return "Ready";
        case token_status_error:
            return "Error";
        default:
            return "Unknown";
    }
}
