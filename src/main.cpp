#include "vars.h"
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <WiFi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

DNSServer dnsServer;
const byte DNS_PORT = 53;

AsyncWebServer server(80); // Porta padrão HTTP

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
    leRele(); // Atualiza os estados dos botões, etc.

    // Variáveis estáticas internas para gerenciar temporização de cada relé
    static bool estadoRele[5] = {false, false, false, false, false};
    static unsigned long inicioPressionado[5] = {0, 0, 0, 0, 0};

    // Para o modo “Pulso”
    static bool pulsoEmAndamento[5] = {false, false, false, false, false};
    static unsigned long pulsoFim[5] = {0, 0, 0, 0, 0};

    for (int i = 0; i < qtdRele; i++)
    {
        bool botaoPressionado = (digitalRead(rele[i].btn) == HIGH);
        int modo = modoAcionamento[i]; // 0=Manter Ligado, 1=Pulso, 2=Desligar ao soltar

        switch (modo)
        {
        // =========================================================
        // MODO 0: Manter Ligado
        // =========================================================
        case 0:
            if (botaoPressionado)
            {
                if (inicioPressionado[i] == 0)
                {
                    inicioPressionado[i] = millis();
                }
                if (!estadoRele[i] && (millis() - inicioPressionado[i] >= (unsigned long)temposEntrada[i]))
                {
                    digitalWrite(rele[i].pino, HIGH);
                    estadoRele[i] = true;
                    /** AQUI: atualizar o status para o /status */
                    rele[i].status = 1;

                    Serial.printf("Relé %d (modo 0: Manter Ligado) ATIVADO\n", i + 1);
                }
            }
            else
            {
                // Soltou o botão mas no modo 0 não desliga
                inicioPressionado[i] = 0;
            }
            break;

        // =========================================================
        // MODO 1: Pulso
        // =========================================================
        case 1:
            // 1) Detecta pressão do botão até temposEntrada[i] e dispara pulso
            if (botaoPressionado)
            {
                if (inicioPressionado[i] == 0)
                {
                    inicioPressionado[i] = millis();
                }
                if (!pulsoEmAndamento[i] && (millis() - inicioPressionado[i] >= (unsigned long)temposEntrada[i]))
                {
                    // Liga
                    digitalWrite(rele[i].pino, HIGH);
                    estadoRele[i] = true;
                    rele[i].status = 1; // <-- Atualiza aqui também
                    pulsoEmAndamento[i] = true;
                    pulsoFim[i] = millis() + temposPulso[i];

                    Serial.printf("Relé %d (modo 1: Pulso) LIGADO\n", i + 1);
                }
            }
            else
            {
                // Botão solto antes de atingir tempo => reseta
                inicioPressionado[i] = 0;
            }

            // 2) Se o pulso estiver em andamento, verifica se acabou
            if (pulsoEmAndamento[i] && (millis() >= pulsoFim[i]))
            {
                // Desliga
                digitalWrite(rele[i].pino, LOW);
                estadoRele[i] = false;
                rele[i].status = 0; // <-- Importante
                pulsoEmAndamento[i] = false;

                Serial.printf("Relé %d (modo 1: Pulso) DESLIGADO (fim do pulso)\n", i + 1);
            }
            break;

        // =========================================================
        // MODO 2: Desligar ao soltar (código original)
        // =========================================================
        case 2:
            if (botaoPressionado)
            {
                if (inicioPressionado[i] == 0)
                {
                    inicioPressionado[i] = millis();
                }
                if (!estadoRele[i] && (millis() - inicioPressionado[i] >= (unsigned long)temposEntrada[i]))
                {
                    digitalWrite(rele[i].pino, HIGH);
                    estadoRele[i] = true;
                    rele[i].status = 1; // <-- Atualiza
                    Serial.printf("Relé %d (modo 2: Desligar ao soltar) LIGADO\n", i + 1);
                }
            }
            else
            {
                // Botão solto
                if (estadoRele[i])
                {
                    digitalWrite(rele[i].pino, LOW);
                    estadoRele[i] = false;
                    rele[i].status = 0; // <-- Atualiza
                    Serial.printf("Relé %d (modo 2: Desligar ao soltar) DESLIGADO\n", i + 1);
                }
                inicioPressionado[i] = 0;
            }
            break;
        } // switch (modo)
    } // for
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

void salvarConfiguracoes() {
    // Cria um documento JSON e preenche com os valores atuais
    StaticJsonDocument<512> doc; 

    JsonArray arr = doc.createNestedArray("reles");
    for (int i = 0; i < qtdRele; i++) {
        JsonObject obj = arr.createNestedObject();
        obj["tempoPulso"] = temposPulso[i];
        obj["tempoEntrada"] = temposEntrada[i];
        obj["horaAtivacao"] = rele[i].horaAtivacao;
        obj["horaDesativacao"] = rele[i].horaDesativacao;
        // se quiser salvar "nome" também: obj["nome"] = rele[i].nome;
    }

    // Abre/cria o arquivo e escreve
    File file = SPIFFS.open("/configRele.json", FILE_WRITE);
    if (file) {
        serializeJson(doc, file);
        file.close();
        Serial.println("Configurações salvas em /configRele.json");
    } else {
        Serial.println("Falha ao abrir /configRele.json para escrita");
    }
}

