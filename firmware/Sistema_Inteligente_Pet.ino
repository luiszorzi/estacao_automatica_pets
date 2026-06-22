#include "secrets.h" 
#include "HX711.h"
#include <ESP32Servo.h>
#include <Wire.h>
#include "RTClib.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <LittleFS.h> 

// ==========================================
// MAPEAMENTO DE PINOS 
// ==========================================
#define DOUT_AGUA 25
#define SCK_AGUA  14
const int PINO_RELE = 26;

#define DOUT_RACAO 32 
#define CLK_RACAO  33
const int PINO_SERVO = 27;

// ==========================================
// OBJETOS 
// ==========================================
HX711 balancaAgua;
HX711 balancaRacao;
Servo servoMotor;
RTC_DS3231 rtc;

// ==========================================
// WI-FI E NUVEM 
// ==========================================
const char* ssid = SECRET_SSID;       
const char* password = SECRET_PASS;  
String apiKey = SECRET_API_KEY;        

unsigned long timer_status_nuvem = 0;
const unsigned long INTERVALO_STATUS = 900000; 

// ==========================================
// BEBEDOURO (ÁGUA) 
// ==========================================
float calibracao_agua = 211400.00; 
long offset_agua = 2511;
const float AGUA_PESO_LIGAR = 150.0;     
const float AGUA_PESO_DESLIGAR = 250.0;  
const float AGUA_TIGELA_PRESENTE = 25.0; 
const float AGUA_PESO_TIGELA = 74.0;

bool agua_sistemaAtivo = false; 
unsigned long agua_timerBomba = 0;
bool agua_bombaLigada = false;
unsigned long agua_tempoEstabilizacao = 0;
unsigned long agua_timerFalsaRemocao = 0;
bool agua_verificandoCheio = false;
unsigned long agua_timerVerificacao = 0; 
float agua_pesoInicioEnchimento = 0; 

// ==========================================
// COMEDOURO (RAÇÃO) 
// ==========================================
float calibracao_racao = 235400.00; 
long offset_racao = 166104;
const float RACAO_PESO_TIGELA = 74.0;    
const float RACAO_PORCAO = 40.0; 
const float RACAO_ALVO_TOTAL = RACAO_PESO_TIGELA + RACAO_PORCAO; 
const float RACAO_MARGEM_AUSENTE = 65.0; 
const float RACAO_SOBRA_ACEITAVEL = 15.0; 

const int POS_FECHADA = 0; 
const int POS_ABERTA_MAX = 35;  
const int POS_ABERTA_FINA = 25; 

// HORÁRIOS DAS REFEIÇÕES
const int HORA_REF_1_BASE = 8;
const int MINUTO_REF_1_BASE = 0;   
int hora_ref_1_dinamica = HORA_REF_1_BASE;
int minuto_ref_1_dinamico = MINUTO_REF_1_BASE; 

const int HORA_REF_2_BASE = 14; 
const int MINUTO_REF_2_BASE = 0;  
int hora_ref_2_dinamica = HORA_REF_2_BASE;
int minuto_ref_2_dinamico = MINUTO_REF_2_BASE; 

const int HORA_REF_3_BASE = 20; 
const int MINUTO_REF_3_BASE = 0;  
int hora_ref_3_dinamica = HORA_REF_3_BASE;
int minuto_ref_3_dinamico = MINUTO_REF_3_BASE;

int ultima_hora_servida = -1; 

