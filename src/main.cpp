#include <Arduino.h>
#include <Ethernet.h>
#include <Firebase_ESP_Client.h>
#include <SPIFFS.h>
#include <NTPClient.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <ESPAsyncWebServer.h>
#include "vars.h"
#include "WebServer.h"

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
WiFiServer wifiServer(80);


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

String getTokenType(TokenInfo info)
{
    switch (info.type)
    {
    case token_type_undefined:
        return "undefined";
    case token_type_legacy_token:
        return "legacy token";
    case token_type_id_token:
        return "id token";
    case token_type_custom_token:
        return "custom token";
    case token_type_oauth2_access_token:
        return "OAuth2.0 access token";
    default:
        return "unknown";
    }
}

String getTokenStatus(TokenInfo info)
{
    switch (info.status)
    {
    case token_status_uninitialized:
        return "uninitialized";
    case token_status_on_signing:
        return "on signing";
    case token_status_on_request:
        return "on request";
    case token_status_on_refresh:
        return "on refresh";
    case token_status_ready:
        return "ready";
    case token_status_error:
        return "error";
    default:
        return "unknown";
    }
}

void tokenStatusCallback(TokenInfo info)
{
    Serial.printf("Token Info: type = %s, status = %s\n",
                  getTokenType(info).c_str(), getTokenStatus(info).c_str());

    if (info.status == token_status_error)
    {
        Serial.printf("Token error: %s\n", info.error.message.c_str());
    }
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

    int startIndex = jsonData.indexOf(jsonKey) + jsonKey.length() + 1;
    int endIndex = jsonData.indexOf(",", startIndex);
    if (endIndex == -1)
        endIndex = jsonData.indexOf("}", startIndex);

    if (startIndex <= 0 || endIndex <= startIndex)
        return "";

    // Extrair o horário exatamente como está no JSON
    String horario = jsonData.substring(startIndex, endIndex);
    horario.replace("\"", ""); // Remove aspas extras
    horario.trim();            // Remove espaços extras

    // Garantir que o formato HH:MM:SS seja mantido sem alterar o JSON original
    if (horario.length() == 8)
    {
        return horario; // Retorna diretamente o horário se já estiver no formato correto
    }

    // Caso contrário, ajusta o formato manualmente
    while (horario.length() < 8)
    {
        horario = "0" + horario; // Adiciona zeros à esquerda se necessário
    }

    return horario;
}

void enviarheartbeat()
{
    String heartbeatPath = databasePath + "/heartbeat";
    String timestamp = String(timeClient.getHours()) + ":" + String(timeClient.getMinutes()) + ":" + String(timeClient.getSeconds());
    if (Firebase.RTDB.setString(&firebaseData, heartbeatPath.c_str(), timestamp))
    {
        Serial.println("HeartBeat enviado: " + timestamp);
    }
    else
    {
        Serial.println("Erro ao enviar HeartBeat: " + firebaseData.errorReason());
    }
}

void atualizarEstadoRele(int rele, int estado)
{
    // Construa o caminho correto para o campo dentro de "rele1" ou "rele2"
    String relePath = "/IdsESP/" + String(espUniqueId) + "/rele" + String(rele) + "/status";

    // Atualiza o campo "status" no caminho especificado
    if (Firebase.RTDB.setInt(&firebaseData, relePath.c_str(), estado))
    {
        Serial.println("Estado do rele " + String(rele) + " atualizado para: " + String(estado));
    }
    else
    {
        Serial.println("Erro ao atualizar estado do rele " + String(rele) + ": " + firebaseData.errorReason());
    }
}

void reiniciarStream()
{
    Firebase.RTDB.endStream(&firebaseData); // Finaliza qualquer stream ativo

    // Reconfigura e tenta reiniciar o stream
    if (!Firebase.RTDB.beginStream(&firebaseData, databasePath.c_str()))
    {
        Serial.println("Falha ao reiniciar o stream: " + firebaseData.errorReason());
    }
    else
    {
        Serial.println("Stream reiniciado com sucesso.");
    }
    streamReiniciado = true; // Sucesso no reinício
}