void carregarConfiguracoes() {
    if(!SPIFFS.exists("/configRele.json")) {
        Serial.println("Nenhum arquivo /configRele.json encontrado, usando valores padrão.");
        return;
    }
    File file = SPIFFS.open("/configRele.json", FILE_READ);
    if (!file) {
        Serial.println("Falha ao abrir /configRele.json para leitura");
        return;
    }

    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, file);
    if (error) {
        Serial.println("Erro ao fazer parse do JSON de config: ");
        Serial.println(error.f_str());
        file.close();
        return;
    }

    file.close();
    JsonArray arr = doc["reles"];
    if (!arr.isNull()) {
        for (int i = 0; i < arr.size() && i < qtdRele; i++) {
            JsonObject obj = arr[i];
            temposPulso[i] = obj["tempoPulso"] | 200;
            temposEntrada[i] = obj["tempoEntrada"] | 200;
            rele[i].horaAtivacao = obj["horaAtivacao"] | "";
            rele[i].horaDesativacao = obj["horaDesativacao"] | "";
            // se tiver nome: rele[i].nome = obj["nome"] | ("Relé " + String(i+1));
        }
        Serial.println("Configurações carregadas de /configRele.json");
    }
}

void atualizarEstadoRele(int pino, int estado, int numRele)
{
    // Exemplo: /IdsESP/<espUniqueId>/rele1/status, caso numRele == 1
    String relePath = "/IdsESP/" + String(espUniqueId) + "/rele" + String(numRele) + "/status";

    // Atualiza o campo "status" no caminho especificado
    if (Firebase.RTDB.setInt(&firebaseData, relePath.c_str(), estado))
    {
        Serial.println("Estado do Rele " + String(numRele) + " (pino " + String(pino) + ") atualizado para: " + String(estado));
    }
    else
    {
        Serial.println("Erro ao atualizar estado do Rele " + String(numRele) + " (pino " + String(pino) + "): " + firebaseData.errorReason());
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
                             /*************  ✨ Codeium Command ⭐  *************/
                             /**
                              * Função que lida com a leitura de dados recebidos via Stream do Firebase.
                              *
                              * Verifica se o stream está disponível e, se sim, extrai o valor do campo 'atualizado' recebido.
                              * Em seguida, verifica se o caminho recebido é igual a "/keeplive" e, se sim, imprime o horário atualizado recebido.
                              *
                              * Além disso, verifica se o caminho recebido é igual a "/releX" (onde X é o número do relé) e, se sim, extrai o valor do campo 'status' recebido e atualiza o estado do relé correspondente.
                              *
                              * Por fim, verifica se o caminho recebido é igual a "/releX" e o valor do campo 'horaAtivacao' ou 'horaDesativacao' foi alterado, e, se sim, atualiza o horário de ativação ou desativação do relé correspondente.
                              *
                              * @return Nenhum valor de retorno.
                              */
/******  dbfb5ebf-b8f3-4b89-a324-2a6862f6bd15  *******/}

void tiposBots()
{
    if (!Firebase.RTDB.readStream(&firebaseData))
    {
        if (!streamReiniciado)
        { // Só tenta reiniciar se ainda não foi reiniciado
            Serial.println("Stream desconectado. Tentando reiniciar...");
            reiniciarStream();
        }
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
                // atualizarEstadoRele(rele[i].pino, 1);
                atualizarEstadoRele(rele[i].pino, true, i + 1);
            }
            else if (jsonData.indexOf("\"status\":false") != -1)
            {
                digitalWrite(rele[i].pino, LOW);
                registrarEvento(("Relé " + String(i + 1)).c_str(), "Desativado");
                // atualizarEstadoRele(rele[i].pino, 0);
                atualizarEstadoRele(rele[i].pino, false, i + 1);
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

    if (jsonData.indexOf("\"tipoBotao\":\"switch") != -1 && dataPath == "/rele1")
    {
        digitalWrite(rele[0].pino, HIGH);
        delay(200);
        digitalWrite(rele[0].pino, LOW);
        registrarEvento("Relé 1", "Ativado e Desativado");
    }
    if (jsonData.indexOf("\"tipoBotao\":\"switch") != -1 && dataPath == "/rele2")
    {
        digitalWrite(rele[1].pino, HIGH);
        delay(200);
        digitalWrite(rele[1].pino, LOW);
        registrarEvento("Relé 2", "Ativado e Desativado");
    }
    if (jsonData.indexOf("\"tipoBotao\":\"switch") != -1 && dataPath == "/rele3")
    {
        digitalWrite(rele[2].pino, HIGH);
        delay(200);
        digitalWrite(rele[2].pino, LOW);
        registrarEvento("Relé 3", "Ativado e Desativado");
    }
    if (jsonData.indexOf("\"tipoBotao\":\"switch") != -1 && dataPath == "/rele4")
    {
        digitalWrite(rele[3].pino, HIGH);
        delay(200);
        digitalWrite(rele[3].pino, LOW);
        registrarEvento("Relé 4", "Ativado e Desativado");
    }
    if (jsonData.indexOf("\"tipoBotao\":\"switch") != -1 && dataPath == "/rele5")
    {
        digitalWrite(rele[4].pino, HIGH);
        delay(200);
        digitalWrite(rele[4].pino, LOW);
        registrarEvento("Relé 5", "Ativado e Desativado");
    }
}

void atualizarEstadoRele2(int rele, int estado)
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

void verificarHorarioReles(String ativacao, String desativacao, int pino, int releNum)
{
    if (ativacao.length() == 8 && desativacao.length() == 8)
    {
        int horaAtualSeg = timeClient.getHours() * 3600 + timeClient.getMinutes() * 60 + timeClient.getSeconds();
        int horaAtivacaoSeg = ativacao.substring(0, 2).toInt() * 3600 +
                              ativacao.substring(3, 5).toInt() * 60 + ativacao.substring(6, 8).toInt();
        int horaDesativacaoSeg = desativacao.substring(0, 2).toInt() * 3600 +
                                 desativacao.substring(3, 5).toInt() * 60 + desativacao.substring(6, 8).toInt();

        int tolerancia = 2; // Tolerância de 2 segundos
        if (abs(horaAtualSeg - horaAtivacaoSeg) <= tolerancia)
        {
            digitalWrite(pino, HIGH);
            Serial.printf("Relé %d ativado!\n", releNum);
            atualizarEstadoRele2(releNum, true);
            registrarEvento(("Relé " + String(releNum)).c_str(), "Ativado");
        }
        if (abs(horaAtualSeg - horaDesativacaoSeg) <= tolerancia)
        {
            digitalWrite(pino, LOW);
            Serial.printf("Relé %d desativado!\n", releNum);
            atualizarEstadoRele2(releNum, false);
            registrarEvento(("Relé " + String(releNum)).c_str(), "Desativado");
        }
    }
}

const char *htmlPage PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-br">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Configuração Wi-Fi</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            background-color: #f2f2f2;
            color: #333;
            text-align: center;
            margin: 0;
            padding: 0;
        }
        h1 {
            margin-top: 20px;
            color: #444;
        }
        form {
            display: inline-block;
            background-color: #fff;
            padding: 20px;
            border-radius: 8px;
            box-shadow: 0 0 10px rgba(0, 0, 0, 0.1);
            margin-top: 20px;
        }
        input[type="text"], input[type="password"] {
            width: 100%;
            padding: 10px;
            margin: 10px 0;
            border: 1px solid #ccc;
            border-radius: 4px;
            box-sizing: border-box;
        }
        input[type="submit"] {
            background-color: #4CAF50;
            color: white;
            border: none;
            padding: 10px 20px;
            border-radius: 4px;
            cursor: pointer;
            font-size: 16px;
        }
        input[type="submit"]:hover {
            background-color: #45a049;
        }
    </style>
</head>
<body>
    <h1>Configuração Wi-Fi</h1>
    <form action="/connect" method="post">
        <label for="ssid">Nome da Rede (SSID):</label>
        <input type="text" id="ssid" name="ssid" required>

        <label for="password">Senha:</label>
        <input type="password" id="password" name="password" required>

        <input type="submit" value="Conectar">
    </form>
</body>
</html>
)rawliteral";

