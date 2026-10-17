/* ============================================================================
 * app.js — frontend do simulador (fala com o servidor embutido em C)
 * ----------------------------------------------------------------------------
 * CONTRATO DA API (documentado aqui porque é o único lugar que descreve o
 * formato completo do JSON -- servidor.h só lista as rotas):
 *
 *   Formato "Estado" devolvido por GET /api/estado, POST /api/carregar,
 *   POST /api/avancar, POST /api/retroceder, POST /api/executar_tudo,
 *   POST /api/editar (em caso de sucesso):
 *   {
 *     "carregado": bool,   (false = nenhum arquivo carregado ainda)
 *     "tick": int, "terminada": bool,
 *     "algoritmo": string, "quantum": int, "ncpus": int,
 *     "ultimo_evento": string,
 *     "cpus":    [ { "id": int, "tarefa": int (-1 = nenhuma), "ticks_desligada": int } ],
 *     "tarefas": [ {
 *         "id": int, "cor": "#RRGGBB",
 *         "ingresso": int, "duracao": int, "periodo": int, "prazo": int,
 *         "estado": "INATIVA"|"PRONTA"|"EXECUTANDO"|"SUSPENSA"|"CONCLUIDA",
 *         "exec_restante": int, "quantum_restante": int,
 *         "ativacao": int, "deadline_abs": int, "ativacoes": int,
 *         "perdeu_prazo": 0|1, "cpu": int (-1 = nenhuma), "sorteada": 0|1,
 *         "eventos": string
 *     } ]
 *   }
 *
 *   POST /api/carregar   corpo: os BYTES do arquivo escolhido no seletor,
 *                        exatamente como estão no disco (pode ter BOM e
 *                        CRLF -- quem trata é o C, em config_carregar_texto).
 *     sucesso -> Estado (acima) + "avisos": [string...]. Carregar com
 *                sucesso também pode gerar avisos (ex.: tarefa aperiódica
 *                ignorada), e o req. 4.4 exige que cheguem ao usuário.
 *     falha   -> { "erro": true, "erros": [string...], "avisos": [string...] }
 *
 *   POST /api/avancar, /api/retroceder, /api/executar_tudo   (sem corpo)
 *     -> Estado, ou { "erro": true, "motivo": string } se não havia o que fazer.
 *
 *   POST /api/editar     corpo: { "id": int, "campo": string, "valor": string }
 *                          (nesta ordem e sem espaços: o C lê com um sscanf)
 *     -> Estado, ou { "erro": true, "erros": [motivo], "avisos": [] } se a
 *        edição foi recusada (mesmo formato do erro de carregamento).
 *
 *   GET /api/gantt        -> SVG pronto, só a janela dos últimos ticks
 *                            (GANTT_JANELA_TELA, em gantt.h).
 *   GET /api/exportar_svg -> SVG da simulação INTEIRA, para download.
 *
 * SEM HISTÓRICO NO CLIENTE (opção A, ver gantt.h): quem desenha o Gantt é o
 * C, a partir do histórico que ele já guarda (req. 1.5.2). Esta página só
 * exibe o SVG que chega pronto. Consequências:
 *   - "executar tudo" é UMA chamada a /api/executar_tudo, e não um laço de
 *     milhares de /api/avancar;
 *   - fechar e reabrir o navegador não perde nada: ao abrir, a página pede
 *     GET /api/estado e redesenha a partir do que o servidor tem.
 *
 * ==========================================================================*/

