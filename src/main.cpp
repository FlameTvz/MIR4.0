#include "vars.h"
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <WiFi.h>

const byte DNS_PORT = 53;
DNSServer dnsServer;
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
            atualizarEstadoRele(releNum, true);
            registrarEvento(("Relé " + String(releNum)).c_str(), "Ativado");
        }
        if (abs(horaAtualSeg - horaDesativacaoSeg) <= tolerancia)
        {
            digitalWrite(pino, LOW);
            Serial.printf("Relé %d desativado!\n", releNum);
            atualizarEstadoRele(releNum, false);
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

String paginaPrincipal()
{
    String html = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-br">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Controle de Relés</title>
    <style>
        body { font-family: Arial, sans-serif; text-align: center; margin: 0; padding: 0; background-color: #f4f4f9; }
        h1 { color: #333; }
        .rele { margin: 10px 0; padding: 10px; border: 1px solid #ccc; border-radius: 5px; background-color: #fff; display: inline-block; }
        button { padding: 10px 20px; margin: 5px; background-color: #007BFF; color: white; border: none; border-radius: 5px; cursor: pointer; }
        button:hover { background-color: #0056b3; }
    </style>

</head>
<body>
    <h1>Controle de Relés</h1>
    <div>
        <!-- Controles para os relés -->
        %CONTROLES%
    </div>
    <h2>Configurar Horários</h2>
    <form action="/configurar" method="POST">
        <label for="rele">Relé:</label>
        <input type="number" id="rele" name="rele" min="1" max="5" required><br>
        <label for="horaAtivacao">Hora de Ativação (HH:MM:SS):</label>
        <input type="text" id="horaAtivacao" name="horaAtivacao" oninput="formatTimeInput(this)" placeholder="HH:MM:SS" required><br><br>

        <label for="horaDesativacao">Hora de Desativação (HH:MM:SS):</label>
        <input type="text" id="horaDesativacao" name="horaDesativacao" oninput="formatTimeInput(this)" placeholder="HH:MM:SS" required><br><br>

        <button type="submit">Configurar</button>
    </form>
    <script>
        function controleRele(rele, acao) {
            fetch(`/controle?rele=${rele}&acao=${acao}`)
                .then(response => response.text())
                .then(data => alert(data));
        }
        // Função para aplicar máscara no campo HH:MM:SS
        function formatTimeInput(input) {
            let value = input.value.replace(/[^0-9]/g, ""); // Remove caracteres não numéricos
            if (value.length > 6) value = value.slice(0, 6); // Limita a 6 dígitos

            if (value.length > 4) {
                input.value = value.slice(0, 2) + ":" + value.slice(2, 4) + ":" + value.slice(4, 6);
            } else if (value.length > 2) {
                input.value = value.slice(0, 2) + ":" + value.slice(2, 4);
            } else {
                input.value = value;
            }
        }
    </script>
</body>
</html>
)rawliteral";

    String controles = "";
    for (int i = 0; i < 5; i++)
    {
        controles += "<div class='rele'>";
        controles += "<h2>Relé " + String(i + 1) + "</h2>";
        controles += "<p>Estado: " + String(rele[i].status ? "Ligado" : "Desligado") + "</p>";
        controles += "<button onclick='controleRele(" + String(i + 1) + ", \"ligar\")'>Ligar</button>";
        controles += "<button onclick='controleRele(" + String(i + 1) + ", \"desligar\")'>Desligar</button>";
        controles += "</div>";
    }
    html.replace("%CONTROLES%", controles);
    return html;
}

void configurarWebServer()
{
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "text/html", paginaPrincipal()); });

    server.on("/controle", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (request->hasParam("rele") && request->hasParam("acao")) {
            int releIdx = request->getParam("rele")->value().toInt() - 1;
            String acao = request->getParam("acao")->value();
            if (releIdx >= 0 && releIdx < 5) {
                if (acao == "ligar") {
                    digitalWrite(rele[releIdx].pino, HIGH);
                    rele[releIdx].status = 1;
                } else if (acao == "desligar") {
                    digitalWrite(rele[releIdx].pino, LOW);
                    rele[releIdx].status = 0;
                }
                request->send(200, "text/plain", "OK");
            } else {
                request->send(400, "text/plain", "Relé inválido");
            }
        } else {
            request->send(400, "text/plain", "Parâmetros inválidos");
        } });

    server.on("/configurar", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        if (request->hasParam("rele", true) && request->hasParam("horaAtivacao", true) && request->hasParam("horaDesativacao", true)) {
            int releIdx = request->getParam("rele", true)->value().toInt() - 1;
            if (releIdx >= 0 && releIdx < 5) {
                rele[releIdx].horaAtivacao = request->getParam("horaAtivacao", true)->value();
                rele[releIdx].horaDesativacao = request->getParam("horaDesativacao", true)->value();
                request->send(200, "text/plain", "Horários configurados no Web Server");
            } else {
                request->send(400, "text/plain", "Relé inválido");
            }
        } else {
            request->send(400, "text/plain", "Parâmetros inválidos");
        } });

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

// Loop para controle de horários configurados no Web Server

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

    Serial.println("Setup concluído.");
}

void loop()
{
    controlarRelesWebServer();
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

    verificarHorarioReles(horaAtivacao, horaDesativacao, rele[0].pino, 1);
    verificarHorarioReles(horaAtivacao2, horaDesativacao2, rele[1].pino, 2);
    verificarHorarioReles(horaAtivacao3, horaDesativacao3, rele[2].pino, 3);
    verificarHorarioReles(horaAtivacao4, horaDesativacao4, rele[3].pino, 4);
    verificarHorarioReles(horaAtivacao5, horaDesativacao5, rele[4].pino, 5);

    delay(100); // Pequeno delay para evitar loops excessivos
}
