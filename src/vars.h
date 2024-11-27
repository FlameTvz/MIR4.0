#include <Arduino.h>

int reles[] = {33, 32, 14, 16};
#define TRIGGER_PIN 25
#define BTN_CONFIG 17
bool res;

class Reles
{
public:
    int btn; //== botao verde
    int pino; //== rele
    int status;
    int contador;
    const char* caminhoArquivo;

    Reles(int pino, int btn)
    {
        this->btn = btn;
        this->pino = pino;
    }
};

Reles rele[5] = {
    Reles(33, 34),
    Reles(32, 36),
    Reles(14, 39),
    Reles(16, 35),
    Reles(24, 43)};


int rele1 = 0;
int rele2 = 0;
int rele3 = 0;
int rele4 = 0;
int rele5 = 0;
#define FORMAT_SPIFFS_IF_FAILED true
