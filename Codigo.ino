#include "SevSeg.h"

SevSeg sevseg;

// --- DADOS DA SUA REDE WI-FI ---
const String NOME_WIFI = "WIFI UNIFEOB"; 
const String SENHA_WIFI = ""; 
// -------------------------------

// --- CONFIGURAÇÃO DO NTFY ---
const String SERVER    = "ntfy.sh"; 
const String TOPICO    = "projeto_alerta_bomba_1977_2002"; // Mantenha o seu tópico secreto aqui
// ----------------------------

#define PINO_A0 A0 
#define PINO_A1 A1 
#define PINO_A2 A2 
#define PINO_A3 A3 

#define PINO_PWM_VERMELHO 5
#define PINO_PWM_AMARELO 6

#define PINO_RGB_R A4   
#define PINO_RGB_G 11   
#define PINO_BUZZER 10 

const float RESISTOR_SHUNT = 220.0; 

unsigned long temporizadorMedicao = 0;
unsigned long temporizadorPisca = 0;
const unsigned long INTERVALO_PISCA = 10000; 

// NOVIDADE 1: Variáveis de consumo agora são globais para o envio poder acessá-las
int potenciaTotal_mW = 0;
int potenciaVermelho_mW = 0;
int potenciaAmarelo_mW = 0;
bool ledsLigados = true; 

void atualizarMedicao() {
  if (ledsLigados) {
    long somaA0 = 0, somaA1 = 0, somaA2 = 0, somaA3 = 0;
    int amostras = 10; 

    for(int i = 0; i < amostras; i++) {
      somaA0 += analogRead(PINO_A0);
      somaA1 += analogRead(PINO_A1);
      somaA2 += analogRead(PINO_A2);
      somaA3 += analogRead(PINO_A3);
    }

    float mediaA2 = (somaA2 / amostras) * (5.0 / 1023.0);
    float mediaA3 = (somaA3 / amostras) * (5.0 / 1023.0);
    float correnteA = (mediaA2 - mediaA3) / RESISTOR_SHUNT; 
    
    // Atualiza a variável global do Amarelo
    potenciaAmarelo_mW = (int)((mediaA3 * correnteA) * 1000.0);
    if(potenciaAmarelo_mW < 0) potenciaAmarelo_mW = 0;

    float mediaA0 = (somaA0 / amostras) * (5.0 / 1023.0);
    float mediaA1 = (somaA1 / amostras) * (5.0 / 1023.0);
    float correnteV = (mediaA0 - mediaA1) / RESISTOR_SHUNT; 
    
    // Atualiza a variável global do Vermelho
    potenciaVermelho_mW = (int)((mediaA1 * correnteV) * 1000.0);
    if(potenciaVermelho_mW < 0) potenciaVermelho_mW = 0;

    potenciaTotal_mW = potenciaVermelho_mW + potenciaAmarelo_mW;
  } else {
    potenciaTotal_mW = 0;
    potenciaVermelho_mW = 0;
    potenciaAmarelo_mW = 0;
  }

  int valorDisplay = potenciaTotal_mW;
  if (valorDisplay > 99) valorDisplay = 99; 
  sevseg.setNumber(valorDisplay); 
}

void enviarComandoAT(String comando, int timeout) {
  Serial.println(comando);
  unsigned long tempo = millis();
  
  while ((millis() - tempo) < timeout) {
    while (Serial.available()) {
      char c = Serial.read(); 
    }
    
    if (millis() - temporizadorMedicao >= 500) {
      temporizadorMedicao = millis();
      atualizarMedicao();
    }
    
    sevseg.refreshDisplay(); 
  }
}

void enviarNotificacao(String payload) {
  enviarComandoAT("AT+CIPSTART=\"TCP\",\"" + SERVER + "\",80", 2000);
  
  String requisicao = "POST /" + TOPICO + " HTTP/1.1\r\n" +
                      "Host: " + SERVER + "\r\n" +
                      "Connection: close\r\n" +
                      "Content-Length: " + String(payload.length()) + "\r\n\r\n" +
                      payload;
  
  enviarComandoAT("AT+CIPSEND=" + String(requisicao.length()), 1000);
  enviarComandoAT(requisicao, 2000);
}

void setup() {
  byte numDigits = 2; 
  byte digitPins[] = {12, 13}; 
  byte segmentPins[] = {2, 3, 4, A5, 9, 7, 8, 20}; 
  
  sevseg.begin(COMMON_ANODE, numDigits, digitPins, segmentPins, true);
  sevseg.setBrightness(90);

  pinMode(PINO_PWM_VERMELHO, OUTPUT);
  pinMode(PINO_PWM_AMARELO, OUTPUT);
  pinMode(PINO_RGB_R, OUTPUT);
  pinMode(PINO_RGB_G, OUTPUT);
  pinMode(PINO_BUZZER, OUTPUT);

  Serial.begin(115200); 

  enviarComandoAT("AT+CWMODE=1", 1000); 
  
  String comandoConexao = "AT+CWJAP=\"" + NOME_WIFI + "\",\"" + SENHA_WIFI + "\"";
  enviarComandoAT(comandoConexao, 8000); 
}

void tocarAlarme() {
  for(int b = 0; b < 3; b++) {
    digitalWrite(PINO_BUZZER, HIGH);
    unsigned long t = millis();
    while(millis() - t < 60) sevseg.refreshDisplay(); 
    
    digitalWrite(PINO_BUZZER, LOW);
    t = millis();
    while(millis() - t < 60) sevseg.refreshDisplay(); 
  }
}

void loop() {
  unsigned long tempoAtual = millis();

  if (tempoAtual - temporizadorPisca >= INTERVALO_PISCA) {
    ledsLigados = !ledsLigados; 

    if (ledsLigados) {
      analogWrite(PINO_PWM_VERMELHO, 255);
      analogWrite(PINO_PWM_AMARELO, 255);
      
      digitalWrite(PINO_RGB_R, LOW);
      digitalWrite(PINO_RGB_G, HIGH);

      tocarAlarme();
      atualizarMedicao();

      // SOLUÇÃO: Juntamos tudo numa única "Super Mensagem"
      String mensagemCompleta = "🚨😱 Alerta: As Bombas da Água foram Acionadas!\n\n📊 Consumo Atual:\n🔴 Bomba 01: " + String(potenciaVermelho_mW) + " mW\n🟡 Bomba 02: " + String(potenciaAmarelo_mW) + " mW\n⚡ Total: " + String(potenciaTotal_mW) + " mW";
      
      // Enviamos apenas 1 vez por ciclo!
      enviarNotificacao(mensagemCompleta);
      
    } else {
      analogWrite(PINO_PWM_VERMELHO, 0);
      analogWrite(PINO_PWM_AMARELO, 0);
      
      digitalWrite(PINO_RGB_R, HIGH);
      digitalWrite(PINO_RGB_G, LOW);
      
      atualizarMedicao();

      // SOLUÇÃO: Mensagem única para quando apaga
      String mensagemCompleta = "⚪😢 Alerta: As Bombas da Água foram Desligadas!\n\n📊 Consumo Atual:\n🔴 0 mW | 🟡 0 mW | ⚡ Total: 0 mW";
      
      // Enviamos apenas 1 vez por ciclo!
      enviarNotificacao(mensagemCompleta);
    }

    temporizadorPisca = millis();
  }

  // Mantém a medição a ser atualizada a cada 500ms
  if (millis() - temporizadorMedicao >= 500) {
    temporizadorMedicao = millis();
    atualizarMedicao();
  }

  // Mantém o display ligado
  sevseg.refreshDisplay(); 
}
