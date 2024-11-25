#include <Arduino.h>

int reles[] = {33, 32, 14, 16};
#define TRIGGER_PIN 25
#define BTN_CONFIG 17
bool res;

uint8_t eth_MAC[] = {0x02, 0xF0, 0x0D, 0xBE, 0xEF, 0x03};

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

const char *API_KEY = "AIzaSyCSISjoMLyNpbfHNN3RS06WkMx4L21GjTU";
const char *DATABASE_URL = "https://firestore-esp32-de37c-default-rtdb.firebaseio.com/";
const char *EMAIL = "renan.hdk.sgr@gmail.com";
const char *EMAIL_PASSWORD = "180931er";
int rele1 = 0;
int rele2 = 0;
int rele3 = 0;
int rele4 = 0;
int rele5 = 0;
#define FORMAT_SPIFFS_IF_FAILED true
