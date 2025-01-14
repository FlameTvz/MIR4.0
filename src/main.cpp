#include "vars.h"
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <WiFi.h>

const byte DNS_PORT = 53;
DNSServer dnsServer;
AsyncWebServer server(80); // Porta padrão HTTP
struct SwitchTask
{
    bool ativo = false;
    unsigned long inicio = 0;
    int releIdx = -1;
};
SwitchTask switchAtual;
/**
 * Salva um valor numérico em um arquivo no sistema de arquivos SPIFFS.
 *
 * @param caminho Caminho do arquivo a ser salvo.
 * @param dados Valor numérico a ser salvo.
 */
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

struct RelePulse
{
    bool ativo = false;       // Indica se o pulso está ativo
    unsigned long inicio = 0; // Momento de início do pulso
    int releIdx = -1;         // Índice do relé associado
};

RelePulse pulseAtual;

/**
 * @brief Retorna o tipo do token como string.
 *
 * @param[in] info Informa es sobre o token.
 *
 * @return String com o tipo do token.
 */
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

/**
 * @brief Retorna o status do token como string.
 *
 * @param[in] info Informa es sobre o token.
 *
 * @return String com o status do token.
 */
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

/**
 * Callback function to handle and print token status information.
 *
 * This function takes a TokenInfo object and prints the token type and status
 * using the Serial interface. If the token status is `token_status_error`,
 * it additionally prints the error message associated with the token.
 *
 * @param info TokenInfo object containing details about the token type, status,
 *             and any associated error message.
 */
void tokenStatusCallback(TokenInfo info)
{
    Serial.printf("Token Info: type = %s, status = %s\n",
                  getTokenType(info).c_str(), getTokenStatus(info).c_str());

    if (info.status == token_status_error)
    {
        Serial.printf("Token error: %s\n", info.error.message.c_str());
    }
}

/**
 * Registra um evento no Firestore.
 *
 * @param releStr String que identifica o rel  (ex. "Rele 1")
 * @param evento String que descreve o evento (ex. "ligado")
 */
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

/**
 * @brief Leitura dos estados dos bot es de controle dos rel s
 *
 * @details
 * Essa fun o l  os estados dos bot es de controle dos rel s
 * e os guarda no array leitura.
 * @see leitura
 */
void leRele()
{
    bool valor;
    for (int i = 0; i < qtdRele; i++)
    {
        leitura[i] = digitalRead(rele[i].btn);
        // Serial.println(leitura[i]);
    }
}

/**
 * Verifica se algum botão de relé está pressionado e, se sim, ativa o respectivo
 * relé e registra o evento no Firestore.
 *
 * Também incrementa o contador de eventos de cada relé e salva o valor no
 * arquivo respectivo no SPIFFS.
 */
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

/**
 * @brief Alterna o estado de um pino de sa da do rel
 *
 * @param pino N mero do pino a ser alterado
 *
 * @details
 *    Se o pino estiver em LOW, ele ser  alterado para HIGH e vice-versa.
 *    Um delay de 200ms  adicionado para evitar ativa es m ltiplas r pidas.
 *    Utilizado para controlar os rel s via HTTP.
 */
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

/**
 * Extrai o valor de uma chave JSON e ajusta o formato para HH:MM:SS se necessário.
 *
 * @param dataPath Caminho do dado recebido via Stream do Firebase.
 * @param jsonData Conteúdo do JSON recebido.
 * @param jsonKey Chave do valor que deve ser extraído.
 * @param pathEsperado Caminho esperado que deve ser igual a dataPath.
 *
 * @return O valor extraído e formatado, ou uma string vazia se o caminho ou chave forem inválidos.
 */
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

/**
 * Envia um HeartBeat (sinal de vida) para o Firebase Realtime Database.
 *
 * O HeartBeat é um sinal de vida que indica que o dispositivo ainda está ativo.
 * Ele é enviado periodicamente para o Firebase e pode ser usado para monitorar
 * a presença do dispositivo na rede.
 *
 * @return Nenhum valor de retorno.
 */
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

/**
 * Atualiza o estado do rele especificado no Realtime Database.
 *
 * @param rele Número do relé a ser atualizado (1 ou 2).
 * @param estado Novo estado do relé (0 = desativado, 1 = ativado).
 *
 * @return Nenhum valor de retorno.
 */
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
/******  bdf903f3-cd84-4ccf-9f8f-29a6fd707fdb  *******/
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

