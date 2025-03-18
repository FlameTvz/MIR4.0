void salvarConfiguracoes()
{
    // Cria um documento JSON e preenche com os valores atuais
    StaticJsonDocument<512> doc;

    JsonArray arr = doc.createNestedArray("reles");
    for (int i = 0; i < qtdRele; i++)
    {
        JsonObject obj = arr.createNestedObject();
        obj["tempoPulso"] = temposPulso[i];
        obj["tempoEntrada"] = temposEntrada[i];    // (campo antigo)
        obj["tempoDebouncing"] = temposEntrada[i]; // (campo novo adicionado!)
        obj["horaAtivacao"] = rele[i].horaAtivacao;
        obj["horaDesativacao"] = rele[i].horaDesativacao;
        // se tiver nome: obj["nome"] = rele[i].nome;
    }

    // Abre/cria o arquivo e escreve
    File file = SPIFFS.open("/configRele.json", FILE_WRITE);
    if (file)
    {
        serializeJson(doc, file);
        file.close();
        Serial.println("Configurações salvas em /configRele.json");
    }
    else
    {
        Serial.println("Falha ao abrir /configRele.json para escrita");
    }
}

void carregarConfiguracoes()
{
    if (!SPIFFS.exists("/configRele.json"))
    {
        Serial.println("Nenhum arquivo /configRele.json encontrado, usando valores padrão.");
        return;
    }
    File file = SPIFFS.open("/configRele.json", FILE_READ);
    if (!file)
    {
        Serial.println("Falha ao abrir /configRele.json para leitura");
        return;
    }

    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, file);
    if (error)
    {
        Serial.println("Erro ao fazer parse do JSON de config: ");
        Serial.println(error.f_str());
        file.close();
        return;
    }

    file.close();
    JsonArray arr = doc["reles"];
    if (!arr.isNull())
    {
        for (int i = 0; i < arr.size() && i < qtdRele; i++)
        {
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