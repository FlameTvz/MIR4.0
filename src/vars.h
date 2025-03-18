#include <Arduino.h>
#include <Ethernet.h>
#include <Firebase_ESP_Client.h>
#include <SPIFFS.h>
#include <NTPClient.h>
#include <ESPAsyncWebServer.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

const char *API_KEY = "AIzaSyBbbkxHGdp00gDNoUfpYVH5ezBm5rEw3gY";
const char *DATABASE_URL = "https://poised-bot-443613-p6-default-rtdb.firebaseio.com/";
const char *EMAIL = "leandro.lopes@inovanex.com.br";
const char *EMAIL_PASSWORD = "Inova123NEX";
const char *PROJECT_ID = "poised-bot-443613-p6";

bool usuarioAutorizado = false; // Variável global para armazenar o status do usuário

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
String horaAtivacao2 = "";
String horaDesativacao2 = "";
String horaAtivacao3 = "";
String horaDesativacao3 = "";
String horaAtivacao4 = "";
String horaDesativacao4 = "";
String horaAtivacao5 = "";
String horaDesativacao5 = "";
String horarioAtual = "";



bool ethernetConnected = false, wifiConnected = false;
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};

int tempoClick = 200;
bool leitura[] = {0, 0, 0, 0, 0};
int qtdRele = 5;

int reles[] = {33, 32, 14, 16, 15};
#define TRIGGER_PIN 25
#define BTN_CONFIG 17
bool res;

class Reles
{
public:
    int btn;                    // Botão associado
    int pino;                   // Pino do relé
    int status;                 // Status atual (ativo/inativo)
    int contador;               // Contador de eventos
    const char *caminhoArquivo; // Caminho do arquivo no SPIFFS
    String horaAtivacao;        // Hora de ativação
    String horaDesativacao;     // Hora de desativação
    String nome;

    Reles(int pino, int btn)
        : pino(pino), btn(btn), status(0), contador(0), caminhoArquivo(nullptr),
          horaAtivacao(""), horaDesativacao(""), nome("") {}
};

Reles rele[5] = {
    Reles(33, 34),
    Reles(32, 36),
    Reles(14, 39),
    Reles(16, 35),
    Reles(15, 2)};

int rele1;
int rele2;
int rele3;
int rele4;
int rele5;

bool streamReiniciado = false;
int temposEntrada[5] = {200, 200, 200, 200, 200};
int temposPulso[5] = {200, 200, 200, 200, 200};
bool relesAtivos[5] = {false, false, false, false, false};
int modoAcionamento[5] = {2, 2, 2, 2, 2};

unsigned long lastCheckTime = 0;
const unsigned long checkInterval = 30000;  // Tempo de verificação (30 segundos)


int tempoEntrada = 200;
// int tempoEntrada = 200;
struct SwitchTask
{
    bool ativo = false;
    unsigned long inicio = 0;
    int releIdx = -1;
};
SwitchTask switchAtual;

const int BTN_PIN = 17;            // Pino em que o botão está conectado
const unsigned long HOLD_TIME = 5000; // 5 segundos (em ms)

bool buttonPressed = false;   // Para marcar se o botão está pressionado
unsigned long pressStartTime = 0;

// 22 scl
// 21 sda


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