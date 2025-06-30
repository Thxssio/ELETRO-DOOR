// =================================================================
// SERVIDOR WEB (HTML SEMÂNTICO com PICO.CSS)
// =================================================================

// Handler para servir o arquivo CSS a partir da memória LittleFS
void handleCSS() {
  File file = LittleFS.open("/pico.min.css", "r");
  if (file) {
    server.streamFile(file, "text/css");
    file.close();
  } else {
    server.send(404, "text/plain", "404: CSS Nao Encontrado");
  }
}

String getPaginaLogin() {
  return R"rawliteral(
<!DOCTYPE html>
<html lang="pt-br" data-theme="dark">
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link rel="stylesheet" href="pico.min.css">
    <title>Login - Tranca RFID</title>
  </head>
  <body>
    <main class="container" style="max-width: 500px;">
      <article>
        <h1 align="center">Tranca RFID</h1>
        <form action='/login' method='POST'>
          <label for='username'>Usuário</label>
          <input type='text' id='username' name='username' required autocomplete='username'>
          <label for='password'>Senha</label>
          <input type='password' id='password' name='password' required autocomplete='current-password'>
          <button type='submit'>Entrar</button>
        </form>
      </article>
    </main>
  </body>
</html>)rawliteral";
}

String getPainelAdmin() {
  String painel = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-br" data-theme="dark">
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    )rawliteral";
    
  // NOVO: Adiciona um script para recarregar a página a cada 5 segundos
  painel += "<meta http-equiv='refresh' content='5'>";
  
  painel += R"rawliteral(
    <link rel="stylesheet" href="pico.min.css">
    <title>Painel Admin</title>
  </head>
  <body>
    <main class="container">
      <hgroup>
        <h1>Painel de Controle</h1>
        <h2>Gerenciamento da Tranca RFID</h2>
      </hgroup>

      <article>
        <hgroup>
          <h2>Status do Sistema</h2>
          <h3>Monitoramento em Tempo Real</h3>
        </hgroup>
        <table>
          <tbody>
            <tr>
              <td><strong>Porta</strong></td>
              <td style="text-align: right;">)rawliteral";
  
  // Lógica para mostrar o status da porta com cores
  if (porta_esta_aberta) {
    painel += "<span style='color: #ffb400; font-weight: bold;'>ABERTA</span>";
  } else {
    painel += "<span style='color: #34eb52; font-weight: bold;'>Fechada</span>";
  }
  
  painel += R"rawliteral(</td>
            </tr>
            <tr>
              <td><strong>Tranca</strong></td>
              <td style="text-align: right;">)rawliteral";

  // Lógica para mostrar o status da tranca com cores
  if (tranca_esta_fechada) {
    painel += "<span style='color: #ff3b30; font-weight: bold;'>TRANCADA</span>";
  } else {
    painel += "<span style='color: #34eb52; font-weight: bold;'>Destrancada</span>";
  }

  painel += R"rawliteral(</td>
            </tr>
          </tbody>
        </table>
      </article>

      <a href='/iniciar-cadastro' role='button'>Cadastrar Nova Tag</a>
      
      <figure>
        <table>
          <thead>
            <tr><th scope="col">UID da Tag</th><th scope="col">Ação</th></tr>
          </thead>
          <tbody>)rawliteral";

  if (xSemaphoreTake(eepromMutex, portMAX_DELAY) == pdTRUE) {
    int tags_encontradas = 0;
    for (int i = 0; i < MAX_TAGS; i++) {
      if (eepromData.tags[i].ativa) {
        String uidAtual = String(eepromData.tags[i].uid);
        painel += "<tr><td><code>" + uidAtual + "</code></td><td><a href='/apagar?uid=" + uidAtual + "' class='secondary'>Apagar</a></td></tr>";
        tags_encontradas++;
      }
    }
    xSemaphoreGive(eepromMutex);
    if (tags_encontradas == 0) {
        painel += "<tr><td colspan='2' style='text-align:center;'>Nenhuma tag cadastrada.</td></tr>";
    }
  }
  
  painel += R"rawliteral(
          </tbody>
        </table>
      </figure>
      <div class="grid">
        <a href='/formatar' role='button' class='secondary' onclick='return confirm("ATENCAO! Deseja apagar TODAS as tags?");'>Formatar Memória</a>
        <a href='/logout' role='button' class='contrast'>Sair do Painel</a>
      </div>
    </main>
  </body>
</html>)rawliteral";
  return painel;
}

void handleRoot() { if(logged_in) { server.send(200, "text/html; charset=UTF-8", getPainelAdmin()); } else { server.send(200, "text/html; charset=UTF-8", getPaginaLogin()); } }
void handleLogin() { if (server.hasArg("username") && server.arg("username") == master_user && server.hasArg("password") && server.arg("password") == master_pass) { logged_in = true; server.sendHeader("Location", "/", true); server.send(302, "text/plain", ""); } else { server.send(401, "text/html; charset=UTF-8", "Login Falhou! Tente novamente.<meta http-equiv='refresh' content='2;url=/' />"); } }
void handleLogout() { logged_in = false; server.sendHeader("Location", "/", true); server.send(302, "text/plain", ""); }
void handleIniciarCadastro() {
  if(!logged_in) { server.send(401, "text/plain", "Nao autorizado"); return; }
  modo_cadastro = true;
  digitalWrite(pino_led_amarelo, HIGH);
  String pagina_cadastro = R"rawliteral(<!DOCTYPE html><html lang='pt-br' data-theme='dark'><head><meta http-equiv='refresh' content='15;url=/'><meta charset='UTF-8'><title>Cadastrando...</title><link rel="stylesheet" href="pico.min.css"></head><body><main class='container' style='text-align:center;'><article><h1>Modo de Cadastro Ativado</h1><p>Aproxime a nova tag do leitor...</p><progress></progress><p><small>(Esta página irá expirar e voltar ao painel em 15 segundos)</small></p></article></main></body></html>)rawliteral";
  server.send(200, "text/html; charset=UTF-8", pagina_cadastro);
}
void handleApagar() { if(!logged_in) { server.send(401, "text/plain", "Nao autorizado"); return; } if (server.hasArg("uid")) { apagarTag(server.arg("uid")); } server.sendHeader("Location", "/", true); server.send(302, "text/plain", ""); }
void handleFormatar() {
  if(!logged_in) { server.send(401, "text/plain", "Nao autorizado"); return; }
  formatarEEPROM();
  String pagina_formatada = R"rawliteral(<!DOCTYPE html><html lang='pt-br' data-theme='dark'><head><meta http-equiv='refresh' content='3;url=/'><meta charset='UTF-8'><title>Memoria Formatada</title><link rel="stylesheet" href="pico.min.css"></head><body><main class='container' style='text-align:center;'><article><h1>Memória Formatada!</h1><p>Todas as tags foram apagadas com sucesso. Redirecionando...</p></article></main></body></html>)rawliteral";
  server.send(200, "text/html; charset=UTF-8", pagina_formatada);
}
void handleNotFound() { server.send(404, "text/plain", "404: Nao encontrado"); }