void startWiFiManager()
{
    WiFi.mode(WIFI_AP_STA);
    const char *ssid = "ESP32_Config";
    const char *password = "12345678";

    Serial.println("Iniciando modo Access Point...");
    WiFi.softAP(ssid, password);

    IPAddress apIP(192, 168, 4, 1); // IP padrão do portal
    dnsServer.start(DNS_PORT, "*", apIP);

    // Página principal
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              {
                  request->send_P(200, "text/html", htmlPage); // Servir a página HTML
              });

    // Configuração Wi-Fi
    server.on("/connect", HTTP_POST, [](AsyncWebServerRequest *request)
              {
    String ssid, password;

    if (request->hasParam("ssid", true)) ssid = request->getParam("ssid", true)->value();
    if (request->hasParam("password", true)) password = request->getParam("password", true)->value();

    if (!ssid.isEmpty()) {
        Serial.println("Tentando conectar à rede: " + ssid);
        WiFi.begin(ssid.c_str(), password.c_str());

        delay(2000); // Pequena pausa para iniciar a conexão
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("Conectado com sucesso! IP: " + WiFi.localIP().toString());

            // Salvar SSID e senha na SPIFFS
            File wifiConfig = SPIFFS.open("/wifi_config.txt", FILE_WRITE);
            if (wifiConfig) {
                wifiConfig.println(ssid);
                wifiConfig.println(password);
                wifiConfig.close();
                Serial.println("Configuração Wi-Fi salva.");
            } else {
                Serial.println("Erro ao salvar configuração Wi-Fi.");
            }

            request->send(200, "text/plain", "Conectado com sucesso! Reinicie o ESP32.");
        } else {
            Serial.println("Falha na conexão.");
            request->send(200, "text/plain", "Falha ao conectar. Tente novamente.");
        }
    } });

    server.begin();
    Serial.println("Servidor HTTP iniciado!");

    while (WiFi.status() != WL_CONNECTED)
    {
        dnsServer.processNextRequest(); // Processa os pacotes DNS
        delay(100);
    }
}