(function () {
  ("use strict");

  /* Estado global do frontend. Só o tick atual: o histórico mora no C. */
  const app = {
    atual: null /* último Estado recebido do servidor */,
    tarefaSelecionada: null,
    versaoGantt: 0 /* ver atualizarGantt() */,
  };

  const $ = (sel) => document.querySelector(sel);

  const els = {
    infoAlgoritmo: $("#info-algoritmo"),
    infoQuantum: $("#info-quantum"),
    infoCpus: $("#info-cpus"),
    infoTick: $("#info-tick"),
    infoEstadoSim: $("#info-estado-sim"),
    entradaArquivo: $("#entrada-arquivo"),
    btnCarregar: $("#btn-carregar"),
    btnRetroceder: $("#btn-retroceder"),
    btnAvancar: $("#btn-avancar"),
    btnExecutarTudo: $("#btn-executar-tudo"),
    btnExportarSvg: $("#btn-exportar-svg"),
    areaDiagnostico: $("#area-diagnostico"),
    listaErros: $("#lista-erros"),
    listaAvisos: $("#lista-avisos"),
    imgGantt: $("#img-gantt"),
    tabelaCpusBody: $("#tabela-cpus tbody"),
    tabelaTarefasBody: $("#tabela-tarefas tbody"),
    painelInspetor: $("#painel-inspetor"),
    inspetorTituloId: $("#inspetor-titulo-id"),
    formEditar: $("#form-editar"),
    editarCampo: $("#editar-campo"),
    editarValor: $("#editar-valor"),
    inspetorMotivo: $("#inspetor-motivo"),
    btnFecharInspetor: $("#btn-fechar-inspetor"),
    textoUltimoEvento: $("#texto-ultimo-evento"),
    selAlgoritmo: $("#sel-algoritmo"),
  };

  /* ------------------------------ chamadas HTTP ---------------------------- */
  async function chamarApi(caminho, opcoes) {
    /* O ÚNICO lugar que conversa com o programa em C. Manda o pedido e
     * devolve a resposta já convertida de JSON para objeto.
     *
     * fetch é a função do navegador que faz um pedido HTTP. O 'await'
     * espera a resposta sem travar a página.
     *
     * Não olha o status HTTP: quem diz se deu certo é o CORPO do JSON
     * ("erro": true), como combinado com o servidor.
     *
     * O catch pega dois casos: o programa em C foi fechado, ou a resposta
     * não era JSON. Devolve o erro NO MESMO FORMATO do servidor (erro,
     * erros, avisos), para quem chamou tratar tudo do mesmo jeito. Assim a
     * página avisa o usuário em vez de parecer travada. */
    try {
      const resp = await fetch(caminho, opcoes);
      return await resp.json();
    } catch (e) {
      const motivo =
        "Sem resposta do simulador. Verifique se o programa continua aberto.";
      return { erro: true, motivo, erros: [motivo], avisos: [] };
    }
  }

  const postJson = (caminho, corpo) =>
    chamarApi(caminho, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: corpo ? JSON.stringify(corpo) : undefined,
    });

  /* ------------------------------ ações da UI ------------------------------ */

  async function aoAbrirPagina() {
    /* Roda UMA vez, quando a página abre: pergunta ao C se já existe uma
     * simulação carregada e, se existir, redesenha a tela com ela.
     *
     * É o que faz fechar e reabrir o navegador (ou apertar F5) não perder
     * nada: a simulação mora no programa em C, e a página é só a tela.
     *
     * Se o C estiver fechado, chamarApi devolve {erro: true}, que não tem
     * "carregado": o if é falso e a página fica na tela inicial. */
    const resp = await chamarApi("/api/estado");
    if (resp && resp.carregado) {
      /* Nesta ordem: primeiro liga os botões, depois aplicarNovoEstado
       * desliga os que não fazem sentido (ex.: "avançar" numa simulação
       * já terminada). Invertido, o segundo desfaria o primeiro. */
      definirControlesHabilitados(true);
      aplicarNovoEstado(resp);
    }
  }

  async function aoCarregar() {
    /* Clique em "carregar": manda o arquivo escolhido para o C e mostra o
     * resultado. Quem LÊ e VALIDA o arquivo é o C (config.c); a página só
     * entrega os bytes e exibe a resposta. */

    /* Apaga as mensagens do carregamento anterior. */
    mostrarDiagnostico([], []);

    /* files[0] é o arquivo escolhido no seletor (<input type="file">). O
     * seletor é do sistema operacional: abre qualquer pasta ou pendrive. */
    const arquivo = els.entradaArquivo.files[0];
    if (!arquivo) {
      mostrarDiagnostico(
        ["Escolha um arquivo de configuração antes de carregar."],
        [],
      );
      return;
    }

    /* O servidor recusa acima de 1 MB fechando a conexão, sem mensagem.
     * Conferir aqui garante que o usuário veja o motivo. */
    if (arquivo.size > 1024 * 1024) {
      mostrarDiagnostico(["Arquivo grande demais (limite de 1 MB)."], []);
      return;
    }

    /* Enquanto espera: botão desativado e com o texto "carregando...". O
     * usuário vê que o programa está trabalhando, e não clica duas vezes. */
    els.btnCarregar.disabled = true;
    els.btnCarregar.textContent = "carregando...";

    /* body: arquivo -> o navegador manda os BYTES do arquivo como estão no
     * disco. O C recebe o conteúdo, nunca um caminho: por isso funciona de
     * qualquer lugar, inclusive do pendrive (req. 3.3.4). */
    const resp = await chamarApi("/api/carregar", {
      method: "POST",
      body: arquivo,
    });
    els.btnCarregar.disabled = false;
    els.btnCarregar.textContent = "carregar";

    /* Arquivo com erro: mostra a lista e PARA. A tela continua com a
     * simulação anterior, que o servidor também manteve. */
    if (resp.erro) {
      mostrarDiagnostico(resp.erros || [], resp.avisos || []);
      return;
    }
    /* Deu certo. Ainda pode haver avisos (ex.: tarefa aperiódica ignorada,
     * req. 4.4). O "|| []" cobre a resposta sem esse campo. */
    mostrarDiagnostico([], resp.avisos || []);
    /* Liga os botões ANTES: aplicarNovoEstado desliga depois os que não
     * fazem sentido (mesma ordem de aoAbrirPagina). */
    definirControlesHabilitados(true);
    aplicarNovoEstado(resp);
  }

  function mostrarDiagnostico(erros, avisos) {
    /* Põe na tela as mensagens de erro e de aviso, um item por mensagem.
     * Chamar com duas listas vazias limpa e esconde a área. */

    /* Esvazia primeiro, senão as mensagens antigas se acumulam. */
    els.listaErros.innerHTML = "";
    els.listaAvisos.innerHTML = "";
    for (const msg of erros) {
      const li = document.createElement("li");
      /* textContent, NUNCA innerHTML: a mensagem traz pedaços do arquivo do
       * usuário. Com innerHTML, um "<script>" escrito no arquivo seria
       * executado pela página; com textContent vira só texto. */
      li.textContent = msg;
      els.listaErros.appendChild(li);
    }

    for (const msg of avisos) {
      const li = document.createElement("li");
      li.textContent = msg;
      els.listaAvisos.appendChild(li);
    }
    /* Sem nenhuma mensagem, a área inteira some. */
    els.areaDiagnostico.hidden = erros.length === 0 && avisos.length === 0;
  }

  /* Manda um comando ao C (avançar, retroceder, executar tudo) e redesenha
   * a tela com o estado que volta. Os três botões fazem a MESMA coisa, só
   * muda o endereço; por isso uma função só, chamada pelos três. */
  async function comando(caminho) {
    /* Apaga as mensagens anteriores (ex.: avisos do carregamento). */
    mostrarDiagnostico([], []);

    /* Botões desligados enquanto espera: impede clique duplo e mostra que
     * o programa está trabalhando ("executar tudo" pode levar um instante).
     * definirControlesHabilitados não mexe no retroceder, daí a 2a linha. */
    definirControlesHabilitados(false);
    els.btnRetroceder.disabled = true;

    const resp = await postJson(caminho);

    /* Liga tudo; logo abaixo aplicarNovoEstado desliga o que não fizer
     * sentido no estado novo (mesma ordem de aoCarregar). */
    definirControlesHabilitados(true);

    /* A única falha aqui é perder a conexão com o programa. Mostra o
     * motivo e redesenha com o estado que a tela já tinha, para os botões
     * voltarem ao que eram. */
    if (resp.erro) {
      mostrarDiagnostico(resp.erros || [], []);
      aplicarNovoEstado(app.atual);
      return;
    }
    aplicarNovoEstado(resp);
  }

  /* Um tick (modo passo a passo, req. 1.5). Avançar depois do fim não faz
   * nada no C: ele devolve o mesmo estado. */
  async function aoAvancar() {
    await comando("/api/avancar");
  }

  /* Volta um tick, restaurado do histórico guardado no C (req. 1.5.2). */
  async function aoRetroceder() {
    await comando("/api/retroceder");
  }

  /* Execução completa (req. 1.5, modo b): UMA chamada. Os ticks do meio
   * ficam no histórico do C, de onde o Gantt é desenhado. */
  async function aoExecutarTudo() {
    await comando("/api/executar_tudo");
  }

  function aoExportarSvg() {
    /* Baixa o SVG da simulação inteira, gerado no C (req. 2.4). Como o
     * servidor responde com Content-Disposition: attachment, o navegador baixa o
     * arquivo sem sair da página. É o SVG da simulação INTEIRA (req. 2.4),
     * diferente do da tela, que mostra só a janela dos últimos ticks. */
    window.location.href = "/api/exportar_svg";
  }

  /* Troca o algoritmo no meio da simulação (req. 3.1). Se o C recusar,
   * mostra o motivo e o seletor volta ao algoritmo em vigor. */
  async function aoTrocarAlgoritmo() {
    mostrarDiagnostico([], []);
    const resp = await chamarApi("/api/algoritmo", {
      method: "POST",
      body: els.selAlgoritmo.value,
    });
    if (resp.erro) {
      mostrarDiagnostico(resp.erros || [], []);
      aplicarNovoEstado(app.atual);
      return;
    }
    aplicarNovoEstado(resp);
  }

  /* ------------------------- aplicar estado recebido ------------------------ */

  function aplicarNovoEstado(estado) {
    /* Redesenha a tela a partir de um Estado que veio do servidor. TODA
     * rota devolve o estado novo e todas passam por aqui: a página não
     * calcula nada da simulação, só mostra o que o C mandou.
     *
     * Com o painel de edição aberto, o valor sugerido acompanha o estado
     * novo (ex.: o usuário avançou um tick sem fechar o painel). */

    app.atual = estado;
    if (app.tarefaSelecionada !== null) {
      /* Outro arquivo carregado, sem essa tarefa: fecha o painel. */
      if (estado.tarefas.some((t) => t.id === app.tarefaSelecionada)) {
        sugerirValor();
        /* Apaga a mensagem da edição anterior: ela falava de outro tick.
         * (A confirmação de uma edição é escrita DEPOIS desta função.) */
        els.inspetorMotivo.textContent = "";
        els.inspetorMotivo.classList.remove("ok");
      } else fecharInspetor();
    }
    if (!estado) return;

    /* Cabeçalho. O rótulo vai junto ("algoritmo: RM") porque textContent
     * troca o texto INTEIRO do elemento. */
    els.infoAlgoritmo.textContent = "algoritmo: " + estado.algoritmo;
    els.infoQuantum.textContent = "quantum: " + estado.quantum;
    els.infoCpus.textContent = "CPUs: " + estado.ncpus;
    els.infoTick.textContent = "tick: " + estado.tick;
    els.infoEstadoSim.textContent = estado.terminada
      ? "concluída"
      : "em execução";
    els.textoUltimoEvento.textContent = estado.ultimo_evento || "--";

    /* Tabelas e Gantt: redesenhados a cada estado novo. */

    atualizarTabelaCpus(estado);
    atualizarTabelaTarefas(estado);
    atualizarGantt();

    /* Botões que não fariam nada ficam desativados: não há para onde
     * retroceder no tick 0, nem o que avançar depois do fim. */
    els.btnRetroceder.disabled = !(estado.tick > 0);
    els.btnAvancar.disabled = estado.terminada;
    els.btnExecutarTudo.disabled = estado.terminada;
    
    /* Opções do seletor: montadas uma vez, a partir da lista do C. */
    if (els.selAlgoritmo.options.length === 0)
      for (const nome of estado.algoritmos || []) {
        const op = document.createElement("option");
        op.value = nome;
        op.textContent = nome;
        els.selAlgoritmo.appendChild(op);
      }
    /* Mostra o algoritmo em vigor: depois de carregar, de trocar e de
     * retroceder para antes de uma troca. */
    els.selAlgoritmo.value = estado.algoritmo;
    els.selAlgoritmo.disabled = estado.terminada;
  }

  function definirControlesHabilitados(ligado) {
    /* Liga ou desliga os botões que só servem com uma simulação carregada.
     * No HTML eles começam desativados (atributo "disabled").
     * "retroceder" fica de fora: quem cuida dele é aplicarNovoEstado, porque
     * depende do tick (no tick 0 não há para onde voltar). */
    els.btnAvancar.disabled = !ligado;
    els.btnExecutarTudo.disabled = !ligado;
    els.btnExportarSvg.disabled = !ligado;
  }

  /* Cria uma célula <td> com um texto e a pendura na linha <tr>. Usada pelas
   * duas tabelas. textContent e não innerHTML, pelo mesmo motivo de
   * mostrarDiagnostico: o texto nunca é interpretado como HTML. */
  function celula(tr, texto) {
    const td = document.createElement("td");
    td.textContent = texto;
    tr.appendChild(td);
  }

  function atualizarTabelaCpus(estado) {
    /* Tabela de CPUs: uma linha por processador, na ordem que o C mandou.
     * A tabela é REFEITA inteira a cada estado novo (esvazia e recria): é
     * mais simples do que descobrir o que mudou, e são poucas linhas. */
    els.tabelaCpusBody.innerHTML = "";
    for (const cpu of estado.cpus) {
      const tr = document.createElement("tr");
      celula(tr, cpu.id);
      /* O C manda o ID da tarefa, ou -1 se a CPU não tem o que executar.
       * Req. 1.2: CPU sem tarefa é DESLIGADA, e o usuário precisa ver isso
       * e por quanto tempo (a coluna seguinte). */
      celula(tr, cpu.tarefa === -1 ? "desligada" : cpu.tarefa);
      celula(tr, cpu.ticks_desligada);
      els.tabelaCpusBody.appendChild(tr);
    }
  }

  function atualizarTabelaTarefas(estado) {
    /* Tabela de tarefas: uma linha por tarefa, com o que o TCB guarda
     * dela neste tick (req. 1.5.1: examinar cada tarefa no passo a passo).
     * Com ~190 tarefas a tabela fica longa; o painel lateral rola. */
    els.tabelaTarefasBody.innerHTML = "";
    for (const tarefa of estado.tarefas) {
      const tr = document.createElement("tr");
      /* Clicar na linha abre o painel de edição daquela tarefa. A função
       * entre "() =>" só roda no clique, e lembra de qual tarefa é. */
      tr.addEventListener("click", () => abrirInspetor(tarefa.id));
      /* Destaca a linha da tarefa aberta no painel de edição. */
      if (tarefa.id === app.tarefaSelecionada) tr.classList.add("selecionada");
      celula(tr, tarefa.id);
      celula(tr, tarefa.estado);
      celula(tr, tarefa.exec_restante);
      celula(tr, tarefa.ativacoes);
      celula(tr, tarefa.prazo);
      els.tabelaTarefasBody.appendChild(tr);
    }
  }

  /* --------------------------------- gantt --------------------------------- */

  function atualizarGantt() {
    /* O desenho todo é do C (gantt.c); aqui só se pede a imagem de novo.
     * O '?v=' com um contador que sempre sobe existe por causa do cache:
     * se a URL fosse sempre '/api/gantt', o navegador reaproveitaria a
     * imagem anterior e o gráfico não mudaria. Contador, e não o número do
     * tick: editar uma tarefa muda o gráfico SEM mudar o tick. */
    els.imgGantt.src = "/api/gantt?v=" + ++app.versaoGantt;
  }

  /* -------------------------------- inspetor -------------------------------- */
  /* Painel de edição (req. 1.5.2 e 3.4): muda o estado, o tempo restante
   * ou o prazo de uma tarefa em qualquer passo da simulação. Quem VALIDA
   * é o C (sim_editar_tarefa); a página só manda o pedido e mostra a
   * resposta. É também a única forma de ter uma tarefa SUSPENSA no
   * Projeto A, porque os eventos que suspendem são do Projeto B. */

  /* Valor sugerido (req. 3.2): o formulário mostra o valor ATUAL do campo
   * escolhido. As opções do seletor têm os mesmos nomes dos campos da
   * tarefa no JSON, por isso t[campo] serve para as três. Sem tarefa (ex.:
   * outro arquivo foi carregado com o painel aberto), não faz nada. */
  function sugerirValor() {
    const t =
      app.atual &&
      app.atual.tarefas.find((x) => x.id === app.tarefaSelecionada);
    if (!t) return;
    const campo = els.editarCampo.value;
    els.editarValor.value = t[campo];
    els.editarValor.placeholder =
      campo === "estado" ? "PRONTA, SUSPENSA, INATIVA ou CONCLUIDA" : "";
  }

  /* Abre o painel para a tarefa clicada na tabela. Guarda QUAL tarefa é
   * em app.tarefaSelecionada, porque o formulário só tem campo e valor. */
  function abrirInspetor(id) {
    app.tarefaSelecionada = id;
    /* "T3", o mesmo nome das linhas do Gantt. */
    els.inspetorTituloId.textContent = "T" + id;
    els.inspetorMotivo.textContent = ""; /* apaga a mensagem da edição anterior */
    els.inspetorMotivo.classList.remove("ok");
    els.painelInspetor.hidden = false;
    atualizarTabelaTarefas(app.atual); /* destaca a linha clicada */
    els.painelInspetor.scrollIntoView({ block: "nearest" });
    sugerirValor(); /* o formulário já abre com o valor atual (req. 3.2) */
  }

  /* Fecha o painel e "esquece" a tarefa selecionada. */
  function fecharInspetor() {
    app.tarefaSelecionada = null;
    els.painelInspetor.hidden = true;
    if (app.atual) atualizarTabelaTarefas(app.atual); /* tira o destaque */
  }

  /* Botão "aplicar" do formulário. Edição aceita: a tela é redesenhada
   * com o estado novo (Gantt incluído) e o painel fecha. Recusada: o
   * motivo aparece no próprio painel, que FICA aberto para o usuário
   * corrigir e tentar de novo (req. 3.4: o usuário precisa saber por que
   * a edição não pôde ser feita). */
  async function aoSubmeterEdicao(ev) {
    /* Sem isto, o navegador "enviaria" o formulário do jeito antigo e
     * recarregaria a página inteira. */
    ev.preventDefault();

    /* O C lê este corpo com um sscanf: a ordem id, campo, valor importa
     * (ver rota_editar em servidor.c). */
    const corpo = {
      id: app.tarefaSelecionada,
      campo: els.editarCampo.value,
      valor: els.editarValor.value,
    };
    const resp = await postJson("/api/editar", corpo);

    if (!resp.erro) {
      /* Aceita: o painel FICA aberto (o usuário pode querer outra edição
       * na mesma tarefa) e confirma em verde; "fechar" fecha. */
      aplicarNovoEstado(resp);
      els.inspetorMotivo.textContent = "alteração aplicada no tick " + resp.tick;
      els.inspetorMotivo.classList.add("ok");
    } else {
      els.inspetorMotivo.textContent = resp.erros[0];
      els.inspetorMotivo.classList.remove("ok");
    }
  }

  /* --------------------------------- start ---------------------------------- */

  els.btnCarregar.addEventListener("click", aoCarregar);
  els.btnAvancar.addEventListener("click", aoAvancar);
  els.btnRetroceder.addEventListener("click", aoRetroceder);
  els.btnExecutarTudo.addEventListener("click", aoExecutarTudo);
  els.btnExportarSvg.addEventListener("click", aoExportarSvg);
  els.formEditar.addEventListener("submit", aoSubmeterEdicao);
  els.btnFecharInspetor.addEventListener("click", fecharInspetor);
  els.selAlgoritmo.addEventListener("change", aoTrocarAlgoritmo);
  /* Trocar o campo no painel (estado -> prazo...) troca o valor sugerido. */
  els.editarCampo.addEventListener("change", sugerirValor);
  aoAbrirPagina();
})();
