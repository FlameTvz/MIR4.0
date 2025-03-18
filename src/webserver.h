String paginaPrincipal(int qtdRele, int releNumber, int tempoPulsoGlobal, int tempoEntradaGlobal, const String &horaAtivacaoGlobal, const String &horaDesativacaoGlobal);




void configurarWebServer()
{
    // Rota raiz (carrega a página principal para, por exemplo, o relé 1 - ou você pode ajustar
    // para exibir algum relé "padrão". Caso queira, pode configurar para não passar parâmetros
    // e usar valores default.)
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        // Exemplo chamando a paginaPrincipal para relé 1 (releNumber=1),
        // ou ajuste conforme sua lógica.
        int releNumber = 1;
        int tempoPulso = temposPulso[releNumber - 1];
        int tEntrada = temposEntrada[releNumber - 1];
        String horaAtivacao = rele[releNumber - 1].horaAtivacao;
        String horaDesativacao = rele[releNumber - 1].horaDesativacao;

        request->send(200, "text/html",
            paginaPrincipal(qtdRele, releNumber, tempoPulso, tEntrada, horaAtivacao, horaDesativacao)); });

    // Liga/Desliga
    server.on("/controle", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (request->hasParam("rele") && request->hasParam("acao")) {
            int releIdx = request->getParam("rele")->value().toInt() - 1;
            String acao = request->getParam("acao")->value();
            if (releIdx >= 0 && releIdx < qtdRele) {
                if (acao == "ligar") {
                    digitalWrite(rele[releIdx].pino, HIGH);
                    rele[releIdx].status = 1; // Atualiza status
                } else if (acao == "desligar") {
                    digitalWrite(rele[releIdx].pino, LOW);
                    rele[releIdx].status = 0; // Atualiza status
                }
                request->send(200, "text/plain", "OK");
            } else {
                request->send(400, "text/plain", "Relé inválido");
            }
        } else {
            request->send(400, "text/plain", "Parâmetros inválidos");
        } });

    // Exemplo de rota POST para configurar apenas horaAtivacao/horaDesativacao (se ainda existir)
    server.on("/configurar", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        if (request->hasParam("rele", true) && 
            request->hasParam("horaAtivacao", true) && 
            request->hasParam("horaDesativacao", true))
        {
            int releIdx = request->getParam("rele", true)->value().toInt() - 1;
            if (releIdx >= 0 && releIdx < 5) {
                rele[releIdx].horaAtivacao = request->getParam("horaAtivacao", true)->value();
                rele[releIdx].horaDesativacao = request->getParam("horaDesativacao", true)->value();
                request->send(200, "text/plain", "Horários configurados");
                request->redirect("/");
            } else {
                request->send(400, "text/plain", "Relé inválido");
            }
        } else {
            request->send(400, "text/plain", "Parâmetros inválidos");
        } });

    // Switch (acionamento temporário)
    server.on("/switch", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (request->hasParam("rele"))
        {
            int releIdx = request->getParam("rele")->value().toInt() - 1;
            if (releIdx >= 0 && releIdx < qtdRele)
            {
                if (!switchAtual.ativo)
                {
                    digitalWrite(rele[releIdx].pino, HIGH);
                    rele[releIdx].status = 1;
                    switchAtual.ativo = true;
                    switchAtual.inicio = millis();
                    switchAtual.releIdx = releIdx;
                    request->send(200, "text/plain", "Switch ativado!");
                }
                else
                {
                    request->send(400, "text/plain", "Outro switch está ativo.");
                }
            }
            else
            {
                request->send(400, "text/plain", "Relé inválido.");
            }
        }
        else
        {
            request->send(400, "text/plain", "Parâmetro 'rele' ausente.");
        } });

    // Retorno do status (JSON)
    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        String response = "{ \"reles\": [";
        for (int i = 0; i < qtdRele; i++) {
            response += "{";
            response += "\"status\": " + String(rele[i].status);
            response += "}";
            if (i < qtdRele - 1) response += ",";
        }
        response += "] }";
        request->send(200, "application/json", response); });

    // Ajuste de tempos via POST (pulso/entrada)
    server.on("/configurartemporele", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        if (request->hasParam("rele", true) && 
            request->hasParam("tempoPulso", true) && 
            request->hasParam("tempoEntrada", true))
        {
            int releIdx = request->getParam("rele", true)->value().toInt() - 1;
            int novoTempoPulso = request->getParam("tempoPulso", true)->value().toInt();
            int novoTempoEntrada = request->getParam("tempoEntrada", true)->value().toInt();

            if (releIdx >= 0 && releIdx < 5) {
                temposPulso[releIdx] = novoTempoPulso;
                tempoEntrada = novoTempoEntrada; // Se for um único global, mantenha. Senão ajuste.
                Serial.printf("Tempo do pulso do relé %d: %d ms\n", releIdx + 1, novoTempoPulso);
                Serial.printf("Tempo de entrada: %d ms\n", novoTempoEntrada);
                request->send(200, "text/plain", "Tempos atualizados");
                request->redirect("/");
            } else {
                request->send(400, "text/plain", "Relé inválido");
            }
        } else {
            request->send(400, "text/plain", "Parâmetros inválidos");
        } });

    server.on("/salvarnome", HTTP_GET, [](AsyncWebServerRequest *request)
              {
    if (request->hasParam("rele") && request->hasParam("nome"))
    {
        int releIdx = request->getParam("rele")->value().toInt() - 1;
        String novoNome = request->getParam("nome")->value();

        if (releIdx >= 0 && releIdx < qtdRele)
        {
            // Salva no struct
            rele[releIdx].nome = novoNome;
            Serial.printf("Nome do Relé %d alterado para: %s\n", releIdx + 1, novoNome.c_str());
            request->send(200, "text/plain", "OK");
        }
        else
        {
            request->send(400, "text/plain", "Relé inválido.");
        }
    }
    else
    {
        request->send(400, "text/plain", "Parâmetros ausentes.");
    } });

    // Exemplo de configuração de “tempo de entrada” separado (se você ainda precisar)
    server.on("/configurarentrada", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        if (request->hasParam("tempo", true)) {
            String tempoStr = request->getParam("tempo", true)->value();
            int novoTempo = tempoStr.toInt();
            if (novoTempo > 0) {
                tempoEntrada = novoTempo;
                Serial.println("Tempo da entrada atualizado para: " + String(tempoEntrada) + " ms");
                request->redirect("/");
            } else {
                request->send(400, "text/plain", "Valor inválido para o tempo do pulso da entrada");
            }
        } else {
            request->send(400, "text/plain", "Parâmetro 'tempo' ausente");
        } });

    // IMPORTANTE:
    // A rota /configurartempo AGORA usa a paginaPrincipal(...) no lugar de paginaConfiguracaoRele(...)
    // para exibir a mesma página unificada (sem excluir a rota ou alterar nomes).
    server.on("/configurartempo", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (request->hasParam("rele"))
        {
            int releIdx = request->getParam("rele")->value().toInt() - 1;

            // Busca valores do relé específico
            int tempoPulso = temposPulso[releIdx];
            int tEntrada   = temposEntrada[releIdx];
            String horaAtivacao   = rele[releIdx].horaAtivacao;
            String horaDesativacao= rele[releIdx].horaDesativacao;

            // Agora chamamos a sua função unificada paginaPrincipal(...)
            // que deve ter a assinatura que inclua estes parâmetros.
            request->send(200, "text/html",
                paginaPrincipal(qtdRele, releIdx + 1, tempoPulso, tEntrada, horaAtivacao, horaDesativacao));
        }
        else
        {
            request->send(400, "text/plain", "Parâmetro 'rele' ausente.");
        } });

    // Salvar config (antes era usado pelo form da página separada, mas agora
    // ele continua funcionando normalmente, pois a mesma <form> está dentro da paginaPrincipal).
    server.on("/salvarconfig", HTTP_POST, [](AsyncWebServerRequest *request)
              {
    if (request->hasParam("rele", true))
    {
        int releIdx = request->getParam("rele", true)->value().toInt() - 1;
        if (releIdx >= 0 && releIdx < qtdRele)
        {
            // tempoPulso
            if (request->hasParam("tempoPulso", true)) {
                int tPulso = request->getParam("tempoPulso", true)->value().toInt();
                temposPulso[releIdx] = tPulso;
            }

            // tempoEntrada
            if (request->hasParam("tempoEntrada", true)) {
                int tEntrada = request->getParam("tempoEntrada", true)->value().toInt();
                temposEntrada[releIdx] = tEntrada;
            }

            // modoRele (se você tiver)
            if (request->hasParam("modoRele", true)) {
                int modo = request->getParam("modoRele", true)->value().toInt();
                modoAcionamento[releIdx] = modo;
            }

            // Agora, SALVAMOS O NOME
            if (request->hasParam("nomeRele", true)) {
                String nome = request->getParam("nomeRele", true)->value();
                rele[releIdx].nome = nome;
            }

            // horaAtivacao / horaDesativacao (opcional)
            if (request->hasParam("horaAtivacao", true)) {
                rele[releIdx].horaAtivacao = request->getParam("horaAtivacao", true)->value();
            }
            if (request->hasParam("horaDesativacao", true)) {
                rele[releIdx].horaDesativacao = request->getParam("horaDesativacao", true)->value();
            }
            
            salvarConfiguracoes();
            request->redirect("/");
        }
        else
        {
            request->send(400, "text/plain", "Relé inválido.");
        }
    }
    else
    {
        request->send(400, "text/plain", "Parâmetro 'rele' ausente.");
    } });
    ;

    // Inicia o servidor
    server.begin();
} 
    
    String paginaPrincipal(int qtdRele, int releNumber, int tempoPulsoGlobal, int tempoEntradaGlobal, const String &horaAtivacaoGlobal, const String &horaDesativacaoGlobal)
    {
        // HTML completo, unificando página de controle + formulários
        String html = R"rawliteral(
    <!DOCTYPE html>
    <html lang="pt-br">
    <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>Controle de Relés</title>
        <link rel="preconnect" href="https://fonts.gstatic.com" />
        <link href="https://fonts.googleapis.com/css2?family=Poppins:wght@300;500;700&display=swap" rel="stylesheet" />
        <style>
           <!DOCTYPE html>
    
           /* =====================================================================
              CSS original da página principal + página de configuração
              (NENHUM trecho foi removido ou renomeado, apenas mantido integralmente)
           ===================================================================== */
           * {
                margin: 0;
                padding: 0;
                box-sizing: border-box;
            }
    
            body {
                font-family: "Poppins", Arial, sans-serif;
                background: linear-gradient(120deg, #fdfbfb 0%, #ebedee 100%);
                color: #333;
                margin: 0;
                text-align: center;
            }
    
            header {
                background-color: #34495e;
                padding: 20px;
                box-shadow: 0 2px 5px rgba(0, 0, 0, 0.2);
            }
    
            header h1 {
                color: #fff;
                font-weight: 700;
                font-size: 2.2rem;
            }
    
            #reles-container {
                display: flex;
                flex-wrap: nowrap;
                overflow-x: auto;
                gap: 15px;
                padding: 30px;
                margin: 0 auto;
                max-width: 95%;
            }
    
            .rele-card {
                flex: 0 0 auto;
                width: 270px;
                min-height: 180px;
                padding: 20px;
                border-radius: 15px;
                background: #fff;
                box-shadow: 0 3px 10px rgba(0, 0, 0, 0.1);
                text-align: center;
                transition: transform 0.2s ease, box-shadow 0.2s ease;
                
                display: flex;
                flex-direction: column;
                justify-content: flex-start; /* Ajuste para acomodar formulário abaixo */
                align-items: center;
            }
    
            .rele-card:hover {
                transform: translateY(-5px);
                box-shadow: 0 10px 15px rgba(0, 0, 0, 0.15);
            }
    
            /* Título do card */
            .rele-card h2 {
                margin: 10px 0;
                font-size: 1.2rem;
                color: #007BFF;
            }
    
            /* Estado do relé (Ligado/Desligado) */
            .estado {
                margin: 10px 0;
                display: inline-block;
                padding: 8px 16px;
                border-radius: 30px;
                font-weight: 500;
                font-size: 1rem;
                min-width: 90px;
                text-align: center;
                box-shadow: 0 3px 6px rgba(0, 0, 0, 0.1);
            }
    
            .estado.ligado {
                background: #27ae60;
                color: #fff;
            }
            .estado.desligado {
                background: #e74c3c;
                color: #fff;
            }
    
            .botoes {
                display: flex;
                justify-content: space-around;
                align-items: center;
                gap: 10px;
                margin-top: 15px;
                width: 100%;
            }
            button {
                flex: 1;
                padding: 10px;
                border: none;
                border-radius: 5px;
                color: #fdfbfb;
                font-size: 0.9rem;
                font-weight: 500;
                cursor: pointer;
                transition: background 0.2s, transform 0.2s, box-shadow 0.2s;
                outline: none;
            }
            button:hover {
                transform: translateY(-2px);
                box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1);
            }
            .btn-liga-desliga {
                background: #2c3e50;
            }
            .btn-liga-desliga:hover {
                background: #2c3e50;
            }
            .btn-switch {
                color: #fff; 
                background: #2c3e50;
            }
            .btn-switch:hover {
                background: #2c3e50;
            }
    
            @media (max-width: 1200px) {
                #reles-container {
                    max-width: 100%;
                    padding: 20px;
                }
                .rele-card {
                    width: calc(25% - 15px);
                }
            }
            @media (max-width: 992px) {
                .rele-card {
                    width: calc(33.33% - 15px);
                }
            }
            @media (max-width: 768px) {
                #reles-container {
                    flex-wrap: wrap;
                    justify-content: center;
                    overflow-x: visible;
                }
                .rele-card {
                    width: calc(50% - 15px);
                    margin: 10px auto;
                }
            }
            @media (max-width: 576px) {
                .rele-card {
                    width: calc(100% - 20px);
                    max-width: 350px;
                    margin: 10px auto;
                }
            }
    
            /* ====================
               CSS da antiga "paginaConfiguracaoRele" 
               Mantemos o que já existia
            ==================== */
            .container {
                max-width: 400px;
                margin: 20px auto; /* Ajustado para ficar mais próximo do card */
                background: #fff;
                border-radius: 8px;
                padding: 20px;
                box-shadow: 0 3px 10px rgba(0, 0, 0, 0.1);
                text-align: left;  /* para o form */
            }
    
            .container h1 {
                margin-bottom: 20px;
                color: #2c3e50;
                font-weight: 700;
                font-size: 1.4rem; /* um pouco menor para caber melhor no card */
                text-align: center;/* centraliza o título */
            }
    
            form {
                text-align: left;
            }
    
            label {
                display: block;
                margin-bottom: 6px;
                font-weight: 500;
                color: #333;
            }
    
            input[type="text"],
            input[type="number"],
            input[type="time"] {
                width: 100%;
                margin-bottom: 15px;
                padding: 10px;
                border: 1px solid #ccc;
                border-radius: 5px;
                box-sizing: border-box;
                font-size: 14px;
            }
    
            button[type="submit"] {
                background: #007BFF;
                border: none;
                color: #fff;
                padding: 12px 20px;
                font-size: 16px;
                border-radius: 5px;
                cursor: pointer;
                transition: background 0.3s, transform 0.3s;
                font-weight: 500;
                display: block;
                margin: 0 auto;
                width: fit-content;
            }
            button[type="submit"]:hover {
                background: #0056b3;
                transform: translateY(-2px);
            }
    
            .back-button {
                background: #dc3545;
                color: #fff;
                text-decoration: none;
                padding: 12px 20px;
                font-size: 16px;
                border-radius: 5px;
                transition: background 0.3s, transform 0.3s;
                font-weight: 500;
                display: block;
                margin: 10px auto 0;
                width: fit-content;
                text-align: center;
            }
            .back-button:hover {
                background: #c82333;
                transform: translateY(-2px);
            }
    
            /* ================================
               NOVA CLASSE PARA O SELECT
            ================================ */
            .select-modo {
                width: 100%;
                padding: 10px;
                background-color: #2c3e50; /* Mesma cor dos botões */
                color: #fff;
                font-size: 0.9rem;
                font-weight: 500;
                border: none;
                border-radius: 5px;
                margin-bottom: 15px;
                cursor: pointer;
                transition: background 0.2s, transform 0.2s, box-shadow 0.2s;
            }
            .select-modo:hover {
                background-color: #3b4d5e; 
                transform: translateY(-2px);
                box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1);
            }
            .select-modo option {
                color: #333;   /* Cor do texto interno das opções */
                background: #fff;
            }
    
        </style>
    </head>
    <body>
        <header>
          <h1>Controle de Relés</h1>
        </header>
    
        <div id="reles-container">
            %CONTROLES%
        </div>
    
        <script>
            // Atualiza o status dos relés a cada 250ms
            setInterval(() => {
                fetch('/status')
                    .then(response => response.json())
                    .then(data => {
                        data.reles.forEach((rele, index) => {
                            const estadoEl = document.getElementById(`estado-rele-${index}`);
                            const ligaDesligaBtn = document.getElementById(`liga-desliga-btn-${index}`);
    
                            if (!estadoEl || !ligaDesligaBtn) return;
    
                            // Atualiza apenas o estado visual
                            if (rele.status) {
                                estadoEl.textContent = "Ligado";
                                estadoEl.classList.add("ligado");
                                estadoEl.classList.remove("desligado");
                                ligaDesligaBtn.textContent = "Desligar";
                            } else {
                                estadoEl.textContent = "Desligado";
                                estadoEl.classList.add("desligado");
                                estadoEl.classList.remove("ligado");
                                ligaDesligaBtn.textContent = "Ligar";
                            }
    
                            // Define os event listeners apenas uma vez
                            if (!ligaDesligaBtn.hasEventListener) {
                                ligaDesligaBtn.hasEventListener = true;
                                ligaDesligaBtn.addEventListener('click', () => {
                                    const acao = rele.status ? 'desligar' : 'ligar';
                                    fetch(`/controle?rele=${index + 1}&acao=${acao}`)
                                        .then(() => {
                                            // Atualiza o status localmente
                                            rele.status = !rele.status;
                                        })
                                        .catch(err => console.error("Erro ao controlar relé:", err));
                                });
                            }
                        });
                    })
                    .catch(err => console.error("Erro ao buscar status:", err));
            }, 250);
        </script>
    </body>
    </html>
    )rawliteral";
    
        // Aqui montamos o conteúdo do %CONTROLES%, incluindo:
        //  - o card do relé
        //  - o formulário de configuração, logo abaixo, sem botão “Configurar”
        String controles;
        for (int i = 0; i < qtdRele; i++)
        {
            int tPulso = temposPulso[i];
            int tEntrada = temposEntrada[i];
            String hAtiv = rele[i].horaAtivacao;
            String hDesativ = rele[i].horaDesativacao;
    
            controles += "<div class='rele-card'>";
            controles += "<h2 contenteditable='true' ";
            controles += " onblur='salvarNomeRele(" + String(i + 1) + ", this.innerText)'>";
            controles += rele[i].nome;
            controles += "</h2>";
            controles += "  <div class='estado desligado' id='estado-rele-" + String(i) + "'>Desligado</div>";
            controles += "  <div class='botoes'>";
            controles += "    <button class='btn-liga-desliga' id='liga-desliga-btn-" + String(i) + "'>Ligar</button>";
            controles += "    <button class='btn-switch' onclick='fetch(`/switch?rele=" + String(i + 1) + "`)'>Pulso</button>";
            controles += "  </div>";
            controles += "  <div class='container'>";
            controles += "    <form action='/salvarconfig' method='POST'>";
            controles += "      <input type='hidden' name='rele' value='" + String(i + 1) + "' />";
            controles += "      <label>Tempo do Pulso (ms):</label>";
            controles += "      <input type='number' name='tempoPulso' min='0' placeholder='Ex: 10000' value='" + String(tPulso) + "' />";
            controles += "      <label>Tempo de Debounce (ms):</label>";
            controles += "      <input type='number' name='tempoEntrada' min='0' placeholder='Ex: 500' value='" + String(tEntrada) + "' />";
            controles += "      <label>Modo de Acionamento:</label>";
            controles += "      <select name='modoRele' class='select-modo'>";
            controles += "        <option value='0' " + String(modoAcionamento[i] == 0 ? "selected" : "") + ">Manter Ligado</option>";
            controles += "        <option value='1' " + String(modoAcionamento[i] == 1 ? "selected" : "") + ">Pulso</option>";
            controles += "        <option value='2' " + String(modoAcionamento[i] == 2 ? "selected" : "") + ">Desligar ao soltar</option>";
            controles += "      </select>";
            controles += "      <label>Hora de Ativação:</label>";
            controles += "      <input type='time' name='horaAtivacao' step='1' value='" + hAtiv + "' />";
            controles += "      <label>Hora de Desativação:</label>";
            controles += "      <input type='time' name='horaDesativacao' step='1' value='" + hDesativ + "' />";
            controles += "      <button type='submit'>Salvar Configuração</button>";
            controles += "    </form>";
            controles += "  </div>"; // .container
            controles += "</div>";
        }
        html.replace("%CONTROLES%", controles);
    
        String scriptJS = R"rawliteral(
        <script>
        // Função chamada no onblur do <h2 contenteditable="true">
        function salvarNomeRele(releIndex, novoNome) {
            // Remove espaços extras nas pontas, se quiser
            novoNome = novoNome.trim();
            if(!novoNome) return; // se vazio, não faz nada ou poderia mandar algo
    
            // Fazemos um fetch GET para /salvarnome?rele=X&nome=...
            fetch(`/salvarnome?rele=${releIndex}&nome=${encodeURIComponent(novoNome)}`)
                .then(response => {
                    if(!response.ok){
                        console.error("Erro ao salvar nome do relé:", response.statusText);
                    }
                })
                .catch(err => console.error("Erro fetch /salvarnome:", err));
        }
        </script>
        </body>
        </html>
        )rawliteral";
    
        html += scriptJS;
    
        return html;
    }

    void controlarRelesWebServer()
{
    String horaAtual = getHoraAtual();
    for (int i = 0; i < 5; i++)
    {
        if (!rele[i].horaAtivacao.isEmpty() && horaAtual == rele[i].horaAtivacao)
        {
            digitalWrite(rele[i].pino, HIGH);
            rele[i].status = 1;
            Serial.printf("Relé %d ativado pelo Web Server.\n", i + 1);
        }
        if (!rele[i].horaDesativacao.isEmpty() && horaAtual == rele[i].horaDesativacao)
        {
            digitalWrite(rele[i].pino, LOW);
            rele[i].status = 0;
            Serial.printf("Relé %d desativado pelo Web Server.\n", i + 1);
        }
    }
}