String paginaPrincipal(int qtdRele,
                       int releNumber,
                       int tempoPulsoGlobal,
                       int tempoEntradaGlobal,
                       const String &horaAtivacaoGlobal,
                       const String &horaDesativacaoGlobal)
{
    // HTML completo, unificando página de controle + formulários
    String html = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-br">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Controle de Relés</title>
    <link rel="preconnect" href="https://fonts.gstatic.com" />
    <link href="https://fonts.googleapis.com/css2?family=Poppins:wght@300;500;700&display=swap" rel="stylesheet" />
    <style>
       <!DOCTYPE html>

       /* =====================================================================
          CSS original da página principal + página de configuração
          (NENHUM trecho foi removido ou renomeado, apenas mantido integralmente)
       ===================================================================== */
       * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }

        body {
            font-family: "Poppins", Arial, sans-serif;
            background: linear-gradient(120deg, #fdfbfb 0%, #ebedee 100%);
            color: #333;
            margin: 0;
            text-align: center;
        }

        header {
            background-color: #34495e;
            padding: 20px;
            box-shadow: 0 2px 5px rgba(0, 0, 0, 0.2);
        }

        header h1 {
            color: #fff;
            font-weight: 700;
            font-size: 2.2rem;
        }

        #reles-container {
            display: flex;
            flex-wrap: nowrap;
            overflow-x: auto;
            gap: 15px;
            padding: 30px;
            margin: 0 auto;
            max-width: 95%;
        }

        .rele-card {
            flex: 0 0 auto;
            width: 270px;
            min-height: 180px;
            padding: 20px;
            border-radius: 15px;
            background: #fff;
            box-shadow: 0 3px 10px rgba(0, 0, 0, 0.1);
            text-align: center;
            transition: transform 0.2s ease, box-shadow 0.2s ease;
            
            display: flex;
            flex-direction: column;
            justify-content: flex-start; /* Ajuste para acomodar formulário abaixo */
            align-items: center;
        }

        .rele-card:hover {
            transform: translateY(-5px);
            box-shadow: 0 10px 15px rgba(0, 0, 0, 0.15);
        }

        /* Título do card */
        .rele-card h2 {
            margin: 10px 0;
            font-size: 1.2rem;
            color: #007BFF;
        }

        /* Estado do relé (Ligado/Desligado) */
        .estado {
            margin: 10px 0;
            display: inline-block;
            padding: 8px 16px;
            border-radius: 30px;
            font-weight: 500;
            font-size: 1rem;
            min-width: 90px;
            text-align: center;
            box-shadow: 0 3px 6px rgba(0, 0, 0, 0.1);
        }

        .estado.ligado {
            background: #27ae60;
            color: #fff;
        }
        .estado.desligado {
            background: #e74c3c;
            color: #fff;
        }

        .botoes {
            display: flex;
            justify-content: space-around;
            align-items: center;
            gap: 10px;
            margin-top: 15px;
            width: 100%;
        }
        button {
            flex: 1;
            padding: 10px;
            border: none;
            border-radius: 5px;
            color: #fdfbfb;
            font-size: 0.9rem;
            font-weight: 500;
            cursor: pointer;
            transition: background 0.2s, transform 0.2s, box-shadow 0.2s;
            outline: none;
        }
        button:hover {
            transform: translateY(-2px);
            box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1);
        }
        .btn-liga-desliga {
            background: #2c3e50;
        }
        .btn-liga-desliga:hover {
            background: #2c3e50;
        }
        .btn-switch {
            color: #fff; 
            background: #2c3e50;
        }
        .btn-switch:hover {
            background: #2c3e50;
        }

        @media (max-width: 1200px) {
            #reles-container {
                max-width: 100%;
                padding: 20px;
            }
            .rele-card {
                width: calc(25% - 15px);
            }
        }
        @media (max-width: 992px) {
            .rele-card {
                width: calc(33.33% - 15px);
            }
        }
        @media (max-width: 768px) {
            #reles-container {
                flex-wrap: wrap;
                justify-content: center;
                overflow-x: visible;
            }
            .rele-card {
                width: calc(50% - 15px);
                margin: 10px auto;
            }
        }
        @media (max-width: 576px) {
            .rele-card {
                width: calc(100% - 20px);
                max-width: 350px;
                margin: 10px auto;
            }
        }

        /* ====================
           CSS da antiga "paginaConfiguracaoRele" 
           Mantemos o que já existia
        ==================== */
        .container {
            max-width: 400px;
            margin: 20px auto; /* Ajustado para ficar mais próximo do card */
            background: #fff;
            border-radius: 8px;
            padding: 20px;
            box-shadow: 0 3px 10px rgba(0, 0, 0, 0.1);
            text-align: left;  /* para o form */
        }

        .container h1 {
            margin-bottom: 20px;
            color: #2c3e50;
            font-weight: 700;
            font-size: 1.4rem; /* um pouco menor para caber melhor no card */
            text-align: center;/* centraliza o título */
        }

        form {
            text-align: left;
        }

        label {
            display: block;
            margin-bottom: 6px;
            font-weight: 500;
            color: #333;
        }

        input[type="text"],
        input[type="number"],
        input[type="time"] {
            width: 100%;
            margin-bottom: 15px;
            padding: 10px;
            border: 1px solid #ccc;
            border-radius: 5px;
            box-sizing: border-box;
            font-size: 14px;
        }

        button[type="submit"] {
            background: #007BFF;
            border: none;
            color: #fff;
            padding: 12px 20px;
            font-size: 16px;
            border-radius: 5px;
            cursor: pointer;
            transition: background 0.3s, transform 0.3s;
            font-weight: 500;
            display: block;
            margin: 0 auto;
            width: fit-content;
        }
        button[type="submit"]:hover {
            background: #0056b3;
            transform: translateY(-2px);
        }

        .back-button {
            background: #dc3545;
            color: #fff;
            text-decoration: none;
            padding: 12px 20px;
            font-size: 16px;
            border-radius: 5px;
            transition: background 0.3s, transform 0.3s;
            font-weight: 500;
            display: block;
            margin: 10px auto 0;
            width: fit-content;
            text-align: center;
        }
        .back-button:hover {
            background: #c82333;
            transform: translateY(-2px);
        }

        /* ================================
           NOVA CLASSE PARA O SELECT
        ================================ */
        .select-modo {
            width: 100%;
            padding: 10px;
            background-color: #2c3e50; /* Mesma cor dos botões */
            color: #fff;
            font-size: 0.9rem;
            font-weight: 500;
            border: none;
            border-radius: 5px;
            margin-bottom: 15px;
            cursor: pointer;
            transition: background 0.2s, transform 0.2s, box-shadow 0.2s;
        }
        .select-modo:hover {
            background-color: #3b4d5e; 
            transform: translateY(-2px);
            box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1);
        }
        .select-modo option {
            color: #333;   /* Cor do texto interno das opções */
            background: #fff;
        }

    </style>