// ==========================================
// SETUP
// ==========================================
void setup() {
  Serial.begin(115200); 
  
  Serial.println("\nIniciando sistema de arquivos interno...");
  if (!LittleFS.begin(true)) {
    Serial.println("ERRO CRITICO: Falha ao montar o LittleFS!");
  } else {
    Serial.println("LittleFS montado com sucesso.");
    if (LittleFS.exists("/backup.csv")) {
      Serial.println("\n--- CONTEUDO DO BACKUP OFFLINE ---");
      File file = LittleFS.open("/backup.csv", FILE_READ);
      while (file.available()) {
        Serial.write(file.read());
      }
      file.close();
      Serial.println("----------------------------------\n");
    }
  }

  Serial.println("Limpando memoria do Wi-Fi...");
  WiFi.disconnect(true); 
  delay(1000);
  WiFi.mode(WIFI_STA);   

  Serial.print("Conectando ao Wi-Fi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  
  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 60) {
    delay(500);
    Serial.print(".");
    tentativas++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("\nWi-Fi Conectado com SUCESSO! IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFalha no Wi-Fi. Iniciando modo offline.");
  }

  pinMode(PINO_RELE, OUTPUT);
  digitalWrite(PINO_RELE, HIGH); 
  
  if (!rtc.begin()) {
    Serial.println("ERRO: Modulo RTC DS3231 nao encontrado!");
    while (1); 
  }
  if (rtc.lostPower()) {
    Serial.println("ALERTA: A bateria do RTC acabou e ele perdeu a hora!");
  }
  
  ESP32PWM::allocateTimer(0);
  servoMotor.setPeriodHertz(50);
  servoMotor.attach(PINO_SERVO, 500, 2400);
  servoMotor.write(POS_FECHADA); 
  delay(1000); 
  servoMotor.detach(); 
  
  balancaAgua.begin(DOUT_AGUA, SCK_AGUA);
  balancaAgua.set_scale(calibracao_agua);
  balancaAgua.set_offset(offset_agua); 
  
  balancaRacao.begin(DOUT_RACAO, CLK_RACAO);
  balancaRacao.set_scale(calibracao_racao);
  balancaRacao.set_offset(offset_racao); 
  
  Serial.println("\n--- ESTACAO COMPLETA (AGUA E RACAO) ---");
  Serial.println("Sistema Iniciado! Operacao autonoma ativa.");
}

// ==========================================
// LOOP
// ==========================================
void loop() {
  DateTime agora = rtc.now();

  float pesoAgua = balancaAgua.get_units(5) * 1000;
  float pesoRacao = balancaRacao.get_units(5) * 1000; 

  if (pesoAgua < 0 && pesoAgua > -10.0) pesoAgua = 0.0;
  if (pesoRacao < 0 && pesoRacao > -10.0) pesoRacao = 0.0;

  static unsigned long tempo_ant = 0;
  if (millis() - tempo_ant > 2000 && !agua_bombaLigada) {
    char hora_formatada[10];
    sprintf(hora_formatada, "%02d:%02d:%02d", agora.hour(), agora.minute(), agora.second());
    
    Serial.print("["); Serial.print(hora_formatada); Serial.print("] ");
    Serial.print("Agua: "); Serial.print(pesoAgua, 1); Serial.print("g | ");
    Serial.print("Racao: "); Serial.print(pesoRacao, 1); Serial.println("g");
    
    tempo_ant = millis();
  }

  // --- NOVA FUNÇÃO: ENVIA STATUS A CADA 15 MINUTOS ---
  if (millis() - timer_status_nuvem > INTERVALO_STATUS) {
    enviarStatusTempoReal(pesoAgua, pesoRacao);
    timer_status_nuvem = millis();
  }

  if (pesoAgua > -500.0 && pesoAgua < 2000.0) {
    processarBebedouro(pesoAgua);
  }

  processarComedouro(agora, pesoRacao);
  
  delay(30); 
}