/**
 *
 * This function compares the current time with specified activation and deactivation times.
 * If the current time is within a 2-second tolerance of the activation time, the relay is activated.
 * If the current time is within a 2-second tolerance of the deactivation time, the relay is deactivated.
 * It logs events for both activation and deactivation.
 *
 * @param ativacao The activation time in the format "HH:MM:SS".
 * @param desativacao The deactivation time in the format "HH:MM:SS".
 * @param pino The pin number associated with the relay.
 * @param releNum The relay number for identification in logging.
 */
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

/**
 * Inicia o modo Access Point no ESP32 e configura o servidor HTTP para
 * fornecer uma página de configuração Wi-Fi.
 *
 * A página de configuração é servida na porta 80 e fornece inputs para o
 * nome da rede (SSID) e a senha. Após a submissão da página, o ESP32
 * tenta se conectar à rede especificada e salva a configuração Wi-Fi na
 * SPIFFS em um arquivo chamado "wifi_config.txt".
 *
 * Caso a conexão seja bem-sucedida, o ESP32 reinicia automaticamente e
 * tenta se conectar à rede novamente. Caso contrário, a página de
 * configuração é novamente servida com uma mensagem de erro.
 */
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

/**
 * Gera a página principal do controle de relés.
 *
 * A página inclui controles para os 5 relés e um formulário para configurar
 * horários de ativação e desativação.
 *
 * @return String com o conteúdo HTML da página.
 */
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
    <h2>Configurar Tempo do Pulso</h2>
<form action="/configurartempo" method="POST">
    <label for="tempo">Tempo do Pulso (ms):</label>
    <input type="number" id="tempo" name="tempo" min="1" required>
    <button type="submit">Atualizar Tempo</button>
</form>
    <script>
        // Atualiza o status e tempo dos relés a cada 1 segundo
        setInterval(() => {
            fetch('/status')
                .then(response => response.json())
                .then(data => {
                    for (let i = 0; i < 5; i++) {
                        document.getElementById(`rele-status-${i}`).innerText = data.reles[i].status ? "Ligado" : "Desligado";
                        document.getElementById(`rele-pulso-${i}`).innerText = `${data.reles[i].tempoPulso} ms`;
                        document.getElementById(`rele-entrada-${i}`).innerText = `${data.reles[i].tempoEntrada} ms`;
                    }
                });
        }, 1000);

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

    // Manter os controles já existentes
    String controles = "";
    for (int i = 0; i < 5; i++)
    {
        controles += "<div class='rele'>";
        controles += "<h2>Relé " + String(i + 1) + "</h2>";
        controles += "<p>Estado: " + String(rele[i].status ? "Ligado" : "Desligado") + "</p>";
        controles += "<p>Tempo do Pulso: <span id='rele-pulso-" + String(i) + "'>0 ms</span></p>";
        controles += "<p>Tempo da Entrada: <span id='rele-entrada-" + String(i) + "'>0 ms</span></p>";
        controles += "<button onclick='fetch(`/controle?rele=" + String(i + 1) + "&acao=ligar`)'>Ligar</button>";
        controles += "<button onclick='fetch(`/controle?rele=" + String(i + 1) + "&acao=desligar`)'>Desligar</button>";
        controles += "<button onclick='fetch(`/switch?rele=" + String(i + 1) + "`).then(response => response.text())'>Switch</button>";
        controles += "</div>";
    }

    html.replace("%CONTROLES%", controles);
    html.replace("%STATUS%", controles);

    return html;
}