</head>
<body>
    <header>
      <h1>Controle de Relés</h1>
    </header>

    <div id="reles-container">
        %CONTROLES%
    </div>

    <script>
        // Atualiza o status dos relés a cada 250ms
        setInterval(() => {
            fetch('/status')
                .then(response => response.json())
                .then(data => {
                    data.reles.forEach((rele, index) => {
                        const estadoEl = document.getElementById(`estado-rele-${index}`);
                        const ligaDesligaBtn = document.getElementById(`liga-desliga-btn-${index}`);

                        if (!estadoEl || !ligaDesligaBtn) return;

                        // Atualiza apenas o estado visual
                        if (rele.status) {
                            estadoEl.textContent = "Ligado";
                            estadoEl.classList.add("ligado");
                            estadoEl.classList.remove("desligado");
                            ligaDesligaBtn.textContent = "Desligar";
                        } else {
                            estadoEl.textContent = "Desligado";
                            estadoEl.classList.add("desligado");
                            estadoEl.classList.remove("ligado");
                            ligaDesligaBtn.textContent = "Ligar";
                        }

                        // Define os event listeners apenas uma vez
                        if (!ligaDesligaBtn.hasEventListener) {
                            ligaDesligaBtn.hasEventListener = true;
                            ligaDesligaBtn.addEventListener('click', () => {
                                const acao = rele.status ? 'desligar' : 'ligar';
                                fetch(`/controle?rele=${index + 1}&acao=${acao}`)
                                    .then(() => {
                                        // Atualiza o status localmente
                                        rele.status = !rele.status;
                                    })
                                    .catch(err => console.error("Erro ao controlar relé:", err));
                            });
                        }
                    });
                })
                .catch(err => console.error("Erro ao buscar status:", err));
        }, 250);
    </script>
</body>
</html>
)rawliteral";

    // Aqui montamos o conteúdo do %CONTROLES%, incluindo:
    //  - o card do relé
    //  - o formulário de configuração, logo abaixo, sem botão “Configurar”
    String controles;
    for (int i = 0; i < qtdRele; i++)
    {
        int tPulso = temposPulso[i];
        int tEntrada = temposEntrada[i];
        String hAtiv = rele[i].horaAtivacao;
        String hDesativ = rele[i].horaDesativacao;

        controles += "<div class='rele-card'>";
        controles += "<h2 contenteditable='true' ";
        controles += " onblur='salvarNomeRele(" + String(i + 1) + ", this.innerText)'>";
        controles += rele[i].nome;
        controles += "</h2>";
        controles += "  <div class='estado desligado' id='estado-rele-" + String(i) + "'>Desligado</div>";
        controles += "  <div class='botoes'>";
        controles += "    <button class='btn-liga-desliga' id='liga-desliga-btn-" + String(i) + "'>Ligar</button>";
        controles += "    <button class='btn-switch' onclick='fetch(`/switch?rele=" + String(i + 1) + "`)'>Pulso</button>";
        controles += "  </div>";
        controles += "  <div class='container'>";
        controles += "    <form action='/salvarconfig' method='POST'>";
        controles += "      <input type='hidden' name='rele' value='" + String(i + 1) + "' />";
        controles += "      <label>Tempo do Pulso (ms):</label>";
        controles += "      <input type='number' name='tempoPulso' min='0' placeholder='Ex: 10000' value='" + String(tPulso) + "' />";
        controles += "      <label>Tempo de Debounce (ms):</label>";
        controles += "      <input type='number' name='tempoEntrada' min='0' placeholder='Ex: 500' value='" + String(tEntrada) + "' />";
        controles += "      <label>Modo de Acionamento:</label>";
        controles += "      <select name='modoRele' class='select-modo'>";
        controles += "        <option value='0' " + String(modoAcionamento[i] == 0 ? "selected" : "") + ">Manter Ligado</option>";
        controles += "        <option value='1' " + String(modoAcionamento[i] == 1 ? "selected" : "") + ">Pulso</option>";
        controles += "        <option value='2' " + String(modoAcionamento[i] == 2 ? "selected" : "") + ">Desligar ao soltar</option>";
        controles += "      </select>";
        controles += "      <label>Hora de Ativação:</label>";
        controles += "      <input type='time' name='horaAtivacao' step='1' value='" + hAtiv + "' />";
        controles += "      <label>Hora de Desativação:</label>";
        controles += "      <input type='time' name='horaDesativacao' step='1' value='" + hDesativ + "' />";
        controles += "      <button type='submit'>Salvar Configuração</button>";
        controles += "    </form>";
        controles += "  </div>"; // .container
        controles += "</div>";
    }
    html.replace("%CONTROLES%", controles);

    String scriptJS = R"rawliteral(
    <script>
    // Função chamada no onblur do <h2 contenteditable="true">
    function salvarNomeRele(releIndex, novoNome) {
        // Remove espaços extras nas pontas, se quiser
        novoNome = novoNome.trim();
        if(!novoNome) return; // se vazio, não faz nada ou poderia mandar algo

        // Fazemos um fetch GET para /salvarnome?rele=X&nome=...
        fetch(`/salvarnome?rele=${releIndex}&nome=${encodeURIComponent(novoNome)}`)
            .then(response => {
                if(!response.ok){
                    console.error("Erro ao salvar nome do relé:", response.statusText);
                }
            })
            .catch(err => console.error("Erro fetch /salvarnome:", err));
    }
    </script>
    </body>
    </html>
    )rawliteral";

    html += scriptJS;

    return html;
}