// ==========================================
// LÓGICA DO BEBEDOURO
// ==========================================
void processarBebedouro(float pesoAtual) {
  unsigned long agora = millis();

  if (pesoAtual < AGUA_TIGELA_PRESENTE) {
    if (agua_timerFalsaRemocao == 0) agua_timerFalsaRemocao = agora;
    if (agora - agua_timerFalsaRemocao > 800) { 
      if (agua_sistemaAtivo || agua_bombaLigada) {
        digitalWrite(PINO_RELE, HIGH);
        agua_sistemaAtivo = false;
        agua_bombaLigada = false;
        agua_verificandoCheio = false; 
        Serial.println("!!! TIGELA DE AGUA REMOVIDA !!!");
      }
      agua_tempoEstabilizacao = 0;
    }
  } else {
    agua_timerFalsaRemocao = 0;
  }

  if (pesoAtual <= AGUA_PESO_LIGAR && pesoAtual > AGUA_TIGELA_PRESENTE && !agua_sistemaAtivo) {
    if (agua_tempoEstabilizacao == 0) agua_tempoEstabilizacao = agora;
    if (agora - agua_tempoEstabilizacao > 10000) { 
      agua_sistemaAtivo = true;
      agua_verificandoCheio = false; 
      agua_pesoInicioEnchimento = pesoAtual; 
      Serial.println(">>> Iniciando enchimento de agua...");
    }
  } 
  else if (pesoAtual > AGUA_PESO_LIGAR && !agua_sistemaAtivo) {
    agua_tempoEstabilizacao = 0;
  }

  if (agua_sistemaAtivo) {
    if (agua_verificandoCheio) {
      if (agora - agua_timerVerificacao > 3000) { 
        if (pesoAtual >= (AGUA_PESO_DESLIGAR - 15.0)) { 
          agua_sistemaAtivo = false;
          agua_verificandoCheio = false;
          Serial.println("✅ Bebedouro Cheio e Estabilizado!");
          
          float agua_adicionada = pesoAtual - agua_pesoInicioEnchimento;
          registrarEventoNaNuvem(1, agua_adicionada); 
          
        } else {
          Serial.println("-> Falso alarme na agua. Retomando pulso...");
          agua_verificandoCheio = false;
          agua_timerBomba = agora; 
        }
      }
    } 
    else {
      if (!agua_bombaLigada) {
        if (pesoAtual >= AGUA_PESO_DESLIGAR) {
          Serial.println("-> Peso da agua atingido. Pausando para confirmar...");
          agua_verificandoCheio = true;
          agua_timerVerificacao = agora;
        }
        else if (agora - agua_timerBomba > 3000) {
          digitalWrite(PINO_RELE, LOW);
          agua_bombaLigada = true;
          agua_timerBomba = agora;
        }
      } 
      else { 
        if (agora - agua_timerBomba > 2000) {
          digitalWrite(PINO_RELE, HIGH);
          agua_bombaLigada = false;
          agua_timerBomba = agora;
        }
      }
    }
  }
}

// ==========================================
// LÓGICA DO COMEDOURO
// ==========================================
void processarComedouro(DateTime agora, float pesoRacao) {
  if (agora.hour() == 3 && agora.minute() == 0 && agora.second() == 0) {
    hora_ref_1_dinamica = HORA_REF_1_BASE;
    minuto_ref_1_dinamico = MINUTO_REF_1_BASE;
    hora_ref_2_dinamica = HORA_REF_2_BASE;
    minuto_ref_2_dinamico = MINUTO_REF_2_BASE;
    hora_ref_3_dinamica = HORA_REF_3_BASE;
    minuto_ref_3_dinamico = MINUTO_REF_3_BASE;
  }

  if (agora.hour() >= HORA_REF_2_BASE && agora.hour() < HORA_REF_3_BASE) {
    if (hora_ref_1_dinamica != HORA_REF_1_BASE || minuto_ref_1_dinamico != MINUTO_REF_1_BASE) {
      hora_ref_1_dinamica = HORA_REF_1_BASE;
      minuto_ref_1_dinamico = MINUTO_REF_1_BASE;
      Serial.println("\n[INFO] Alarme das 08:00 expirou por atingir o horario da tarde. Focando no de 14:00!");
    }
  }

  if (agora.hour() >= HORA_REF_3_BASE) {
    if (hora_ref_2_dinamica != HORA_REF_2_BASE || minuto_ref_2_dinamico != MINUTO_REF_2_BASE) {
      hora_ref_2_dinamica = HORA_REF_2_BASE;
      minuto_ref_2_dinamico = MINUTO_REF_2_BASE;
      Serial.println("\n[INFO] Alarme das 14:00 expirou por atingir o horario da noite. Focando no de 20:00!");
    }
  }

  bool eh_hora_ref_1 = (agora.hour() == hora_ref_1_dinamica && agora.minute() == minuto_ref_1_dinamico);
  bool eh_hora_ref_2 = (agora.hour() == hora_ref_2_dinamica && agora.minute() == minuto_ref_2_dinamico);
  bool eh_hora_ref_3 = (agora.hour() == hora_ref_3_dinamica && agora.minute() == minuto_ref_3_dinamico);

  if ((eh_hora_ref_1 || eh_hora_ref_2 || eh_hora_ref_3) && ultima_hora_servida != agora.hour()) {
    
    float racao_sobrando = pesoRacao - RACAO_PESO_TIGELA; 
    
    if (pesoRacao < RACAO_MARGEM_AUSENTE) {
      Serial.println("\n[SEGURANCA] Alarme ativo, mas tigela ausente! Adicionando +30 minutos na tentativa...");
      if (eh_hora_ref_1) {
        minuto_ref_1_dinamico = agora.minute() + 30;
        if (minuto_ref_1_dinamico >= 60) { minuto_ref_1_dinamico %= 60; hora_ref_1_dinamica = (agora.hour() + 1) % 24; }
      }
      if (eh_hora_ref_2) {
        minuto_ref_2_dinamico = agora.minute() + 30;
        if (minuto_ref_2_dinamico >= 60) { minuto_ref_2_dinamico %= 60; hora_ref_2_dinamica = (agora.hour() + 1) % 24; }
      }
      if (eh_hora_ref_3) {
        minuto_ref_3_dinamico = agora.minute() + 30;
        if (minuto_ref_3_dinamico >= 60) { minuto_ref_3_dinamico %= 60; hora_ref_3_dinamica = (agora.hour() + 1) % 24; }
      }
    }
    else if (racao_sobrando >= RACAO_SOBRA_ACEITAVEL) {
      Serial.println("\n[AVISO MODO SONECA] O prato de racao ainda esta cheio!");
      Serial.print("Racao detectada: "); Serial.print(racao_sobrando, 1);
      Serial.println(" g. Adicionando +30 minutos na tentativa...");
      if (eh_hora_ref_1) {
        minuto_ref_1_dinamico = agora.minute() + 30;
        if (minuto_ref_1_dinamico >= 60) { minuto_ref_1_dinamico %= 60; hora_ref_1_dinamica = (agora.hour() + 1) % 24; }
      }
      if (eh_hora_ref_2) {
        minuto_ref_2_dinamico = agora.minute() + 30;
        if (minuto_ref_2_dinamico >= 60) { minuto_ref_2_dinamico %= 60; hora_ref_2_dinamica = (agora.hour() + 1) % 24; }
      }
      if (eh_hora_ref_3) {
        minuto_ref_3_dinamico = agora.minute() + 30;
        if (minuto_ref_3_dinamico >= 60) { minuto_ref_3_dinamico %= 60; hora_ref_3_dinamica = (agora.hour() + 1) % 24; }
      }
    } 
    else {
      Serial.println("\n[ALARME] Hora da Racao!");
      digitalWrite(PINO_RELE, HIGH);
      agua_bombaLigada = false;
      agua_sistemaAtivo = false; 

      liberarRacao(pesoRacao);
      
      if (eh_hora_ref_1) { hora_ref_1_dinamica = HORA_REF_1_BASE; minuto_ref_1_dinamico = MINUTO_REF_1_BASE; }
      if (eh_hora_ref_2) { hora_ref_2_dinamica = HORA_REF_2_BASE; minuto_ref_2_dinamico = MINUTO_REF_2_BASE; }
      if (eh_hora_ref_3) { hora_ref_3_dinamica = HORA_REF_3_BASE; minuto_ref_3_dinamico = MINUTO_REF_3_BASE; }
      
      ultima_hora_servida = agora.hour(); 
    }
  }
}

