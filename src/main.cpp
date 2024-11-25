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
    if (!Firebase.beginStream(firebaseData, "/IdsESP/9999"))
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
        WiFi.softAP("ESP32_AP", "9999"); // Configura um ponto de acesso WiFi no ESP32
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

void tiposBots()
{
    if (firebaseData.streamAvailable())
    {
        Serial.println("Dados atualizados recebidos para /IdsESP/9999:");
        String jsonData = firebaseData.stringData();
        Serial.println(jsonData);

        if (jsonData.indexOf("\"status\":true") != -1 && firebaseData.dataPath() == "/rele1")
        {
            Serial.println("Ativando pulso no rele1");
            digitalWrite(rele[0].pino, HIGH);
            rele1++;
            salvarDados("/rele1.txt", rele1);
        }
        else if (jsonData.indexOf("\"status\":true") != -1 && firebaseData.dataPath() == "/rele2")
        {
            Serial.println("Ativando pulso no rele2");
            digitalWrite(rele[1].pino, HIGH);
            rele2++;
            salvarDados("/rele2.txt", rele2);
        }

        else if (jsonData.indexOf("\"status\":true") != -1 && firebaseData.dataPath() == "/rele3")
        {
            Serial.println("Ativando rele");
            digitalWrite(rele[2].pino, HIGH);
            rele3++;
            salvarDados("/rele3.txt", rele3);
        }
        else if (jsonData.indexOf("\"status\":false") != -1 && firebaseData.dataPath() == "/rele3")
        {
            Serial.println("desativando rele");
            digitalWrite(rele[2].pino, LOW);
        }

        else if (jsonData.indexOf("\"status\":true") != -1 && firebaseData.dataPath() == "/rele4")
        {
            Serial.println("Ativando pulso no rele4");
            digitalWrite(rele[3].pino, HIGH);
            rele4++;
            salvarDados("/rele4.txt", rele4);
        }

        if (jsonData.indexOf("\"tipoBotao\":\"switch\"") != -1 && firebaseData.dataPath() == "/rele1")
        {

            Serial.println("entrei aqui");
            if (digitalRead(rele[0].pino) == LOW)
            {
                digitalWrite(rele[0].pino, HIGH);
                delay(200);
                digitalWrite(rele[0].pino, LOW);
            }
            else
            {
                digitalWrite(rele[0].pino, LOW);
                delay(200);
                digitalWrite(rele[0].pino, HIGH);
            }
        }
        else if (jsonData.indexOf("\"tipoBotao\":\"switch\"") != -1 && firebaseData.dataPath() == "/rele2")
        {
            if (digitalRead(rele[0].pino) == LOW)
            {
                digitalWrite(rele[0].pino, HIGH);
                delay(200);
                digitalWrite(rele[0].pino, LOW);
            }
            else
            {
                digitalWrite(rele[0].pino, LOW);
                delay(200);
                digitalWrite(rele[0].pino, HIGH);
            }
        }
        else if (jsonData.indexOf("\"tipoBotao\":\"switch\"") != -1 && firebaseData.dataPath() == "/rele3")
        {
            if (digitalRead(rele[0].pino) == LOW)
            {
                digitalWrite(rele[0].pino, HIGH);
                delay(200);
                digitalWrite(rele[0].pino, LOW);
            }
            else
            {
                digitalWrite(rele[0].pino, LOW);
                delay(200);
                digitalWrite(rele[0].pino, HIGH);
            }
        }
        else if (jsonData.indexOf("\"tipoBotao\":\"switch\"") != -1 && firebaseData.dataPath() == "/rele4")
        {
            if (digitalRead(rele[0].pino) == LOW)
            {
                digitalWrite(rele[0].pino, HIGH);
                delay(200);
                digitalWrite(rele[0].pino, LOW);
            }
            else
            {
                digitalWrite(rele[0].pino, LOW);
                delay(200);
                digitalWrite(rele[0].pino, HIGH);
            }
        }
        else if (jsonData.indexOf("\"tipoBotao\":\"switch\"") != -1 && firebaseData.dataPath() == "/rele5")
        {
            if (digitalRead(rele[0].pino) == LOW)
            {
                digitalWrite(rele[0].pino, HIGH);
                delay(200);
                digitalWrite(rele[0].pino, LOW);
            }
            else
            {
                digitalWrite(rele[0].pino, LOW);
                delay(200);
                digitalWrite(rele[0].pino, HIGH);
            }
        }

        else
        {
            Serial.println("Nenhuma correspondência de atualização de status para os relés.");
        }
        
    }
}

unsigned long previousMillis = 0;
const long interval = 1000;

void loop()
{

    if (!Firebase.readStream(firebaseData))
    {
        Serial.println("Erro no stream /IdsESP/9999, aguardando próximo ciclo...");
        Firebase.endStream(firebaseData);
        if (Firebase.beginStream(firebaseData, "/IdsESP/9999"))
        {
            ESP.restart();
            Serial.println("Contador de loop: ");
            Serial.println("Firebase stream reconectado com sucesso para /IdsESP/9999.");
        }
        else
        {
            Serial.println("Contador de loop: ");
            Serial.println("Não foi possível iniciar o stream do Firebase para /IdsESP/9999");
            Serial.println(firebaseData.errorReason());
        }
    }

    // tiposBots();


    // Manter o ponto de acesso para dispositivos conectados ao WiFi quando Ethernet está ativo
    if (ethernetConnected)
    {
        WiFiClient client = wifiServer.available();
        if (client)
        {
            Serial.println("Cliente conectado ao ponto de acesso WiFi via Ethernet.");
            client.println("Conectado ao ESP32 via Ethernet como ponto de acesso WiFi!");
            client.stop();
        }
    }
    // String horaAtivacao;

    // if (firebaseData.streamAvailable())
    // {
    //     Serial.println("Dados atualizados recebidos para /IdsESP/9999:");
    //     String jsonData = firebaseData.stringData();
    //     Serial.println(jsonData);

    //     if (jsonData.indexOf("\"horaAtivacao\":") != -1 && firebaseData.dataPath() == "/rele1")
    //     {
    //         horaAtivacao = processarhorarios(firebaseData.dataPath(), jsonData, "\"horaAtivacao\":", "/rele1");
    //         if (horaAtivacao != "")
    //         {
    //             Serial.println("Horário de ativação capturado: " + horaAtivacao);
    //             Serial.println("Caminho do Firebase: " + firebaseData.dataPath());
    //             // Comparação do horário formatado
    //         }
    //     }
    // }
    // String horarioAtual = timeClient.getFormattedTime();
    // if (horaAtivacao == horarioAtual)
    // {
    //     digitalWrite(rele[0].pino, HIGH);
    //     Serial.println("Relé 1 ativado!");
    //     Serial.println("horaAtivacao" + horaAtivacao);
    // }
    // delay(100); // Delay para evitar loops excessivos
    // unsigned long currentMillis = millis();
    // if (currentMillis - previousMillis >= interval)
    // {
    //     previousMillis = currentMillis; // Atualiza o último tempo

    //     timeClient.update(); // Atualiza o horário do NTP
    //     Serial.println("Hora atual: " + timeClient.getFormattedTime());
    // }
}