void configurarWebServer()
{
    // Rota raiz (carrega a página principal para, por exemplo, o relé 1 - ou você pode ajustar
    // para exibir algum relé "padrão". Caso queira, pode configurar para não passar parâmetros
    // e usar valores default.)
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        // Exemplo chamando a paginaPrincipal para relé 1 (releNumber=1),
        // ou ajuste conforme sua lógica.
        int releNumber = 1;
        int tempoPulso = temposPulso[releNumber - 1];
        int tEntrada = temposEntrada[releNumber - 1];
        String horaAtivacao = rele[releNumber - 1].horaAtivacao;
        String horaDesativacao = rele[releNumber - 1].horaDesativacao;

        request->send(200, "text/html",
            paginaPrincipal(qtdRele, releNumber, tempoPulso, tEntrada, horaAtivacao, horaDesativacao)); });

    // Liga/Desliga
    server.on("/controle", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (request->hasParam("rele") && request->hasParam("acao")) {
            int releIdx = request->getParam("rele")->value().toInt() - 1;
            String acao = request->getParam("acao")->value();
            if (releIdx >= 0 && releIdx < qtdRele) {
                if (acao == "ligar") {
                    digitalWrite(rele[releIdx].pino, HIGH);
                    rele[releIdx].status = 1; // Atualiza status
                } else if (acao == "desligar") {
                    digitalWrite(rele[releIdx].pino, LOW);
                    rele[releIdx].status = 0; // Atualiza status
                }
                request->send(200, "text/plain", "OK");
            } else {
                request->send(400, "text/plain", "Relé inválido");
            }
        } else {
            request->send(400, "text/plain", "Parâmetros inválidos");
        } });

    // Exemplo de rota POST para configurar apenas horaAtivacao/horaDesativacao (se ainda existir)
    server.on("/configurar", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        if (request->hasParam("rele", true) && 
            request->hasParam("horaAtivacao", true) && 
            request->hasParam("horaDesativacao", true))
        {
            int releIdx = request->getParam("rele", true)->value().toInt() - 1;
            if (releIdx >= 0 && releIdx < 5) {
                rele[releIdx].horaAtivacao = request->getParam("horaAtivacao", true)->value();
                rele[releIdx].horaDesativacao = request->getParam("horaDesativacao", true)->value();
                request->send(200, "text/plain", "Horários configurados");
                request->redirect("/");
            } else {
                request->send(400, "text/plain", "Relé inválido");
            }
        } else {
            request->send(400, "text/plain", "Parâmetros inválidos");
        } });

    // Switch (acionamento temporário)
    server.on("/switch", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (request->hasParam("rele"))
        {
            int releIdx = request->getParam("rele")->value().toInt() - 1;
            if (releIdx >= 0 && releIdx < qtdRele)
            {
                if (!switchAtual.ativo)
                {
                    digitalWrite(rele[releIdx].pino, HIGH);
                    rele[releIdx].status = 1;
                    switchAtual.ativo = true;
                    switchAtual.inicio = millis();
                    switchAtual.releIdx = releIdx;
                    request->send(200, "text/plain", "Switch ativado!");
                }
                else
                {
                    request->send(400, "text/plain", "Outro switch está ativo.");
                }
            }
            else
            {
                request->send(400, "text/plain", "Relé inválido.");
            }
        }
        else
        {
            request->send(400, "text/plain", "Parâmetro 'rele' ausente.");
        } });

    // Retorno do status (JSON)
    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        String response = "{ \"reles\": [";
        for (int i = 0; i < qtdRele; i++) {
            response += "{";
            response += "\"status\": " + String(rele[i].status);
            response += "}";
            if (i < qtdRele - 1) response += ",";
        }
        response += "] }";
        request->send(200, "application/json", response); });

    // Ajuste de tempos via POST (pulso/entrada)
    server.on("/configurartemporele", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        if (request->hasParam("rele", true) && 
            request->hasParam("tempoPulso", true) && 
            request->hasParam("tempoEntrada", true))
        {
            int releIdx = request->getParam("rele", true)->value().toInt() - 1;
            int novoTempoPulso = request->getParam("tempoPulso", true)->value().toInt();
            int novoTempoEntrada = request->getParam("tempoEntrada", true)->value().toInt();

            if (releIdx >= 0 && releIdx < 5) {
                temposPulso[releIdx] = novoTempoPulso;
                tempoEntrada = novoTempoEntrada; // Se for um único global, mantenha. Senão ajuste.
                Serial.printf("Tempo do pulso do relé %d: %d ms\n", releIdx + 1, novoTempoPulso);
                Serial.printf("Tempo de entrada: %d ms\n", novoTempoEntrada);
                request->send(200, "text/plain", "Tempos atualizados");
                request->redirect("/");
            } else {
                request->send(400, "text/plain", "Relé inválido");
            }
        } else {
            request->send(400, "text/plain", "Parâmetros inválidos");
        } });

    server.on("/salvarnome", HTTP_GET, [](AsyncWebServerRequest *request)
              {
    if (request->hasParam("rele") && request->hasParam("nome"))
    {
        int releIdx = request->getParam("rele")->value().toInt() - 1;
        String novoNome = request->getParam("nome")->value();

        if (releIdx >= 0 && releIdx < qtdRele)
        {
            // Salva no struct
            rele[releIdx].nome = novoNome;
            Serial.printf("Nome do Relé %d alterado para: %s\n", releIdx + 1, novoNome.c_str());
            request->send(200, "text/plain", "OK");
        }
        else
        {
            request->send(400, "text/plain", "Relé inválido.");
        }
    }
    else
    {
        request->send(400, "text/plain", "Parâmetros ausentes.");
    } });

    // Exemplo de configuração de “tempo de entrada” separado (se você ainda precisar)
    server.on("/configurarentrada", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        if (request->hasParam("tempo", true)) {
            String tempoStr = request->getParam("tempo", true)->value();
            int novoTempo = tempoStr.toInt();
            if (novoTempo > 0) {
                tempoEntrada = novoTempo;
                Serial.println("Tempo da entrada atualizado para: " + String(tempoEntrada) + " ms");
                request->redirect("/");
            } else {
                request->send(400, "text/plain", "Valor inválido para o tempo do pulso da entrada");
            }
        } else {
            request->send(400, "text/plain", "Parâmetro 'tempo' ausente");
        } });

    // IMPORTANTE:
    // A rota /configurartempo AGORA usa a paginaPrincipal(...) no lugar de paginaConfiguracaoRele(...)
    // para exibir a mesma página unificada (sem excluir a rota ou alterar nomes).
    server.on("/configurartempo", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (request->hasParam("rele"))
        {
            int releIdx = request->getParam("rele")->value().toInt() - 1;

            // Busca valores do relé específico
            int tempoPulso = temposPulso[releIdx];
            int tEntrada   = temposEntrada[releIdx];
            String horaAtivacao   = rele[releIdx].horaAtivacao;
            String horaDesativacao= rele[releIdx].horaDesativacao;

            // Agora chamamos a sua função unificada paginaPrincipal(...)
            // que deve ter a assinatura que inclua estes parâmetros.
            request->send(200, "text/html",
                paginaPrincipal(qtdRele, releIdx + 1, tempoPulso, tEntrada, horaAtivacao, horaDesativacao));
        }
        else
        {
            request->send(400, "text/plain", "Parâmetro 'rele' ausente.");
        } });

    // Salvar config (antes era usado pelo form da página separada, mas agora
    // ele continua funcionando normalmente, pois a mesma <form> está dentro da paginaPrincipal).
    server.on("/salvarconfig", HTTP_POST, [](AsyncWebServerRequest *request)
              {
    if (request->hasParam("rele", true))
    {
        int releIdx = request->getParam("rele", true)->value().toInt() - 1;
        if (releIdx >= 0 && releIdx < qtdRele)
        {
            // tempoPulso
            if (request->hasParam("tempoPulso", true)) {
                int tPulso = request->getParam("tempoPulso", true)->value().toInt();
                temposPulso[releIdx] = tPulso;
            }

            // tempoEntrada
            if (request->hasParam("tempoEntrada", true)) {
                int tEntrada = request->getParam("tempoEntrada", true)->value().toInt();
                temposEntrada[releIdx] = tEntrada;
            }

            // modoRele (se você tiver)
            if (request->hasParam("modoRele", true)) {
                int modo = request->getParam("modoRele", true)->value().toInt();
                modoAcionamento[releIdx] = modo;
            }

            // Agora, SALVAMOS O NOME
            if (request->hasParam("nomeRele", true)) {
                String nome = request->getParam("nomeRele", true)->value();
                rele[releIdx].nome = nome;
            }

            // horaAtivacao / horaDesativacao (opcional)
            if (request->hasParam("horaAtivacao", true)) {
                rele[releIdx].horaAtivacao = request->getParam("horaAtivacao", true)->value();
            }
            if (request->hasParam("horaDesativacao", true)) {
                rele[releIdx].horaDesativacao = request->getParam("horaDesativacao", true)->value();
            }
            
            salvarConfiguracoes();
            request->redirect("/");
        }
        else
        {
            request->send(400, "text/plain", "Relé inválido.");
        }
    }
    else
    {
        request->send(400, "text/plain", "Parâmetro 'rele' ausente.");
    } });
    ;

    // Inicia o servidor
    server.begin();
}