// FUNÇÃO DE DOSAGEM PROPORCIONAL 
void liberarRacao(float peso_atual) {
  if (peso_atual < RACAO_MARGEM_AUSENTE) {
    Serial.println("[ERRO] Tigela de racao ausente! Cancelando refeicao.");
    return; 
  }

  Serial.println("*** INICIANDO DOSAGEM DE RACAO ***");
  float peso_inicio_refeicao = peso_atual; 
  int falhas_consecutivas = 0;
  const int MAX_FALHAS = 5; 
  float peso_anterior = peso_atual; 
  int ciclo = 1; 

  while (peso_atual < RACAO_ALVO_TOTAL && falhas_consecutivas < MAX_FALHAS) {
    float falta = RACAO_ALVO_TOTAL - peso_atual;
    int angulo_atual;
    int tempo_aberto;
    
    Serial.print("Ciclo "); Serial.print(ciclo);
    Serial.print(" | Peso: "); Serial.print(peso_atual, 1);
    Serial.print("g | Falta: "); Serial.print(falta, 1); Serial.print("g");

    if (falta > 15.0) {
      angulo_atual = POS_ABERTA_MAX; 
      tempo_aberto = 800;            
      Serial.println(" -> Modo: GROSSO");
    } else {
      angulo_atual = POS_ABERTA_FINA; 
      tempo_aberto = 500;             
      Serial.println(" -> Modo: FINO");
    }

    if (!servoMotor.attached()) servoMotor.attach(PINO_SERVO, 500, 2400);
    servoMotor.write(angulo_atual); 
    delay(tempo_aberto); 
    servoMotor.write(POS_FECHADA); 
    delay(500); 
    servoMotor.detach(); 
    
    delay(2000); 
    peso_atual = balancaRacao.get_units(5) * 1000; 
    
    float variacao_de_peso = peso_atual - peso_anterior;
    if (variacao_de_peso < 0.5) { 
      falhas_consecutivas++;
      Serial.print("   [!] Nenhum grao caiu! Falha ");
      Serial.print(falhas_consecutivas); Serial.print(" de "); Serial.println(MAX_FALHAS);
    } else {
      falhas_consecutivas = 0; 
    }
    peso_anterior = peso_atual; 
    ciclo++;
  }
  
  if (peso_atual >= RACAO_ALVO_TOTAL) {
    Serial.print("\n*** PORCAO DE RACAO CONCLUIDA! Peso final: ");
    Serial.print(peso_atual, 1); Serial.println(" g ***\n");
    float racao_adicionada = peso_atual - peso_inicio_refeicao;
    registrarEventoNaNuvem(2, racao_adicionada); 
  } else if (falhas_consecutivas >= MAX_FALHAS) {
    Serial.println("\n[ALERTA MAXIMO] A balanca parou de registrar entrada de racao!");
  }
  
  if (!servoMotor.attached()) servoMotor.attach(PINO_SERVO, 500, 2400);
  servoMotor.write(POS_FECHADA);
  delay(500);
  servoMotor.detach(); 
}

