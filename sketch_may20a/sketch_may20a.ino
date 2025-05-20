#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <EEPROM.h>

// Configuração do LCD e Servo
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Endereço I2C 0x27, display 16x2
Servo servo;
#define MOTOR_PIN 13                  // GPIO13 para o servo

// Credenciais (substitua pelas suas)
char auth[] = "SEU_TOKEN_BLYNK";
char ssid[] = "SUA_REDE_WIFI";
char pass[] = "SENHA_WIFI";

// Variáveis do sistema
int qtdRacao = 10;
int qtdRefeicoes = 1;
int racaoPorRefeicao;
int rotacoesMotor;

int tela = 1;
int S = 0, M = 0, H = 0;
int sentido = 0;
int countSentido = 0;

const int EEPROM_POS = 0;
double tempoHora, tempoMinuto, tempoSegundo;

// Estrutura para armazenamento persistente
struct Config {
  int servoPos;
  int sentido;
  int countSentido;
  byte checksum;
};

// Protótipos de funções
void loadConfig();
void salvarConfig();
byte calcularChecksum(Config config);
void atualizarTela();
void telaRacao();
void telaRefeicoes();
void telaCronometro();
void calcularTempos();
void rodarMotor();
void cronometro(double horas);

BLYNK_CONNECTED() {
  Blynk.syncAll();
  Blynk.virtualWrite(V6, tela);
  Blynk.virtualWrite(V7, "00:00:00");
}

BLYNK_WRITE(V0) { 
  qtdRacao = min(qtdRacao + 10, 1000);
  atualizarTela(); 
}

BLYNK_WRITE(V1) { 
  qtdRacao = max(qtdRacao - 10, 10);
  atualizarTela(); 
}

BLYNK_WRITE(V2) { 
  qtdRefeicoes = min(qtdRefeicoes + 1, 24);
  atualizarTela(); 
}

BLYNK_WRITE(V3) { 
  qtdRefeicoes = max(qtdRefeicoes - 1, 1);
  atualizarTela(); 
}

BLYNK_WRITE(V4) { 
  tela = tela % 3 + 1;
  atualizarTela();
}

BLYNK_WRITE(V5) { 
  ESP.restart();
}

void setup() {
  Serial.begin(115200);
  
  lcd.init();
  lcd.backlight();
  lcd.print("Iniciando...");
  
  servo.attach(MOTOR_PIN);

  EEPROM.begin(64);
  loadConfig();

  // Inicia WiFiManager
  WiFiManager wm;
  lcd.setCursor(0, 1);
  lcd.print("Config WiFi...");

  // Abre um AP chamado "ConfigESP32" se não conseguir conectar
  bool res = wm.autoConnect("ConfigESP32");

  if (!res) {
    lcd.clear();
    lcd.print("Erro WiFi");
    lcd.setCursor(0, 1);
    lcd.print("Reiniciando...");
    delay(3000);
    ESP.restart();
  }

  // Agora conectado à internet
  lcd.clear();
  lcd.print("Conectado WiFi");
  delay(1000);

  // Inicia Blynk
  Blynk.config(auth);
  Blynk.connect();

  calcularTempos();
  atualizarTela();
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    Blynk.run();
  }
  
  if (tela == 3) {
    cronometro(tempoHora);
  }
  
  delay(10); // Pequeno delay para evitar sobrecarga
}

void loadConfig() {
  Config config;
  EEPROM.get(EEPROM_POS, config);
  
  if (config.checksum != calcularChecksum(config)) {
    // Configuração padrão se checksum inválido
    config = {90, 0, 0, 0};
    config.checksum = calcularChecksum(config);
    EEPROM.put(EEPROM_POS, config);
    EEPROM.commit();
  }
  
  servo.write(config.servoPos);
  sentido = config.sentido;
  countSentido = config.countSentido;
}

