

#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <WiFi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH1106.h>
#include <ArduinoJson.h>
#include <Wire.h>

#define SDA_PIN 4 // Alterar se estiver usando outros pinos
#define SCL_PIN 22

// Adafruit_SH1106 display(-1);

DNSServer dnsServer;
const byte DNS_PORT = 53;

AsyncWebServer server(80); // Porta padrão HTTP

#include "vars.h"
#include "functions.h"
#include "display.h"
#include "FFS.h"
#include "webserver.h"



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
int teste = 0;
void tokenStatusCallback(TokenInfo info)
{
    Serial.printf("Token Info: type = %s, status = %s\n",
                  getTokenType(info).c_str(), getTokenStatus(info).c_str());

    if (info.status == token_status_error)
    {
        Serial.printf("Token error: %s\n", info.error.message.c_str());
        teste++;
        if (teste == 20)
        {
            ESP.restart();
        }
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
        }
        if (abs(horaAtualSeg - horaDesativacaoSeg) <= tolerancia)
        {
            digitalWrite(pino, LOW);
            Serial.printf("Relé %d desativado!\n", releNum);
        }
    }
}

void taskEntradaNaSaida(void *pvParameters)
{
    for (;;)
    {
        entradaNaSaida(); // A função que você quer rodando no segundo núcleo
        vTaskDelay(10 / portTICK_PERIOD_MS);
        // Ajuste esse delay conforme a frequência desejada de chamadas;
        // se quiser rodar sem parar, pode usar vTaskDelay(1) ou mesmo sem delay,
        // mas é recomendado ao menos um pequeno delay pra não travar a CPU.
    }
}

void verificarToken()
{
    if (Firebase.ready())
    {
        if (config.signer.tokens.status == token_status_error)
        {
            Serial.println("Renovando token...");
            Firebase.refreshToken(&config); // Renovar token manualmente
        }
    }
    else
    {
        Serial.println("Firebase não está pronto. Verificando conexão...");
    }
}
// String serialNumber = getESP32Serial();
    // Serial.println("Serial do ESP32: " + serialNumber);
void setup()
{
    Serial.begin(115200);
    pinMode(BTN_CONFIG, INPUT);
    digitalWrite(25, HIGH);
    pinMode(BTN_PIN, INPUT);

    Wire.begin(SDA_PIN, SCL_PIN);

    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
    {
        Serial.println(F("Falha ao inicializar o display OLED"));
        for (;;)
            ;
    }

    Serial.println("Iniciando display...");
    display.begin(SH1106_SWITCHCAPVCC, 0x3C); // Endereço padrão 0x3C
    display.clearDisplay();
    display.display();

    // Obter o serial único do ESP32
    // String serialNumber = getESP32Serial();
    // Serial.println("Serial do ESP32: " + serialNumber);

    // QRCode qrcode;
    // uint8_t qrcodeData[qrcode_getBufferSize(3)];
    // qrcode_initText(&qrcode, qrcodeData, 3, ECC_LOW, serialNumber.c_str());

    // drawQRCode(qrcode);

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
    else
    {
        carregarConfiguracoes();
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



    xTaskCreatePinnedToCore(
        taskEntradaNaSaida, // Função que implementa a tarefa
        "TarefaEnSa",       // Nome amigável da tarefa (p/ debug)
        2048,               // Tamanho da stack (em palavras) - ajuste se necessário
        NULL,               // Parâmetro passado para a tarefa (se precisar)
        1,                  // Prioridade da tarefa (0 a configMAX_PRIORITIES-1)
        NULL,               // (opcional) handle da tarefa
        1                   // Core 1 = segundo núcleo
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

    // Configuração do Firebase
    config.api_key = API_KEY;
    config.database_url = DATABASE_URL; // Adicione esta linha
    auth.user.email = EMAIL;
    auth.user.password = EMAIL_PASSWORD;
    config.token_status_callback = tokenStatusCallback;

    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP("MeuAP", "12345678");

    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);
    FirebaseJson content;
    content.set("fields/espId/stringValue", espUniqueId);

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
    carregarConfiguracoesDoFirebase();

    String firebaseUserPath = "/IdsESP/" + String(espUniqueId) + "/lastUser";

    if (Firebase.RTDB.getString(&firebaseData, firebaseUserPath))
    {
        String userID = firebaseData.stringData();
        Serial.println("👤 Último usuário autenticado no app: " + userID);

        registrarESPNoUsuario(userID);

        // Verifica se o usuário tem permissão para acessar este ESP
        verificarPermissaoUsuario(userID);
    }
    else
    {
        Serial.println("❌ Erro ao obter userID do Firebase: " + firebaseData.errorReason());
    }

    // Configuração do Web Server
    configurarWebServer();

    // if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
    // {
    //     Serial.println(F("Falha ao inicializar o display OLED"));
    //     for (;;)
    //         ;
    // }

    // display.clearDisplay();
    // display.setTextSize(1);
    // display.setTextColor(WHITE);
    // display.setCursor(10, 10);
    // display.println("Gerando QR Code...");
    // display.display();

    // // Obter o serial único do ESP32
    // String serialNumber = getESP32Serial();
    // Serial.println("Serial do ESP32: " + serialNumber);

    // // Criar QR Code com o serial único do ESP32
    // QRCode qrcode;
    // uint8_t qrcodeData[qrcode_getBufferSize(3)];
    // qrcode_initText(&qrcode, qrcodeData, 3, ECC_LOW, serialNumber.c_str());

    // drawQRCode(qrcode);

    // Serial.println(serialNumber);
}

void loop()
{
    alternateDisplay();
    leRele();
    entradaNaSaida();
    controlarRelesWebServer();

    delay(10);

    static unsigned long lastStreamAttempt = 0;
    if (millis() - lastStreamAttempt > 200)
    {
        lastStreamAttempt = millis();
        tiposBots(); // Faz a leitura e parse do Realtime DB
    }

    static unsigned long lastHeartbeat = 0;
    unsigned long now = millis();

    enviarHeartbeat();

    // Atualizar o horário NTP
    static unsigned long previousMillis = 0;
    const long interval = 1000;
    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= interval)
    {
        if (WiFi.status() == WL_CONNECTED)
        {
            timeClient.update(); // só tenta se tiver Wi-Fi
            previousMillis = currentMillis;
            horarioAtual = timeClient.getFormattedTime();
        }
        else
        {
            // pula a atualização ou faz algo offline
        }
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

    verificarToken();
    int btnState = digitalRead(BTN_PIN);

    if (btnState == HIGH) // Botão pressionado
    {
        if (!buttonPressed)
        {
            // Início da contagem do tempo de pressão
            buttonPressed = true;
            pressStartTime = millis();
        }
        else
        {
            // Verifica se passou 5 segundos
            if (millis() - pressStartTime >= HOLD_TIME)
            {
                Serial.println("Botão pressionado por 5s. Formatando SPIFFS...");
                SPIFFS.format(); // Apaga todos os arquivos da SPIFFS
                delay(500);
                ESP.restart();
            }
        }
    }
    else
    {
        // Botão solto, resetamos a flag
        buttonPressed = false;
    }
    verificarPermissaoPeriodicamente(); // Verifica a permissão do usuário a cada 30s

    if (!usuarioAutorizado)
    {
        Serial.println("⚠️ Acesso bloqueado. Nenhuma ação será permitida!");
        delay(5000); // Espera um tempo antes de verificar novamente
        return;
    }

    delay(100); // Pequeno delay para evitar loops excessivos
}