// ==========================================
// FUNÇÕES DE COMUNICAÇÃO 
// ==========================================

// 1. Envia apenas quando a máquina trabalha
void registrarEventoNaNuvem(int tipo, float quantidade_adicionada) {
  if (quantidade_adicionada < 0) quantidade_adicionada = 0;

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "http://api.thingspeak.com/update?api_key=" + apiKey;
    
    if (tipo == 1) {
      url += "&field1=" + String(quantidade_adicionada, 1);
      Serial.print("[NUVEM] Adicionado de AGUA: ");
    } else if (tipo == 2) {
      url += "&field2=" + String(quantidade_adicionada, 1);
      Serial.print("[NUVEM] Adicionado de RACAO: ");
    }
    Serial.print(quantidade_adicionada, 1);
    Serial.println("g. Enviando log...");

    http.begin(url);
    int httpCode = http.GET();
    
    if (httpCode > 0) {
      Serial.println("[NUVEM] ✅ Evento registrado no ThingSpeak!");
    } else {
      Serial.println("[NUVEM] ❌ Falha de comunicacao com o site. Acionando backup offline...");
      salvarOffline(tipo, quantidade_adicionada); 
    }
    http.end();
  } else {
    Serial.println("[NUVEM] ❌ Sem Wi-Fi no momento. Acionando backup offline...");
    salvarOffline(tipo, quantidade_adicionada); 
    WiFi.reconnect(); 
  }
}

// 2. Envia a situação do prato a cada 15 min 
void enviarStatusTempoReal(float pAgua, float pRacao) {
  float agua_liq = pAgua - AGUA_PESO_TIGELA;
  float racao_liq = pRacao - RACAO_PESO_TIGELA;
  
  if(agua_liq < 0) agua_liq = 0;
  if(racao_liq < 0) racao_liq = 0;

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "http://api.thingspeak.com/update?api_key=" + apiKey + 
                 "&field3=" + String(agua_liq, 1) + 
                 "&field4=" + String(racao_liq, 1);
    
    http.begin(url);
    int httpCode = http.GET();
    
    if (httpCode > 0) {
      Serial.println("[TELEMETRIA] 📡 Status atualizado! App sincronizado.");
    }
    http.end();
  }
}

// 3. Salva no disco interno do ESP32 se faltar rede
void salvarOffline(int tipo, float quantidade) {
  DateTime agora = rtc.now(); 
  File file = LittleFS.open("/backup.csv", FILE_APPEND);
  if (!file) return;
  
  char linhaCSV[50];
  sprintf(linhaCSV, "%02d/%02d/%04d,%02d:%02d:%02d,%s,%.1f\n", 
          agora.day(), agora.month(), agora.year(),
          agora.hour(), agora.minute(), agora.second(),
          (tipo == 1) ? "AGUA" : "RACAO", 
          quantidade);
          
  file.print(linhaCSV); 
  file.close();        
  Serial.print("Salvo com sucesso na memoria interna: ");
  Serial.print(linhaCSV);
}
