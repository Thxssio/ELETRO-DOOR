/*
 * =================================================================
 * PROJETO TRANCA ELETRÔNICA - v5.1 
 * =================================================================
 * Autor: Arthur, Claudenir e Thassio
 *
 * Versão Final Estável:
 * - Arquitetura RTOS Híbrida 
 * - Foco no controle de acesso por UID.
 */

// =================================================================
// BIBLIOTECAS
// =================================================================
#include <SPI.h>
#include <SD.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <LittleFS.h>

// =================================================================
// DEFINIÇÕES E VARIÁVEIS GLOBAIS
// =================================================================
#define MAX_TAGS 20
#define TAMANHO_UID 20
#define EEPROM_ADDR 0
#define SIGNATURE "T3A"

// Pinos
const int pino_cs_sd = 5;
const int pino_sda_rfid = 21;
const int pino_rst_rfid = 22;
const int pino_buzzer = 27;
const int pino_led_verde = 33;
const int pino_led_amarelo = 25;
const int pino_led_vermelho = 26;

const int pino_stby = 16;
const int pino_pwma = 15;
const int pino_ain1 = 2;
const int pino_ain2 = 4;

// Estruturas de Dados
struct TagAutorizada {
  bool ativa;
  char uid[TAMANHO_UID];
};
struct EepromData {
  char signature[4];
  TagAutorizada tags[MAX_TAGS];
};
char uid_buffer[TAMANHO_UID];

// Objetos e Variáveis Globais
EepromData eepromData;
WebServer server(80);
MFRC522 rfid(pino_sda_rfid, pino_rst_rfid);

const char* ap_ssid = "ELETRO-DOOR";
const char* ap_password = "batidadecocosenna";
String master_user = "admin";
String master_pass = "admin";
bool cartao_sd_presente = false;
bool logged_in = false;
volatile bool modo_cadastro = false;

// Switchs de sensoreamento
const int pino_sensor_porta = 12;   // D12 - Verifica se a porta está aberta ou fechada
const int pino_sensor_tranca = 13;  // D13 - Verifica se a fechadura está trancada ou destrancada

volatile bool porta_esta_aberta = true;
volatile bool tranca_esta_fechada = false;

// RTOS
TaskHandle_t TaskHandle_WebServer;
TaskHandle_t TaskHandle_Processamento;
TaskHandle_t TaskHandle_SD_Check;
TaskHandle_t TaskHandle_MonitorSensores;

QueueHandle_t rfidQueue;
SemaphoreHandle_t eepromMutex;
SemaphoreHandle_t serialMutex;
SemaphoreHandle_t spiMutex;

// =================================================================
// SETUP E LOOP PRINCIPAL
// =================================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n\nIniciando Sistema de Tranca v5.0 (UI Minimalista)...");

  pinMode(pino_buzzer, OUTPUT);
  pinMode(pino_led_verde, OUTPUT);
  pinMode(pino_led_amarelo, OUTPUT);
  pinMode(pino_led_vermelho, OUTPUT);

  pinMode(pino_sensor_porta, INPUT_PULLUP);
  pinMode(pino_sensor_tranca, INPUT_PULLUP);

  pinMode(pino_stby, OUTPUT);
  pinMode(pino_pwma, OUTPUT);
  pinMode(pino_ain1, OUTPUT);
  pinMode(pino_ain2, OUTPUT);

  digitalWrite(pino_stby, HIGH);

  eepromMutex = xSemaphoreCreateMutex();
  serialMutex = xSemaphoreCreateMutex();
  spiMutex = xSemaphoreCreateMutex();

  rfidQueue = xQueueCreate(5, sizeof(char) * TAMANHO_UID);

  if (!LittleFS.begin(true)) {
    Serial.println("Erro ao montar LittleFS!");
    return;
  }

  SPI.begin();
  rfid.PCD_Init();
  setupEEPROM();

  if (xSemaphoreTake(spiMutex, portMAX_DELAY) == pdTRUE) {
    if (SD.begin(pino_cs_sd)) {
      cartao_sd_presente = true;
      Serial.println("Cartao SD inicializado.");
    } else {
      Serial.println("Falha ao inicializar o Cartao SD.");
    }
    xSemaphoreGive(spiMutex);
  }

  WiFi.softAP(ap_ssid, ap_password);
  Serial.print("Ponto de Acesso: ");
  Serial.println(ap_ssid);
  Serial.print("IP para acesso: http://");
  Serial.println(WiFi.softAPIP());

  xTaskCreatePinnedToCore(
    TaskWebServer,          // Função da Tarefa
    "Task Web Server",      // Nome
    8192,                   // Tamanho da Pilha (a ser otimizado)
    NULL,                   // Parâmetros
    2,                      // Prioridade (Baixa)
    &TaskHandle_WebServer,  // Handle
    0                       // <<< FIXADO NO CORE 0
  );

  xTaskCreatePinnedToCore(
    TaskMonitorSensores,          // Função da Tarefa
    "Task Sensores",              // Nome
    8192,                         // Tamanho da Pilha (a ser otimizado)
    NULL,                         // Parâmetros
    3,                            // Prioridade (Alta, para garantir a coleta de dados)
    &TaskHandle_MonitorSensores,  // Handle
    1                             // <<< FIXADO NO CORE 1
  );

  xTaskCreatePinnedToCore(
    TaskProcessamento,          // Função da Tarefa
    "Task Processamento",       // Nome
    8192,                       // Tamanho da Pilha (a ser otimizado)
    NULL,                       // Parâmetros
    3,                          // Prioridade (Alta, processa os dados dos sensores)
    &TaskHandle_Processamento,  // Handle
    1                           // <<< FIXADO NO CORE 1
  );

  xTaskCreatePinnedToCore(
    TaskSDCheck,           // Função da Tarefa
    "Task SD Check",       // Nome
    4096,                  // Tamanho da Pilha (a ser otimizado)
    NULL,                  // Parâmetros
    2,                     // Prioridade (Media)
    &TaskHandle_SD_Check,  // Handle
    0                      // <<< FIXADO NO CORE 0
  );

  Serial.println("Setup finalizado. Sistema principal rodando no loop().");
  fecharTranca();
}

void loop() {
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    String uid_lido = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
      uid_lido.concat(String(rfid.uid.uidByte[i] < 0x10 ? " 0" : " "));
      uid_lido.concat(String(rfid.uid.uidByte[i], HEX));
    }
    uid_lido.toUpperCase();
    uid_lido.trim();
    uid_lido.toCharArray(uid_buffer, TAMANHO_UID);
    xQueueSend(rfidQueue, &uid_buffer, pdMS_TO_TICKS(10));
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }
  delay(10);
}