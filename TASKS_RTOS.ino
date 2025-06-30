// =================================================================
// TASKS DO RTOS
// =================================================================

void TaskWebServer(void *pvParameters) {
  (void) pvParameters;
  
  server.on("/", HTTP_GET, handleRoot);
  server.on("/pico.min.css", HTTP_GET, handleCSS); 
  server.on("/login", HTTP_POST, handleLogin);
  server.on("/logout", HTTP_GET, handleLogout);
  server.on("/iniciar-cadastro", HTTP_GET, handleIniciarCadastro);
  server.on("/apagar", HTTP_GET, handleApagar);
  server.on("/formatar", HTTP_GET, handleFormatar);
  server.onNotFound(handleNotFound);
  
  server.begin();
  
  for (;;) {
    server.handleClient();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void TaskSDCheck(void *pvParameters) {
  (void) pvParameters;
  for (;;) {
    if (xSemaphoreTake(spiMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
      if (cartao_sd_presente) {
        File root = SD.open("/");
        if (!root) {
          cartao_sd_presente = false;
          if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) { Serial.println("<< Cartao SD Removido! >>"); xSemaphoreGive(serialMutex); }
          apito_removido_sd();
          SD.end();
        } else {
          root.close();
        }
      } else {
        if (SD.begin(pino_cs_sd)) {
          cartao_sd_presente = true;
          if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) { Serial.println(">> Cartao SD Inserido! <<"); xSemaphoreGive(serialMutex); }
          apito_sucesso();
        }
      }
      xSemaphoreGive(spiMutex);
    }
    vTaskDelay(pdMS_TO_TICKS(2500));
  }
}

void TaskProcessamento(void *pvParameters) {
  (void) pvParameters;
  char uid_recebido[TAMANHO_UID];
  for (;;) {
    if (xQueueReceive(rfidQueue, &uid_recebido, portMAX_DELAY) == pdPASS) {
      String uid_str(uid_recebido);
      
      if (modo_cadastro) {
        digitalWrite(pino_led_amarelo, LOW);
        if (cadastrarNovaTag(uid_str)) {
          if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) { Serial.println("TAG CADASTRADA: " + uid_str); xSemaphoreGive(serialMutex); }
          apito_sucesso();
        } else {
          if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) { Serial.println("MEMORIA CHEIA!"); xSemaphoreGive(serialMutex); }
          apito_erro();
        }
        modo_cadastro = false;
        
      } else {
        bool acesso = verificarAcesso(uid_str);
        registrarLog(uid_str, acesso);
        
        if (acesso) {
          if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) { Serial.println("ACESSO PERMITIDO para " + uid_str); xSemaphoreGive(serialMutex); }
          digitalWrite(pino_led_verde, HIGH);
          apito_acesso_permitido();
        } else {
          if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) { Serial.println("ACESSO NEGADO para " + uid_str); xSemaphoreGive(serialMutex); }
          digitalWrite(pino_led_vermelho, HIGH);
          apito_acesso_negado();
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
        digitalWrite(pino_led_verde, LOW);
        digitalWrite(pino_led_vermelho, LOW);
      }
    }
  }
}


void TaskMonitorSensores(void *pvParameters) {
  (void) pvParameters;
  bool estado_anterior_porta = true;
  bool estado_anterior_tranca = false;

  if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
    Serial.println("Task de Monitoramento de Sensores iniciada.");
    xSemaphoreGive(serialMutex);
  }

  for (;;) {
    bool porta_agora_aberta = (digitalRead(pino_sensor_porta) == HIGH);
    bool tranca_agora_fechada = (digitalRead(pino_sensor_tranca) == LOW);

    porta_esta_aberta = porta_agora_aberta;
    tranca_esta_fechada = tranca_agora_fechada;

    if (porta_agora_aberta && tranca_agora_fechada) {
      if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
        Serial.println("!!! ALARME: PORTA ARROMBADA !!!");
        xSemaphoreGive(serialMutex);
      }
      digitalWrite(pino_led_vermelho, !digitalRead(pino_led_vermelho)); // Pisca o LED vermelho
      tone(pino_buzzer, 2000, 100);
    } else {
      // Desliga o pisca-alerta se a condição de alarme não for mais verdadeira
       digitalWrite(pino_led_vermelho, LOW);
    }

    if(porta_agora_aberta != estado_anterior_porta) {
      if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
        Serial.print("Status Porta: ");
        Serial.println(porta_agora_aberta ? "ABERTA" : "FECHADA");
        xSemaphoreGive(serialMutex);
      }
      estado_anterior_porta = porta_agora_aberta;
    }

    if(tranca_agora_fechada != estado_anterior_tranca) {
      if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
        Serial.print("Status Tranca: ");
        Serial.println(tranca_agora_fechada ? "TRANCADA" : "DESTRANCADA");
        xSemaphoreGive(serialMutex);
      }
      estado_anterior_tranca = tranca_agora_fechada;
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}