void tiposBots()
{
    if (!Firebase.RTDB.readStream(&firebaseData))
    {
        if (!streamReiniciado)
        { // Só tenta reiniciar se ainda não foi reiniciado
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

    if (dataPath == "/keeplive")
    { // Certifique-se de que o caminho está correto
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
                atualizarEstadoRele(rele[i].pino, 1);
            }
            else if (jsonData.indexOf("\"status\":false") != -1)
            {
                digitalWrite(rele[i].pino, LOW);
                registrarEvento(("Relé " + String(i + 1)).c_str(), "Desativado");
                atualizarEstadoRele(rele[i].pino, 0);
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

    if (jsonData.indexOf("\"horaAtivacao\":") != -1 && dataPath == "/rele2")
    {
        horaAtivacao2 = processarHorarios(dataPath, jsonData, "\"horaAtivacao\":", "/rele2");
        Serial.println("Horário de ativação capturado: " + horaAtivacao2);
    }
    if (jsonData.indexOf("\"horaDesativacao\":") != -1 && dataPath == "/rele2")
    {
        horaDesativacao2 = processarHorarios(dataPath, jsonData, "\"horaDesativacao\":", "/rele2");
        Serial.println("Horário de desativação capturado: " + horaDesativacao2);
    }

    if (jsonData.indexOf("\"horaAtivacao\":") != -1 && dataPath == "/rele3")
    {
        horaAtivacao3 = processarHorarios(dataPath, jsonData, "\"horaAtivacao\":", "/rele3");
        Serial.println("Horário de ativação capturado: " + horaAtivacao3);
    }
    if (jsonData.indexOf("\"horaDesativacao\":") != -1 && dataPath == "/rele3")
    {
        horaDesativacao3 = processarHorarios(dataPath, jsonData, "\"horaDesativacao\":", "/rele3");
        Serial.println("Horário de desativação capturado: " + horaDesativacao3);
    }

    if (jsonData.indexOf("\"horaAtivacao\":") != -1 && dataPath == "/rele4")
    {
        horaAtivacao4 = processarHorarios(dataPath, jsonData, "\"horaAtivacao\":", "/rele4");
        Serial.println("Horário de ativação capturado: " + horaAtivacao4);
    }
    if (jsonData.indexOf("\"horaDesativacao\":") != -1 && dataPath == "/rele4")
    {
        horaDesativacao4 = processarHorarios(dataPath, jsonData, "\"horaDesativacao\":", "/rele4");
        Serial.println("Horário de desativação capturado: " + horaDesativacao4);
    }

    if (jsonData.indexOf("\"horaAtivacao\":") != -1 && dataPath == "/rele5")
    {
        horaAtivacao5 = processarHorarios(dataPath, jsonData, "\"horaAtivacao\":", "/rele5");
        Serial.println("Horário de ativação capturado: " + horaAtivacao5);
    }
    if (jsonData.indexOf("\"horaDesativacao\":") != -1 && dataPath == "/rele5")
    {
        horaDesativacao5 = processarHorarios(dataPath, jsonData, "\"horaDesativacao\":", "/rele5");
        Serial.println("Horário de desativação capturado: " + horaDesativacao5);
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
    for (int i = 0; i < qtdRele; i++)
    {
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
        WiFiManager wm;
        bool res = wm.autoConnect("AutoConnectAP", "password"); // Nome e senha do ponto de acesso Wi-Fi
        if (!res)
        {
            Serial.println("Falha ao conectar via WiFiManager.");
            // ESP.restart();
            wifiConnected = true;
        }
        else
        {
            Serial.println("Wi-Fi conectado via WiFiManager.");
            Serial.print("Endereço IP: ");
            Serial.println(WiFi.localIP());
            wifiConnected = false;
        }
    }


    Serial.println("cheguedsajdsa");
    // Configuração do Firebase
    config.api_key = API_KEY;
    config.database_url = DATABASE_URL; // Adicione esta linha
    auth.user.email = EMAIL;
    auth.user.password = EMAIL_PASSWORD;
    config.token_status_callback = tokenStatusCallback;

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

    

}

void leRele()
{
    bool valor;
    for (int i = 0; i < qtdRele; i++)
    {
        leitura[i] = digitalRead(rele[i].btn);
        // Serial.println(leitura[i]);
    }
}

void entradaNaSaida()
{
    bool algumON = 0;
    leRele();
    for (int i = 0; i < qtdRele; i++)
    {
        // Serial.println(i);
        if (leitura[i] == 1)
        {
            Serial.printf("Botão do relé %d está pressionado (HIGH)\n", i);
            digitalWrite(rele[i].pino, HIGH);
            registrarEvento(("Relé " + String(i + 1)).c_str(), "Ativado");
            algumON = 1;
        }
    }
    if (algumON == 1)
    {
        //         //*************************** */
        //         display.clearDisplay();
        //         display.fillScreen(WHITE);
        //         display.display();
        //         //*************************** */
        delay(tempoClick);
    }
    // else{
    //         //*************************** */
    //         display.clearDisplay();
    //         display.fillScreen(BLACK);
    //         display.display();
    //         //*************************** */
    // }
    for (int i = 0; i < qtdRele; i++)
    {
        if (leitura[i] == 1)
        {
            digitalWrite(rele[i].pino, LOW);
        }
    }
    for (int i = 0; i < qtdRele; i++)
    {
        if (leitura[i] == 1)
        {
            if (i == 0)
            {
                rele1++;
                salvarDados("/rele1.txt", rele1);
            }
            if (i == 1)
            {
                rele2++;
                salvarDados("/rele2.txt", rele2);
            }
            if (i == 2)
            {
                rele3++;
                salvarDados("/rele3.txt", rele3);
            }
            if (i == 3)
            {
                rele4++;
                salvarDados("/rele4.txt", rele4);
            }
            if (i == 4)
            {
                rele5++;
                salvarDados("/rele5.txt", rele5);
            }
        }
    }
}

void loop()
{
    tiposBots();
    entradaNaSaida();
    static unsigned long lastHeartBeat = 0;
    const unsigned long heartBeatInterval = 60000; // 1 minuto

    // Envia o HeartBeat periodicamente
    if (millis() - lastHeartBeat >= heartBeatInterval)
    {
        lastHeartBeat = millis();
        enviarheartbeat();
    }

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

        int toleranciaSegundos = 2; // Tolerância de 2 segundos

        // Ativar relé no horário correto
        if (abs(horaAtualSegundos - horarioAtivacaoSegundos) <= toleranciaSegundos)
        {
            digitalWrite(rele[0].pino, HIGH);
            Serial.println("Relé 1 ativado!");
            atualizarEstadoRele(1, true);
            registrarEvento("Relé 1", "Ativado");
        }

        // Desativar relé no horário correto
        if (abs(horaAtualSegundos - horarioDesativacaoSegundos) <= toleranciaSegundos)
        {
            digitalWrite(rele[0].pino, LOW);
            Serial.println("Relé 1 desativado!");
            atualizarEstadoRele(1, false);
            registrarEvento("Relé 1", "Desativado");
        }
    }

    if (horaAtivacao2.length() == 8 && horaDesativacao2.length() == 8)
    {
        int horaAtualSegundos2 = timeClient.getHours() * 3600 + timeClient.getMinutes() * 60 + timeClient.getSeconds();
        int horarioAtivacaoSegundos2 = horaAtivacao2.substring(0, 2).toInt() * 3600 +
                                       horaAtivacao2.substring(3, 5).toInt() * 60 +
                                       horaAtivacao2.substring(6, 8).toInt();

        int horarioDesativacaoSegundos2 = horaDesativacao2.substring(0, 2).toInt() * 3600 +
                                          horaDesativacao2.substring(3, 5).toInt() * 60 +
                                          horaDesativacao2.substring(6, 8).toInt();

        int toleranciaSegundos2 = 2; // Tolerância de 2 segundos

        // Ativar relé no horário correto
        if (abs(horaAtualSegundos2 - horarioAtivacaoSegundos2) <= toleranciaSegundos2)
        {
            digitalWrite(rele[1].pino, HIGH);
            Serial.println("Relé 2 ativado!");
            atualizarEstadoRele(2, true);
            registrarEvento("Relé 2", "Ativado");
        }

        // Desativar relé no horário correto
        if (abs(horaAtualSegundos2 - horarioDesativacaoSegundos2) <= toleranciaSegundos2)
        {
            digitalWrite(rele[1].pino, LOW);
            Serial.println("Relé 1 desativado!");
            atualizarEstadoRele(2, false);
            registrarEvento("Relé 2", "Desativado");
        }
    }

    if (horaAtivacao3.length() == 8 && horaDesativacao3.length() == 8)
    {
        int horaAtualSegundos3 = timeClient.getHours() * 3600 + timeClient.getMinutes() * 60 + timeClient.getSeconds();
        int horarioAtivacaoSegundos3 = horaAtivacao3.substring(0, 2).toInt() * 3600 +
                                       horaAtivacao3.substring(3, 5).toInt() * 60 +
                                       horaAtivacao3.substring(6, 8).toInt();

        int horarioDesativacaoSegundos3 = horaDesativacao3.substring(0, 2).toInt() * 3600 +
                                          horaDesativacao3.substring(3, 5).toInt() * 60 +
                                          horaDesativacao3.substring(6, 8).toInt();

        int toleranciaSegundos3 = 2; // Tolerância de 2 segundos

        // Ativar relé no horário correto
        if (abs(horaAtualSegundos3 - horarioAtivacaoSegundos3) <= toleranciaSegundos3)
        {
            digitalWrite(rele[2].pino, HIGH);
            Serial.println("Relé 1 ativado!");
            atualizarEstadoRele(3, true);
            registrarEvento("Relé 3", "Ativado");
        }

        // Desativar relé no horário correto
        if (abs(horaAtualSegundos3 - horarioDesativacaoSegundos3) <= toleranciaSegundos3)
        {
            digitalWrite(rele[2].pino, LOW);
            Serial.println("Relé 1 desativado!");
            atualizarEstadoRele(3, false);
            registrarEvento("Relé 3", "Desativado");
        }
    }

    if (horaAtivacao4.length() == 8 && horaDesativacao4.length() == 8)
    {
        int horaAtualSegundos4 = timeClient.getHours() * 3600 + timeClient.getMinutes() * 60 + timeClient.getSeconds();
        int horarioAtivacaoSegundos4 = horaAtivacao4.substring(0, 2).toInt() * 3600 +
                                       horaAtivacao4.substring(3, 5).toInt() * 60 +
                                       horaAtivacao4.substring(6, 8).toInt();

        int horarioDesativacaoSegundos4 = horaDesativacao4.substring(0, 2).toInt() * 3600 +
                                          horaDesativacao4.substring(3, 5).toInt() * 60 +
                                          horaDesativacao4.substring(6, 8).toInt();

        int toleranciaSegundos4 = 2; // Tolerância de 2 segundos

        // Ativar relé no horário correto
        if (abs(horaAtualSegundos4 - horarioAtivacaoSegundos4) <= toleranciaSegundos4)
        {
            digitalWrite(rele[3].pino, HIGH);
            Serial.println("Relé 1 ativado!");
            atualizarEstadoRele(4, true);
            registrarEvento("Relé 4", "Ativado");
        }

        // Desativar relé no horário correto
        if (abs(horaAtualSegundos4 - horarioDesativacaoSegundos4) <= toleranciaSegundos4)
        {
            digitalWrite(rele[3].pino, LOW);
            Serial.println("Relé 1 desativado!");
            atualizarEstadoRele(4, false);
            registrarEvento("Relé 4", "Desativado");
        }
    }

    if (horaAtivacao5.length() == 8 && horaDesativacao5.length() == 8)
    {
        int horaAtualSegundos5 = timeClient.getHours() * 3600 + timeClient.getMinutes() * 60 + timeClient.getSeconds();
        int horarioAtivacaoSegundos5 = horaAtivacao5.substring(0, 2).toInt() * 3600 +
                                       horaAtivacao5.substring(3, 5).toInt() * 60 +
                                       horaAtivacao5.substring(6, 8).toInt();

        int horarioDesativacaoSegundos5 = horaDesativacao5.substring(0, 2).toInt() * 3600 +
                                          horaDesativacao5.substring(3, 5).toInt() * 60 +
                                          horaDesativacao5.substring(6, 8).toInt();

        int toleranciaSegundos5 = 2; // Tolerância de 2 segundos

        // Ativar relé no horário correto
        if (abs(horaAtualSegundos5 - horarioAtivacaoSegundos5) <= toleranciaSegundos5)
        {
            digitalWrite(rele[4].pino, HIGH);
            Serial.println("Relé 1 ativado!");
            atualizarEstadoRele(5, true);
            registrarEvento("Relé 5", "Ativado");
        }

        // Desativar relé no horário correto
        if (abs(horaAtualSegundos5 - horarioDesativacaoSegundos5) <= toleranciaSegundos5)
        {
            digitalWrite(rele[4].pino, LOW);
            Serial.println("Relé 1 desativado!");
            atualizarEstadoRele(5, false);
            registrarEvento("Relé 5", "Desativado");
        }
    }

    delay(100); // Pequeno delay para evitar loops excessivos
}
