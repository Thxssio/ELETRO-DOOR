// =================================================================
// FUNÇÕES DE FEEDBACK (BUZZER)
// =================================================================
void apito_sucesso() { tone(pino_buzzer, 1200, 150); }
void apito_erro() { tone(pino_buzzer, 300, 500); }
void apito_removido_sd() { tone(pino_buzzer, 700, 100); vTaskDelay(pdMS_TO_TICKS(150)); tone(pino_buzzer, 700, 100); }
void apito_acesso_permitido() { tone(pino_buzzer, 1000, 100); vTaskDelay(pdMS_TO_TICKS(120)); tone(pino_buzzer, 1500, 150); }
void apito_acesso_negado() { tone(pino_buzzer, 200, 800); }

// =================================================================
// GERENCIAMENTO DA EEPROM (CORRIGIDO PARA EVITAR DEADLOCK)
// =================================================================

void _salvarDadosNaEEPROM_unsafe() {
  EEPROM.put(EEPROM_ADDR, eepromData);
  EEPROM.commit();
}


void salvarDadosNaEEPROM() {
  if (xSemaphoreTake(eepromMutex, portMAX_DELAY) == pdTRUE) {
    _salvarDadosNaEEPROM_unsafe();
    xSemaphoreGive(eepromMutex);
  }
}

void formatarEEPROM() {
  if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
    Serial.println("ASSINATURA INVALIDA! Formatando EEPROM...");
    xSemaphoreGive(serialMutex);
  }
  if (xSemaphoreTake(eepromMutex, portMAX_DELAY) == pdTRUE) {
    strcpy(eepromData.signature, SIGNATURE);
    for (int i = 0; i < MAX_TAGS; i++) {
      eepromData.tags[i].ativa = false;
      strcpy(eepromData.tags[i].uid, "");
    }
    _salvarDadosNaEEPROM_unsafe(); 
    xSemaphoreGive(eepromMutex);
  }
}

void setupEEPROM() {
  EEPROM.begin(sizeof(EepromData));
  if (xSemaphoreTake(eepromMutex, portMAX_DELAY) == pdTRUE) {
    EEPROM.get(EEPROM_ADDR, eepromData);
    xSemaphoreGive(eepromMutex);
  }
  if (strcmp(eepromData.signature, SIGNATURE) != 0) {
    formatarEEPROM();
  } else {
    if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
      Serial.println("EEPROM ja formatada. Tags carregadas.");
      xSemaphoreGive(serialMutex);
    }
  }
}

bool cadastrarNovaTag(String uid) {
  bool sucesso = false;
  if (xSemaphoreTake(eepromMutex, portMAX_DELAY) == pdTRUE) {
    bool jaExiste = false;
    for(int i = 0; i < MAX_TAGS; i++){
      if(eepromData.tags[i].ativa && uid.equals(eepromData.tags[i].uid)){
        jaExiste = true;
        break;
      }
    }

    if(!jaExiste){
      for (int i = 0; i < MAX_TAGS; i++) {
        if (!eepromData.tags[i].ativa) {
          uid.toCharArray(eepromData.tags[i].uid, TAMANHO_UID);
          eepromData.tags[i].ativa = true;
          _salvarDadosNaEEPROM_unsafe(); 
          sucesso = true;
          break;
        }
      }
    }
    xSemaphoreGive(eepromMutex);
  }
  return sucesso;
}

void apagarTag(String uid) {
  if (xSemaphoreTake(eepromMutex, portMAX_DELAY) == pdTRUE) {
    for (int i = 0; i < MAX_TAGS; i++) {
      if (eepromData.tags[i].ativa && uid.equals(eepromData.tags[i].uid)) {
        eepromData.tags[i].ativa = false;
        break; 
      }
    }
    _salvarDadosNaEEPROM_unsafe();
    xSemaphoreGive(eepromMutex);
  }
}

bool verificarAcesso(String uid) {
  bool acesso = false;
  if (xSemaphoreTake(eepromMutex, portMAX_DELAY) == pdTRUE) {
    for (int i = 0; i < MAX_TAGS; i++) {
      if (eepromData.tags[i].ativa && uid.equals(eepromData.tags[i].uid)) {
        acesso = true;
        break;
      }
    }
    xSemaphoreGive(eepromMutex);
  }
  return acesso;
}

// =================================================================
// LOG NO CARTÃO SD (SIMPLIFICADO)
// =================================================================
void registrarLog(String uid, bool acessoPermitido) {
  if (cartao_sd_presente) {
    if (xSemaphoreTake(spiMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
      File logFile = SD.open("/acessos.log", FILE_APPEND);
      if (logFile) {
        logFile.printf("%lus - UID: %s | Acesso: %s\n", millis() / 1000, uid.c_str(), acessoPermitido ? "PERMITIDO" : "NEGADO");
        logFile.close();
      }
      xSemaphoreGive(spiMutex);
    }
  }
}