
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <qrcode.h>

#define SCREEN_WIDTH 128 // Largura do display OLED
#define SCREEN_HEIGHT 64 // Altura do display OLED

#define OLED_RESET -1       // Reset não utilizado para SSD1306
#define SCREEN_ADDRESS 0x3C // Endereço I2C do display

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
QRCode qrcode;

// Função para converter o número de série do ESP32 em uma string hexadecimal
String getESP32Serial()
{
    uint64_t chipId = ESP.getEfuseMac();
    char serialStr[18];
    sprintf(serialStr, "%04X%08X", (uint16_t)(chipId >> 32), (uint32_t)chipId);
    return String(serialStr);
}

void DisplayIP()
{
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.println("Wi-Fi conectado!");
    display.println("IP:");
    display.println(WiFi.localIP().toString());
    display.display();
}

void drawQRCode(QRCode &qrcode)
{
    display.clearDisplay();
    int scale = 2; // Escala do QR Code
    int offsetX = (SCREEN_WIDTH - qrcode.size * scale) / 2;
    int offsetY = (SCREEN_HEIGHT - qrcode.size * scale) / 2;

    for (uint8_t y = 0; y < qrcode.size; y++)
    {
        for (uint8_t x = 0; x < qrcode.size; x++)
        {
            if (qrcode_getModule(&qrcode, x, y))
            {
                display.fillRect(offsetX + x * scale, offsetY + y * scale, scale, scale, WHITE);
            }
        }
    }
    display.display();
}

void alternateDisplay()
{
    unsigned long interval = 5000;
    static unsigned long previousMillis = 0;
    static bool showIP = true;
    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= interval)
    {
        previousMillis = currentMillis;

        if (showIP)
        {
            DisplayIP();
        }
        else
        {
            String serialNumber = getESP32Serial();
            QRCode qrcode;
            uint8_t qrcodeData[qrcode_getBufferSize(3)];
            qrcode_initText(&qrcode, qrcodeData, 3, ECC_LOW, serialNumber.c_str());

            drawQRCode(qrcode);
        }

        showIP = !showIP;
    }
}

// void setup() {
//     Serial.begin(115200);

//     // Configuração do I2C nos pinos corretos
//     Wire.begin(4, 21);  // SDA no pino 4, SCL no pino 21

//     if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
//         Serial.println(F("Falha ao inicializar o display OLED"));
//         for (;;);
//     }

//     display.clearDisplay();
//     display.setTextSize(1);
//     display.setTextColor(WHITE);
//     display.setCursor(10, 10);
//     display.println("Gerando QR Code...");
//     display.display();

//     // Obter o serial único do ESP32
//     String serialNumber = getESP32Serial();
//     Serial.println("Serial do ESP32: " + serialNumber);

//     // Criar QR Code com o serial único do ESP32
//     QRCode qrcode;
//     uint8_t qrcodeData[qrcode_getBufferSize(3)];
//     qrcode_initText(&qrcode, qrcodeData, 3, ECC_LOW, serialNumber.c_str());

//     drawQRCode(qrcode);

//     Serial.println(serialNumber);
// }