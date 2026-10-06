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
 *     -> { "ok": true, ...Estado } ou { "ok": false, "motivo": string }.
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
  'use strict';

  /* Estado global do frontend. Só o tick atual: o histórico mora no C. */
  const app = {
    atual: null,          /* último Estado recebido do servidor */
    tarefaSelecionada: null,
    versaoGantt: 0,       /* ver atualizarGantt() */
  };

  const $ = (sel) => document.querySelector(sel);

  const els = {
    infoAlgoritmo: $('#info-algoritmo'),
    infoQuantum: $('#info-quantum'),
    infoCpus: $('#info-cpus'),
    infoTick: $('#info-tick'),
    infoEstadoSim: $('#info-estado-sim'),
    entradaArquivo: $('#entrada-arquivo'),
    btnCarregar: $('#btn-carregar'),
    btnRetroceder: $('#btn-retroceder'),
    btnAvancar: $('#btn-avancar'),
    btnExecutarTudo: $('#btn-executar-tudo'),
    btnExportarSvg: $('#btn-exportar-svg'),
    areaDiagnostico: $('#area-diagnostico'),
    listaErros: $('#lista-erros'),
    listaAvisos: $('#lista-avisos'),
    imgGantt: $('#img-gantt'),
    tabelaCpusBody: $('#tabela-cpus tbody'),
    tabelaTarefasBody: $('#tabela-tarefas tbody'),
    painelInspetor: $('#painel-inspetor'),
    inspetorTituloId: $('#inspetor-titulo-id'),
    formEditar: $('#form-editar'),
    editarCampo: $('#editar-campo'),
    editarValor: $('#editar-valor'),
    inspetorMotivo: $('#inspetor-motivo'),
    btnFecharInspetor: $('#btn-fechar-inspetor'),
    textoUltimoEvento: $('#texto-ultimo-evento'),
  };

  /* ------------------------------ chamadas HTTP ---------------------------- */

  async function chamarApi(caminho, opcoes) {
    /* TODO: fetch(caminho, opcoes); ler a resposta como JSON
     * (await resp.json()); não lançar em resposta HTTP não-2xx -- o
     * contrato usa o CORPO json ({erro:true,...} ou {ok:false,...}) pra
     * sinalizar falha, não o status HTTP, então basta devolver o JSON já
     * parseado pro chamador decidir. Se o fetch em si falhar (rede/
     * servidor fora do ar), capturar num try/catch e devolver algo como
     * {erro:true, motivo:"não consegui falar com o servidor"} em vez de
     * deixar a exceção subir. */
    return null;
  }

  const postJson = (caminho, corpo) => chamarApi(caminho, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: corpo ? JSON.stringify(corpo) : undefined,
  });

  /* ------------------------------ ações da UI ------------------------------ */

  async function aoAbrirPagina() {
    /* TODO: const resp = await chamarApi('/api/estado'); se
     * resp && resp.carregado: aplicarNovoEstado(resp) e
     * definirControlesHabilitados(true). Senão, deixar a tela no estado
     * inicial (só o seletor de arquivo habilitado).
     * É isto que faz fechar e reabrir o navegador no meio da simulação
     * não perder nada (card do kanban "testar fechar e reabrir"). */
  }

  async function aoCarregar() {
    /* TODO:
     * 1) limpar area-diagnostico (mostrarDiagnostico([], [])).
     * 2) const arquivo = els.entradaArquivo.files[0]; se não houver,
     *    mostrarDiagnostico(['Escolha um arquivo de configuração antes
     *    de carregar.'], []) e parar.
     * 3) const resp = await chamarApi('/api/carregar', { method: 'POST',
     *    body: arquivo }). Passar o próprio File como body faz o navegador
     *    mandar os bytes como estão no disco -- inclusive do pendrive --
     *    sem nenhuma leitura em JS. (file.text() também funciona, mas
     *    decodifica como UTF-8 e já remove o BOM sozinho; aí o tratamento
     *    de BOM do C nunca seria exercitado pela página.)
     * 4) se resp.erro: mostrarDiagnostico(resp.erros||[], resp.avisos||[])
     *    e parar aqui (não mexer em app.atual).
     * 5) senão: mostrarDiagnostico([], resp.avisos||[]) se o servidor
     *    mandar avisos junto (ex.: tarefa aperiódica ignorada, req. 4.4),
     *    aplicarNovoEstado(resp); definirControlesHabilitados(true). */
  }

  function mostrarDiagnostico(erros, avisos) {
    /* TODO: popular #lista-erros e #lista-avisos com um <li> por mensagem
     * (usar .textContent, nunca innerHTML, pra não injetar HTML vindo do
     * arquivo de config do usuário) e mostrar/esconder #area-diagnostico
     * (hidden = erros.length === 0 && avisos.length === 0). Lembrar de
     * limpar (innerHTML = '') as listas antes de repopular, senão
     * mensagens antigas ficam acumulando a cada carregar(). */
  }

  async function aoAvancar() {
    /* TODO: const resp = await postJson('/api/avancar'); se
     * !resp || resp.erro, não fazer nada (ex.: já terminou). Senão,
     * aplicarNovoEstado(resp). */
  }

  async function aoRetroceder() {
    /* TODO: const resp = await postJson('/api/retroceder'); se
     * !resp || resp.erro, não fazer nada (já estava no tick 0). Senão,
     * aplicarNovoEstado(resp) -- resp já é o estado do tick anterior,
     * restaurado pelo servidor a partir do histórico dele. */
  }

  async function aoExecutarTudo() {
    /* TODO: const resp = await postJson('/api/executar_tudo'); se
     * sucesso, aplicarNovoEstado(resp). Uma chamada só: os ticks
     * intermediários ficam no histórico do C, e o Gantt vem de lá. */
  }

  function aoExportarSvg() {
    /* TODO: window.location.href = '/api/exportar_svg'. Como o servidor
     * responde com Content-Disposition: attachment, o navegador baixa o
     * arquivo sem sair da página. É o SVG da simulação INTEIRA (req. 2.4),
     * diferente do da tela, que mostra só a janela dos últimos ticks. */
  }

  /* ------------------------- aplicar estado recebido ------------------------ */

  function aplicarNovoEstado(estado) {
    /* TODO: app.atual = estado; se estado for falsy, retornar cedo.
     * Depois:
     *   - atualizar o cabeçalho (#info-algoritmo/#info-quantum/
     *     #info-cpus/#info-tick/#info-estado-sim, este último tipo
     *     "concluída" vs "em execução") e #texto-ultimo-evento.
     *   - atualizarTabelaCpus(estado) e atualizarTabelaTarefas(estado).
     *   - atualizarGantt().
     *   - habilitar/desabilitar botões: retroceder só faz sentido se
     *     estado.tick > 0; avançar/executar-tudo só fazem sentido se
     *     !estado.terminada.
     * Se o painel do inspetor estiver aberto (app.tarefaSelecionada !=
     * null) pra uma tarefa que ainda existe no novo estado, considere
     * também atualizar o que está mostrado lá. */
  }

  function definirControlesHabilitados(ligado) {
    /* TODO: ligar/desligar btnAvancar/btnExecutarTudo/btnExportarSvg
     * conforme 'ligado'; btnRetroceder começa desabilitado e é
     * aplicarNovoEstado quem liga, quando estado.tick > 0. */
  }

  function atualizarTabelaCpus(estado) {
    /* TODO: reconstruir #tabela-cpus tbody (limpar com innerHTML = '' e
     * recriar): uma <tr> por CPU em estado.cpus, com <td> pra
     * id/tarefa (mostrar "desligada" se tarefa === -1)/ticks_desligada. */
  }

  function atualizarTabelaTarefas(estado) {
    /* TODO: reconstruir #tabela-tarefas tbody: uma <tr> clicável por
     * tarefa em estado.tarefas (id/estado/exec_restante/ativacoes/
     * prazo), com um listener de click que chama abrirInspetor(t.id).
     * Com o arquivo do professor (~190 tarefas) a tabela fica longa:
     * normal, o painel lateral rola. */
  }

  /* --------------------------------- gantt --------------------------------- */

  function atualizarGantt() {
    /* TODO: els.imgGantt.src = '/api/gantt?v=' + (++app.versaoGantt);
     *
     * O desenho todo é do C (gantt.c); aqui só se pede a imagem de novo.
     * O '?v=' com um contador que sempre sobe existe por causa do cache:
     * se a URL fosse sempre '/api/gantt', o navegador reaproveitaria a
     * imagem anterior e o gráfico não mudaria. Contador, e não o número do
     * tick: editar uma tarefa muda o gráfico SEM mudar o tick. */
  }

  /* -------------------------------- inspetor -------------------------------- */

  function abrirInspetor(id) {
    /* TODO: app.tarefaSelecionada = id; preencher
     * #inspetor-titulo-id (ex. "#"+id); limpar #inspetor-motivo; mostrar
     * #painel-inspetor (hidden = false). */
  }

  function fecharInspetor() {
    /* TODO: app.tarefaSelecionada = null; #painel-inspetor.hidden =
     * true. */
  }

  async function aoSubmeterEdicao(ev) {
    /* TODO: ev.preventDefault(); montar o corpo {id:
     * app.tarefaSelecionada, campo: els.editarCampo.value, valor:
     * els.editarValor.value} e const resp = await
     * postJson('/api/editar', corpo). Se resp.ok: aplicarNovoEstado(resp)
     * (que já pede o Gantt de novo) e fecharInspetor(). Se !resp.ok:
     * mostrar resp.motivo em #inspetor-motivo, SEM fechar o painel (pra
     * o usuário poder corrigir e tentar de novo). */
  }

  /* --------------------------------- start ---------------------------------- */

  els.btnCarregar.addEventListener('click', aoCarregar);
  els.btnAvancar.addEventListener('click', aoAvancar);
  els.btnRetroceder.addEventListener('click', aoRetroceder);
  els.btnExecutarTudo.addEventListener('click', aoExecutarTudo);
  els.btnExportarSvg.addEventListener('click', aoExportarSvg);
  els.formEditar.addEventListener('submit', aoSubmeterEdicao);
  els.btnFecharInspetor.addEventListener('click', fecharInspetor);
  aoAbrirPagina();
})();
