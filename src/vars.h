#include <Arduino.h>
// #include <ESPAsyncWebServer.h>
// Configuração do Firebase



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

class Reles {
public:
    int btn; // Botão associado
    int pino; // Pino do relé
    int status; // Status atual (ativo/inativo)
    int contador; // Contador de eventos
    const char* caminhoArquivo; // Caminho do arquivo no SPIFFS
    String horaAtivacao; // Hora de ativação
    String horaDesativacao; // Hora de desativação

    Reles(int pino, int btn)
        : pino(pino), btn(btn), status(0), contador(0), caminhoArquivo(nullptr),
          horaAtivacao(""), horaDesativacao("") {}
};

Reles rele[5] = {
    Reles(33, 34),
    Reles(32, 36),
    Reles(14, 39),
    Reles(16, 35),
    Reles(15, 2)
};

int rele1;
int rele2;
int rele3;
int rele4;
int rele5;

bool streamReiniciado = false;