void salvarConfig() {
  Config config;
  config.servoPos = servo.read();
  config.sentido = sentido;
  config.countSentido = countSentido;
  config.checksum = calcularChecksum(config);
  
  EEPROM.put(EEPROM_POS, config);
  EEPROM.commit();
}

byte calcularChecksum(Config config) {
  byte checksum = 0;
  byte* bytes = (byte*)&config;
  for (int i = 0; i < sizeof(config) - 1; i++) {
    checksum ^= bytes[i];
  }
  return checksum;
}

void atualizarTela() {
  lcd.clear();
  switch (tela) {
    case 1: telaRacao(); break;
    case 2: telaRefeicoes(); break;
    case 3: telaCronometro(); break;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Blynk.virtualWrite(V6, tela);
  }
}

void telaRacao() {
  lcd.setCursor(0, 0);
  lcd.print("RACAO DIARIA:");
  lcd.setCursor(0, 1);
  lcd.print(qtdRacao);
  lcd.print(" GRAMAS");
}

void telaRefeicoes() {
  lcd.setCursor(0, 0);
  lcd.print("QTD REFEICOES:");
  lcd.setCursor(0, 1);
  lcd.print(qtdRefeicoes);
  lcd.print(qtdRefeicoes == 1 ? " REFEICAO" : " REFEICOES");
  
  calcularTempos();
}

void telaCronometro() {
  lcd.setCursor(0, 0);
  lcd.print("COMIDA EM:");
  lcd.setCursor(0, 1);
  lcd.printf("%02d:%02d:%02d", H, M, S);
}

void calcularTempos() {
  tempoHora = 24.0 / qtdRefeicoes;
  tempoMinuto = (tempoHora - int(tempoHora)) * 60;
  tempoSegundo = (tempoMinuto - int(tempoMinuto)) * 60;
  racaoPorRefeicao = qtdRacao / qtdRefeicoes;

  while (racaoPorRefeicao % 10 != 0) {
    racaoPorRefeicao++;
  }

  rotacoesMotor = racaoPorRefeicao / 10;
  
  // Reinicia o cronômetro quando recalcular
  H = tempoHora;
  M = tempoMinuto;
  S = tempoSegundo;
}

void rodarMotor() {
  static unsigned long lastMove = 0;
  static int currentStep = 0;
  static int rotationsDone = 0;
  
  if (currentStep == 0 && rotationsDone >= rotacoesMotor) {
    rotationsDone = 0;
    salvarConfig();
    return;
  }
  
  if (millis() - lastMove >= 100) {
    lastMove = millis();
    
    int atual = servo.read();
    int novoPos = sentido == 0 ? atual + 5 : atual - 5;
    servo.write(constrain(novoPos, 0, 180));
    
    currentStep++;
    if (currentStep >= 18) { // 90 graus / 5 = 18 passos
      currentStep = 0;
      rotationsDone++;
      
      countSentido++;
      if (countSentido >= 2) {
        sentido = !sentido;
        countSentido = 0;
      }
    }
  }
}

void cronometro(double horas) {
  static unsigned long lastUpdate = 0;
  static bool motorStarted = false;
  
  if (millis() - lastUpdate >= 1000) {
    lastUpdate = millis();
    
    if (--S < 0) { S = 59; M--; }
    if (M < 0) { M = 59; H--; }
    if (H < 0) {
      H = horas;
      M = tempoMinuto;
      S = tempoSegundo;
      motorStarted = false;
    }
    
    // Atualiza display
    telaCronometro();
    
    // Atualiza Blynk
    if (WiFi.status() == WL_CONNECTED) {
      char buffer[9];
      sprintf(buffer, "%02d:%02d:%02d", H, M, S);
      Blynk.virtualWrite(V7, buffer);
    }
    
    // Dispara motor quando chegar a zero
    if (H == 0 && M == 0 && S == 0 && !motorStarted) {
      motorStarted = true;
      rodarMotor();
    }
  }
}