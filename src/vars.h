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