void loadWiFiConfig()
{
    if (SPIFFS.exists("/wifi_config.txt"))
    {
        File wifiConfig = SPIFFS.open("/wifi_config.txt", FILE_READ);
        if (wifiConfig)
        {
            String ssid = wifiConfig.readStringUntil('\n');
            String password = wifiConfig.readStringUntil('\n');
            ssid.trim();
            password.trim();
            wifiConfig.close();

            if (!ssid.isEmpty())
            {
                Serial.println("Carregando configuração Wi-Fi da SPIFFS...");
                WiFi.begin(ssid.c_str(), password.c_str());

                unsigned long startTime = millis();
                while (WiFi.status() != WL_CONNECTED && millis() - startTime < 15000)
                {
                    delay(500);
                    Serial.print(".");
                }

                if (WiFi.status() == WL_CONNECTED)
                {
                    Serial.println("\nConectado ao Wi-Fi com sucesso!");
                    Serial.println("IP: " + WiFi.localIP().toString());
                }
                else
                {
                    Serial.println("\nFalha ao conectar ao Wi-Fi com as configurações salvas.");
                }
            }
        }
    }
    else
    {
        Serial.println("Nenhuma configuração Wi-Fi salva encontrada.");
    }
}

String getHoraAtual()
{
    return timeClient.getFormattedTime(); // Ajuste para o método que você usa para obter o horário
}

