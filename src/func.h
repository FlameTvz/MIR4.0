void salvarDados(const char* caminho, int dados);
void leRele(void);
void DisplayInit(void);


// Display
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);


void DisplayInit() {

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  delay(2000);
  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 10);
  // Display static text
  // display.print("Date: " + date + " - Time: " + hour);
  display.display(); 
}

void salvarDados(const char* caminho, int dados) {
    File arquivo = SPIFFS.open(caminho, FILE_WRITE);
    if (!arquivo) {
        Serial.println("Falha ao abrir o arquivo para escrita!");
        return;
    }

    if (arquivo.print(dados)) {
        Serial.println("Dados salvos com sucesso!");
    } else {
        Serial.println("Falha ao escrever no arquivo!");
    }
    arquivo.close();  // Fechar o arquivo após a escrita
}

void leRele(){
    bool valor;
    for(int i = 0; i< qtdRele; i++){
            leitura[i] = digitalRead(rele[i].btn);
            // Serial.println(leitura[i]);
    }
}

void entradaNaSaida(){
    bool algumON = 0;
    leRele();
        for(int i = 0; i< qtdRele; i++){
            // Serial.println(i);
            if(leitura[i] == 1){
                Serial.printf("Botão do relé %d está pressionado (HIGH)\n", i);
                digitalWrite(rele[i].pino, HIGH);
                algumON = 1;
            }
        }
        if(algumON == 1){
                //*************************** */
                display.clearDisplay();
                display.fillScreen(WHITE);
                display.display(); 
                //*************************** */
                delay(tempoClick);

        }
        else{
                //*************************** */
                display.clearDisplay();
                display.fillScreen(BLACK);
                display.display(); 
                //*************************** */ 
        }
        for(int i = 0; i< qtdRele; i++){
            if(leitura[i] == 1){
                digitalWrite(rele[i].pino, LOW);
            }
        }
        for(int i = 0; i< qtdRele; i++){
            if(leitura[i] == 1){
                if (i == 0){
                    rele1++;
                    salvarDados("/rele1.txt", rele1);
                }
                if (i == 1){
                    rele1++;
                    salvarDados("/rele2.txt", rele2);
                }
                if (i == 2){
                    rele1++;
                    salvarDados("/rele3.txt", rele3);
                }
                if (i == 3){
                    rele1++;
                    salvarDados("/rele4.txt", rele4);
                }
                if (i == 4){
                    rele1++;
                    salvarDados("/rele5.txt", rele5);
                }
            }
        }

}