void startWiFiManager();
void leRele();
void entradaNaSaida();
String processarHorarios(String dataPath, String jsonData, String jsonKey, String pathEsperado);
void enviarHeartbeat();
void atualizarEstadoRele(int pino, int estado, int numRele);
void reiniciarStream();
void tiposBots();
String getHoraAtual();
void carregarConfiguracoesDoFirebase();
void atualizarConfiguracoes();
void verificarPermissaoUsuario(String userID);
void verificarPermissaoPeriodicamente();
void registrarESPNoUsuario(String userID);
void salvarConfiguracoes();


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


void enviarHeartbeat()
{
    static unsigned long lastHeartbeat = 0;
    unsigned long intervalo = 60000; // 1 minuto

    if (millis() - lastHeartbeat > intervalo)
    {
        FirebaseJson json;
        json.set("espId", espUniqueId);  // Identificação do dispositivo
        json.set("timestamp", millis()); // Tempo de execução desde o início
        json.set("status", "ativo");     // Informação de status

        String path = "/heartbeat/" + String(espUniqueId);

        if (Firebase.RTDB.setJSON(&firebaseData, path.c_str(), &json))
        {
            Serial.println("Heartbeat enviado com sucesso.");
        }
        else
        {
            Serial.printf("Erro ao enviar heartbeat: %s\n", firebaseData.errorReason().c_str());
        }

        lastHeartbeat = millis();
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
    // Finaliza qualquer stream ativo
    Firebase.RTDB.endStream(&firebaseData);

    // Tenta iniciar novamente
    if (!Firebase.RTDB.beginStream(&firebaseData, databasePath.c_str()))
    {
        Serial.println("Falha ao reiniciar o stream: " + firebaseData.errorReason());
    }
    else
    {
        Serial.println("Stream reiniciado com sucesso.");
    }

    // Se não usar a flag 'streamReiniciado' para nada, pode remover completamente.
    // Caso ainda deseje marcar que "ainda não recebemos nada após a reconexão", use:
    // streamReiniciado = true;
}

void tiposBots()
{
    bool ok = Firebase.RTDB.readStream(&firebaseData);

    // 2) Se falhou e há um httpCode != 0, é erro real de conexão
    if (!ok)
    {
        int code = firebaseData.httpCode();
        if (code != 0)
        {
            Serial.printf("Stream caiu: httpCode=%d reason=%s\n", code, firebaseData.errorReason().c_str());
            reiniciarStream();
        }
    }

    // 3) Se não chegou nada novo => apenas retorna
    if (!firebaseData.streamAvailable())
    {
        return;
    }

    // Se chegou aqui, significa que temos dados novos no stream
    String jsonData = firebaseData.to<String>();
    String dataPath = firebaseData.dataPath();
    Serial.println("Dados recebidos: " + jsonData);

    if (dataPath == "/keeplive")
    {
        int startIndex = jsonData.indexOf("\"hora\":\"") + 7;
        int endIndex = jsonData.indexOf("\"", startIndex);
        String horarioAtualizado = jsonData.substring(startIndex, endIndex);

        Serial.println("Hora atualizada recebida: " + horarioAtualizado);
    }

    for (int i = 0; i < 5; i++)
    {
        String pathRele = "/rele" + String(i + 1);

        if (dataPath == pathRele)
        {
            if (jsonData.indexOf("\"status\":true") != -1)
            {
                digitalWrite(rele[i].pino, HIGH);
            }
            else if (jsonData.indexOf("\"status\":false") != -1)
            {
                digitalWrite(rele[i].pino, LOW);
            }

            // Escutando tempoPulso no Firebase (Somente para switch)
            if (jsonData.indexOf("\"tempoPulso\":") != -1)
            {
                int startIndex = jsonData.indexOf("\"tempoPulso\":") + 13;
                int endIndex = jsonData.indexOf(",", startIndex);
                if (endIndex == -1)
                    endIndex = jsonData.indexOf("}", startIndex);

                String tempoStr = jsonData.substring(startIndex, endIndex);
                tempoStr.trim();
                int novoTempoPulso = tempoStr.toInt();

                if (novoTempoPulso > 0)
                {
                    temposPulso[i] = novoTempoPulso;
                    Serial.printf("Tempo de pulso do Relé %d atualizado para %d ms\n", i + 1, novoTempoPulso);
                }
            }

            // Escutando tempoDebouncing no Firebase (Somente para botões físicos)
            if (jsonData.indexOf("\"tempoDebouncing\":") != -1)
            {
                int startIndex = jsonData.indexOf("\"tempoDebouncing\":") + 18;
                int endIndex = jsonData.indexOf(",", startIndex);
                if (endIndex == -1)
                    endIndex = jsonData.indexOf("}", startIndex);

                String tempoStr = jsonData.substring(startIndex, endIndex);
                tempoStr.trim();
                int novoTempoDebouncing = tempoStr.toInt();

                if (novoTempoDebouncing > 0)
                {
                    temposEntrada[i] = novoTempoDebouncing;
                    Serial.printf("Tempo de debouncing do Relé %d atualizado para %d ms\n", i + 1, novoTempoDebouncing);

                    // **SALVA NO SPIFFS** para não perder ao reiniciar:
                    salvarConfiguracoes();
                }
            }

            // Escutando modoAcionamento no Firebase e atualizando a variável correta
            if (jsonData.indexOf("\"modoAcionamento\":") != -1)
            {
                int startIndex = jsonData.indexOf("\"modoAcionamento\":") + 18;
                int endIndex = jsonData.indexOf(",", startIndex);
                if (endIndex == -1)
                    endIndex = jsonData.indexOf("}", startIndex);

                String modoStr = jsonData.substring(startIndex, endIndex);
                modoStr.trim();
                int novoModo = modoStr.toInt();

                if (novoModo >= 0 && novoModo <= 2)
                {
                    modoAcionamento[i] = novoModo;
                    Serial.printf("Modo de acionamento do Relé %d atualizado para %d\n", i + 1, novoModo);
                }
            }
        }
    }

    // ** Acionando apenas o Switch com tempoPulso!**
    for (int i = 0; i < 5; i++)
    {
        String pathSwitch = "/rele" + String(i + 1);
        if (jsonData.indexOf("\"tipoBotao\":\"switch") != -1 && dataPath == pathSwitch)
        {
            // O tempoPulso é utilizado apenas quando o ESP recebe o comando switch!
            digitalWrite(rele[i].pino, HIGH);
            delay(temposPulso[i]); //  Somente tempoPulso aplicado ao switch!
            digitalWrite(rele[i].pino, LOW);
        }
    }
}

String getHoraAtual()
{
    return timeClient.getFormattedTime(); // Ajuste para o método que você usa para obter o horário
}

void carregarConfiguracoesDoFirebase()
{
    Serial.println("🔄 Buscando configurações do Firebase...");

    for (int i = 0; i < qtdRele; i++)
    {
        // Caminho base do Realtime Database para o rele i
        String basePath = "/IdsESP/" + String(espUniqueId) + "/rele" + String(i + 1);

        // ==========================
        // 1) tempoPulso
        // ==========================
        if (Firebase.RTDB.getInt(&firebaseData, basePath + "/tempoPulso") && firebaseData.dataType() == "int")
        {
            temposPulso[i] = firebaseData.intData();
            Serial.printf("⏱️ tempoPulso[%d]: %d\n", i, temposPulso[i]);
        }

        // ==========================
        // 2) modoAcionamento
        // ==========================
        if (Firebase.RTDB.getInt(&firebaseData, basePath + "/modoAcionamento") && firebaseData.dataType() == "int")
        {
            modoAcionamento[i] = firebaseData.intData();
            Serial.printf("🔄 modoAcionamento[%d]: %d\n", i, modoAcionamento[i]);
        }

        // ==========================
        // 3) horaAtivacao
        // ==========================
        if (Firebase.RTDB.getString(&firebaseData, basePath + "/horaAtivacao") && firebaseData.dataType() == "string")
        {
            rele[i].horaAtivacao = firebaseData.stringData();
            Serial.printf("⏰ horaAtivacao[%d]: %s\n", i, rele[i].horaAtivacao.c_str());
        }

        // ==========================
        // 4) horaDesativacao
        // ==========================
        if (Firebase.RTDB.getString(&firebaseData, basePath + "/horaDesativacao") && firebaseData.dataType() == "string")
        {
            rele[i].horaDesativacao = firebaseData.stringData();
            Serial.printf("⏰ horaDesativacao[%d]: %s\n", i, rele[i].horaDesativacao.c_str());
        }

        // ==========================
        // 5) tempoDebouncing
        // ==========================
        if (Firebase.RTDB.getInt(&firebaseData, basePath + "/tempoDebouncing") && firebaseData.dataType() == "int")
        {
            temposEntrada[i] = firebaseData.intData();
            Serial.printf("🔄 tempoDebouncing[%d]: %d\n", i, temposEntrada[i]);
        }

        // ==========================
        // 6) status (true/false)
        // ==========================
        // Ajuste se seu status for int ou string. Este exemplo supõe bool.
        if (Firebase.RTDB.getBool(&firebaseData, basePath + "/status") && firebaseData.dataType() == "boolean")
        {
            bool st = firebaseData.boolData();
            digitalWrite(rele[i].pino, st ? HIGH : LOW);
            rele[i].status = st ? 1 : 0;
            Serial.printf("Relé %d => status: %s\n", i + 1, st ? "Ligado" : "Desligado");
        }
        else
        {
            Serial.printf("Falha ao ler 'status' do Rele %d: %s\n",
                          i + 1, firebaseData.errorReason().c_str());
        }
    }

    Serial.println("✅ Configurações carregadas do Firebase.");
}

void atualizarConfiguracoes()
{
    Serial.println("🔄 Atualização detectada no Firebase!");
    carregarConfiguracoesDoFirebase();
}

void verificarPermissaoUsuario(String userID)
{
    String firebasePath = "/IdsESP/" + String(espUniqueId) + "/owner";

    if (Firebase.RTDB.getString(&firebaseData, firebasePath))
    {
        String ownerID = firebaseData.stringData();
        Serial.println("👤 Dono do ESP registrado: " + ownerID);
        Serial.println("👤 Usuário tentando acessar: " + userID);

        if (ownerID == userID)
        {
            usuarioAutorizado = true;
            Serial.println("✅ Acesso permitido! O usuário pode controlar este ESP.");
        }
        else
        {
            usuarioAutorizado = false;
            Serial.println("⛔ Acesso revogado! O ESP será bloqueado para este usuário.");
        }
    }
    else
    {
        Serial.println("❌ Erro ao verificar dono do ESP: " + firebaseData.errorReason());
        usuarioAutorizado = false;
    }
}

void verificarPermissaoPeriodicamente() {
    unsigned long currentTime = millis();
    if (currentTime - lastCheckTime >= checkInterval) {
        lastCheckTime = currentTime; // Atualiza o tempo da última verificação

        // Obtém o userID salvo no Firebase
        String firebaseUserPath = "/IdsESP/" + String(espUniqueId) + "/lastUser";

        if (Firebase.RTDB.getString(&firebaseData, firebaseUserPath)) {
            String userID = firebaseData.stringData();
            verificarPermissaoUsuario(userID); // Verifica se o usuário tem permissão
        } else {
            Serial.println("❌ Erro ao obter userID do Firebase: " + firebaseData.errorReason());
        }
    }
}

void registrarESPNoUsuario(String userID)
{
    String pathOwner = "/IdsESP/" + String(espUniqueId) + "/owner";
    String pathUserESP = "/users/" + userID + "/ESPs/" + String(espUniqueId);

    // Define este ESP como pertencente ao usuário no caminho IdsESP/{espID}/owner
    if (Firebase.RTDB.setString(&firebaseData, pathOwner, userID))
    {
        Serial.println("✅ Dono do ESP registrado com sucesso: " + userID);
    }
    else
    {
        Serial.println("❌ Erro ao registrar dono do ESP: " + firebaseData.errorReason());
    }

    // Adiciona este ESP à lista do usuário em users/{userID}/ESPs/{espID}
    if (Firebase.RTDB.setBool(&firebaseData, pathUserESP, true))
    {
        Serial.println("✅ ESP vinculado ao usuário no Firebase.");
    }
    else
    {
        Serial.println("❌ Erro ao vincular ESP ao usuário: " + firebaseData.errorReason());
    }
}