/**
 * Configures the web server routes for handling HTTP requests.
 *
 * Sets up the following routes:
 * - "/" (GET): Serves the main relay control page.
 * - "/controle" (GET): Controls relay states based on query parameters "rele" and "acao".
 *   - "rele" specifies the relay number (1 to 5).
 *   - "acao" specifies the action ("ligar" or "desligar").
 *   Responds with "OK" if successful, or an error message if parameters are invalid.
 * - "/configurar" (POST): Configures activation and deactivation times for relays based on the
 *   provided parameters "rele", "horaAtivacao", and "horaDesativacao".
 *   Responds with confirmation if successful, or an error message if parameters are invalid.
 */
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
                request->redirect("/");
            } else {
                request->send(400, "text/plain", "Relé inválido");
            }
        } else {
            request->send(400, "text/plain", "Parâmetros inválidos");
        } });

    server.on("/switch", HTTP_GET, [](AsyncWebServerRequest *request)
              {
    if (request->hasParam("rele")) {
        int releIdx = request->getParam("rele")->value().toInt() - 1; // Índice do relé (0 a 4)
        if (releIdx >= 0 && releIdx < qtdRele) {
            if (!switchAtual.ativo) {
                digitalWrite(rele[releIdx].pino, HIGH); // Liga o relé
                switchAtual.ativo = true;
                switchAtual.inicio = millis(); // Marca o início do pulso
                switchAtual.releIdx = releIdx;
                request->send(200, "text/plain", "Pulso iniciado para o relé " + String(releIdx + 1));
            } else {
                request->send(400, "text/plain", "Outro pulso está em andamento. Tente novamente.");
            }
        } else {
            request->send(400, "text/plain", "Relé inválido");
        }
    } else {
        request->send(400, "text/plain", "Parâmetro 'rele' ausente");
    } });

    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request)
              {
    String response = "{ \"reles\": [";
    for (int i = 0; i < 5; i++) {
        response += "{";
        response += "\"status\": " + String(rele[i].status) + ",";
        response += "\"tempoPulso\": " + String(tempoClick) + ",";
        response += "\"tempoEntrada\": " + String(millis() - pulseAtual.inicio);

        response += "}";
        if (i < 4) response += ",";
    }
    response += "] }";
    request->send(200, "application/json", response); });

    server.on("/configurartempo", HTTP_POST, [](AsyncWebServerRequest *request)
              {
    if (request->hasParam("tempo", true)) {
        String tempoStr = request->getParam("tempo", true)->value();
        int novoTempo = tempoStr.toInt();
        if (novoTempo > 0) {
            tempoClick = novoTempo;
            Serial.println("Tempo do pulso atualizado para: " + String(tempoClick) + " ms");
            request->redirect("/"); // Redireciona de volta para o índice
        } else {
            request->send(400, "text/plain", "Valor inválido para o tempo do pulso");
        }
    } else {
        request->send(400, "text/plain", "Parâmetro 'tempo' ausente");
    } });

    server.begin();
}

/**
 * Carrega a configuração Wi-Fi salva na SPIFFS em um arquivo chamado
 * "wifi_config.txt". Se o arquivo existir, tenta se conectar à rede
 * especificada. Se a conexão for bem-sucedida, imprime o IP atribuído.
 * Se a conexão falhar ou o arquivo não existir, imprime uma mensagem de
 * erro.
 */
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

/**
 * Retorna a hora atual formatada como uma string no padrão HH:MM:SS.
 *
 * @return String com a hora atual formatada.
 */
String getHoraAtual()
{
    return timeClient.getFormattedTime(); // Ajuste para o método que você usa para obter o horário
}

// Loop para controle de horários configurados no Web Server

/**
 * Verifica se a hora atual é igual às horas configuradas para cada um dos
 * 5 relés e, se sim, ativa ou desativa o relé de acordo com a configuração.
 *
 * A função acessa o horário atual com o método getHoraAtual() e itera sobre
 * os objetos do vetor "rele". Para cada relé, verifica se a hora atual é igual
 * às horas de ativação ou desativação configuradas. Caso seja, ativa ou desativa
 * o relé de acordo com a configuração.
 *
 * A função é chamada no loop principal do programa, logo após a configuração
 * do Web Server.
 */
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

/**
 * Configures and initializes the hardware and network settings for the system.
 *
 * - Initializes the serial communication and configures the button and relay pins.
 * - Retrieves the unique ESP32 chip ID and constructs the database path.
 * - Mounts the SPIFFS filesystem and loads the Wi-Fi configuration.
 * - Attempts to connect to Ethernet and, if unsuccessful, switches to Wi-Fi.
 * - Initializes Firebase with authentication and registers the ESP32 ID in Firestore.
 * - Starts a stream in the Firebase Realtime Database for data synchronization.
 * - Sets up the web server for handling HTTP requests.
 */
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

    if (pulseAtual.ativo)
    {
        if (millis() - pulseAtual.inicio >= 200)
        {                                                     // Após 200ms
            digitalWrite(rele[pulseAtual.releIdx].pino, LOW); // Desliga o relé
            pulseAtual.ativo = false;                         // Finaliza o estado do pulso
            registrarEvento(("Relé " + String(pulseAtual.releIdx + 1)).c_str(), "Ativado e Desativado");
        }
    }
    if (switchAtual.ativo)
    {
        // Verifica se o tempo do pulso expirou
        if (millis() - switchAtual.inicio >= tempoClick)
        {
            digitalWrite(rele[switchAtual.releIdx].pino, LOW); // Desliga o relé
            switchAtual.ativo = false;                         // Finaliza o pulso
            Serial.println("Pulso finalizado para o relé " + String(switchAtual.releIdx + 1));
        }
    }

    delay(100); // Pequeno delay para evitar loops excessivos
}

// tempo do pulso, tempo do pulso da entrada, mostrar status do rele