void controlarRelesWebServer()
{
    String horaAtual = getHoraAtual();
    for (int i = 0; i < 5; i++)
    {
        if (!rele[i].horaAtivacao.isEmpty() && horaAtual == rele[i].horaAtivacao)
        {
            digitalWrite(rele[i].pino, HIGH);
            rele[i].status = 1;
            Serial.printf("Relé %d ativado pelo Web Server.\n", i + 1);
        }
        if (!rele[i].horaDesativacao.isEmpty() && horaAtual == rele[i].horaDesativacao)
        {
            digitalWrite(rele[i].pino, LOW);
            rele[i].status = 0;
            Serial.printf("Relé %d desativado pelo Web Server.\n", i + 1);
        }
    }
}

void DisplayInit()
{

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    { // Address 0x3D for 128x64
        Serial.println(F("SSD1306 allocation failed"));
        for (;;)
            ;
    }
    delay(2000);
    display.clearDisplay();

    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 10);
    // Display static text
    // display.print("Date: " + date + " - Time: " + hour);
    display.display();
}

void taskEntradaNaSaida(void *pvParameters) {
  for (;;) {
    entradaNaSaida();      // A função que você quer rodando no segundo núcleo
    vTaskDelay(10 / portTICK_PERIOD_MS);  
    // Ajuste esse delay conforme a frequência desejada de chamadas;
    // se quiser rodar sem parar, pode usar vTaskDelay(1) ou mesmo sem delay,
    // mas é recomendado ao menos um pequeno delay pra não travar a CPU.
  }
}

void setup()
{
    Serial.begin(115200);
    pinMode(BTN_CONFIG, INPUT);
    digitalWrite(25, HIGH);

    uint64_t chipid = ESP.getEfuseMac();
    sprintf(espUniqueId, "%04X%08X", (uint16_t)(chipid >> 32), (uint32_t)chipid);

    // Construir o caminho do banco de dados usando o ID único
    databasePath = "/IdsESP/" + String(espUniqueId);

    // Inicializa os pinos dos relés e botões
    for (int i = 0; i < qtdRele; i++)
    {
        pinMode(rele[i].pino, OUTPUT);
        pinMode(rele[i].btn, INPUT);
    }

    // Inicializar o SPIFFS
    if (!SPIFFS.begin())
    {
        Serial.println("Falha ao montar o sistema de arquivos!");
        return;
    }

    // Carregar configuração de Wi-Fi
    loadWiFiConfig();

    // Verificar conexão com Ethernet ou Wi-Fi
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

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    { // Inicializar o display
        Serial.println(F("Falha na inicialização do display SSD1306!"));
        for (;;)
            ; // Trava o sistema se não inicializar
    }
    display.clearDisplay();
    display.display();

    xTaskCreatePinnedToCore(
      taskEntradaNaSaida,   // Função que implementa a tarefa
      "TarefaEnSa",         // Nome amigável da tarefa (p/ debug)
      2048,                 // Tamanho da stack (em palavras) - ajuste se necessário
      NULL,                 // Parâmetro passado para a tarefa (se precisar)
      1,                    // Prioridade da tarefa (0 a configMAX_PRIORITIES-1)
      NULL,                 // (opcional) handle da tarefa
      1                     // Core 1 = segundo núcleo
  );

    // Se Ethernet falhar, verificar Wi-Fi
    if (!ethernetConnected)
    {
        if (WiFi.status() != WL_CONNECTED)
        {
            Serial.println("Não conectado ao Wi-Fi. Iniciando modo de configuração Wi-Fi...");
            startWiFiManager(); // Entra no modo de configuração Wi-Fi
        }
        else
        {
            Serial.println("Wi-Fi conectado com as configurações salvas.");
        }
    }

    rele[0].nome = "Relé 1";
    rele[1].nome = "Relé 2";
    rele[2].nome = "Relé 3";
    rele[3].nome = "Relé 4";
    rele[4].nome = "Relé 5";

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0); // Posição inicial
    display.println("Wi-Fi conectado!");
    display.println("IP:");
    display.println(WiFi.localIP().toString()); // Converte IP para string e exibe
    display.display();

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

    // Configuração do Web Server
    configurarWebServer();
}

void loop()
{
    leRele();
    entradaNaSaida();
    controlarRelesWebServer();

    static unsigned long lastStreamAttempt = 0;
    if (millis() - lastStreamAttempt > 200) {
        lastStreamAttempt = millis();
        tiposBots(); // Faz a leitura e parse do Realtime DB
    }
    static unsigned long lastHeartBeat = 0;
    const unsigned long heartBeatInterval = 60000; // 1 minuto


  
    // Atualizar o horário NTP
    static unsigned long previousMillis = 0;
    const long interval = 1000;
    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= interval)
    {
        previousMillis = currentMillis;
        timeClient.update();
        horarioAtual = timeClient.getFormattedTime();
    }

    verificarHorarioReles(horaAtivacao, horaDesativacao, rele[0].pino, 1);
    verificarHorarioReles(horaAtivacao2, horaDesativacao2, rele[1].pino, 2);
    verificarHorarioReles(horaAtivacao3, horaDesativacao3, rele[2].pino, 3);
    verificarHorarioReles(horaAtivacao4, horaDesativacao4, rele[3].pino, 4);
    verificarHorarioReles(horaAtivacao5, horaDesativacao5, rele[4].pino, 5);

    if (switchAtual.ativo)
    {
        unsigned long tempoDecorrido = millis() - switchAtual.inicio;
        int releIdx = switchAtual.releIdx;

        // Atualiza o status enquanto o relé está ativo
        rele[releIdx].status = 1;

        if (tempoDecorrido >= temposPulso[releIdx])
        {
            digitalWrite(rele[releIdx].pino, LOW);
            switchAtual.ativo = false;
            rele[releIdx].status = 0; // Atualiza o status quando desativa
        }
    }

    delay(100); // Pequeno delay para evitar loops excessivos

}

