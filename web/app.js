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
 *   POST /api/carregar   corpo: { "caminho": "config.txt" }
 *     sucesso -> Estado (acima).
 *     falha   -> { "erro": true, "erros": [string...], "avisos": [string...] }
 *
 *   POST /api/avancar, /api/retroceder, /api/executar_tudo   (sem corpo)
 *     -> Estado, ou { "erro": true, "motivo": string } se não havia o que fazer.
 *
 *   POST /api/editar     corpo: { "id": int, "campo": string, "valor": string }
 *     -> { "ok": true, ...Estado } ou { "ok": false, "motivo": string }.
 *
 *   GET /api/exportar_svg -> corpo é o arquivo SVG (Content-Type image/svg+xml).
 *
 * HISTÓRICO NO CLIENTE: o backend guarda o histórico completo (requisito
 * 1.5.2), mas a API só expõe o tick ATUAL a cada chamada. Pra desenhar o
 * Gantt (que precisa de TODOS os ticks já percorridos, não só o corrente),
 * este arquivo mantém seu próprio array `historico` e o alimenta a cada
 * avancar/retroceder/carregar -- por isso "executar tudo" é feito aqui como
 * um LOOP de avancar() no cliente (em vez de uma única chamada a
 * /api/executar_tudo): assim cada tick intermediário passa pelo mesmo
 * caminho de código que popula `historico`, e o Gantt fica completo. Isso é
 * uma escolha de design, não uma imposição do contrato acima -- dá pra
 * trocar por /api/executar_tudo + reconstrução via estado_json_historico se
 * preferir (ver estado_json.h).
 *
 * ==========================================================================*/

(function () {
  'use strict';

  /* Estado global do frontend. */
  const app = {
    historico: [],   /* array de Estado, indexado por tick (historico[t].tick === t) */
    atual: null,      /* Estado do tick corrente (== historico[historico.length-1]) */
    tarefaSelecionada: null,
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
    svgGantt: $('#svg-gantt'),
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

  async function aoCarregar() {
    /* TODO:
     * 1) limpar area-diagnostico (mostrarDiagnostico([], []) ou similar).
     * 2) const resp = await postJson('/api/carregar', {caminho:
     *    els.entradaArquivo.value}).
     * 3) se resp.erro: mostrarDiagnostico(resp.erros||[], resp.avisos||[])
     *    e parar aqui (não mexer em app.historico/app.atual).
     * 4) senão: app.historico = [resp]; aplicarNovoEstado(resp);
     *    definirControlesHabilitados(true). */
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
     * !resp || resp.erro, não fazer nada (ex.: já terminou). Senão:
     * app.historico.push(resp) e aplicarNovoEstado(resp). */
  }

  async function aoRetroceder() {
    /* TODO: se app.historico.length <= 1, não há pra onde voltar --
     * retornar sem chamar a API. Senão: const resp = await
     * postJson('/api/retroceder'); se sucesso, app.historico.pop() (o
     * tick que acabamos de descartar) e aplicarNovoEstado(resp) (resp já
     * é o estado do tick anterior, devolvido pelo servidor). */
  }

  async function aoExecutarTudo() {
    /* TODO: ver o comentário no topo do arquivo sobre por que isso é um
     * LOOP de aoAvancar()-like no cliente, e não uma única chamada a
     * /api/executar_tudo: precisamos passar por CADA tick intermediário
     * pra alimentar app.historico e o Gantt ficar completo.
     *   while (app.atual && !app.atual.terminada) {
     *     const resp = await postJson('/api/avancar');
     *     if (!resp || resp.erro) break;
     *     app.historico.push(resp);
     *     app.atual = resp;
     *   }
     *   aplicarNovoEstado(app.atual);
     * Cuidado com simulações longas (até MAX_TICKS=5000): redesenhar o
     * SVG inteiro a cada tick trava a UI. Dá pra só chamar
     * aplicarNovoEstado a cada N ticks e uma última vez no final --
     * detalhe de performance, não obrigatório pro funcionamento básico. */
  }

  function aoExportarSvg() {
    /* TODO: basta abrir '/api/exportar_svg' numa nova aba/janela (ex.
     * window.open('/api/exportar_svg', '_blank')) -- o servidor já
     * devolve o arquivo com o Content-Type certo, o navegador cuida do
     * download/exibição. */
  }

  /* ------------------------- aplicar estado recebido ------------------------ */

  function aplicarNovoEstado(estado) {
    /* TODO: app.atual = estado; se estado for falsy, retornar cedo.
     * Depois:
     *   - atualizar o cabeçalho (#info-algoritmo/#info-quantum/
     *     #info-cpus/#info-tick/#info-estado-sim, este último tipo
     *     "concluída" vs "em execução") e #texto-ultimo-evento.
     *   - atualizarTabelaCpus(estado) e atualizarTabelaTarefas(estado).
     *   - redesenharGantt().
     *   - habilitar/desabilitar botões: retroceder só faz sentido se
     *     app.historico.length > 1; avançar/executar-tudo só fazem
     *     sentido se !estado.terminada.
     * Se o painel do inspetor estiver aberto (app.tarefaSelecionada !=
     * null) pra uma tarefa que ainda existe no novo estado, considere
     * também atualizar o que está mostrado lá. */
  }

  function definirControlesHabilitados(ligado) {
    /* TODO: ligar/desligar btnAvancar/btnExecutarTudo/btnExportarSvg
     * conforme 'ligado'; btnRetroceder começa sempre desabilitado (só
     * liga depois do primeiro avançar, quando historico.length > 1). */
  }

  function atualizarTabelaCpus(estado) {
    /* TODO: reconstruir #tabela-cpus tbody (limpar com innerHTML = '' e
     * recriar): uma <tr> por CPU em estado.cpus, com <td> pra
     * id/tarefa (mostrar "--" se tarefa === -1)/ticks_desligada. */
  }

  function atualizarTabelaTarefas(estado) {
    /* TODO: reconstruir #tabela-tarefas tbody: uma <tr> clicável por
     * tarefa em estado.tarefas (id/estado/exec_restante/ativacoes/
     * prazo), com um listener de click que chama abrirInspetor(t.id). */
  }

  /* --------------------------------- gantt --------------------------------- */

  const CEL_W = 14, LIN_H = 20, MARG_E = 34, MARG_T = 10;

  function redesenharGantt() {
    /* TODO: espelha a lógica de gantt_svg() em gantt.c -- mesmas
     * convenções do requisito 2.1/2.5 (EXECUTANDO = cor da tarefa +
     * número da CPU dentro da célula; PRONTA = sem preenchimento, só
     * contorno; SUSPENSA = preto com padrão hachurado; INATIVA/
     * CONCLUIDA = nada desenhado; eixo Y com o MENOR id embaixo), mas
     * lendo de app.historico (client-side) em vez de um array Estado[]
     * do C. Testado e validado visualmente antes deste esqueleto, com
     * duas tarefas RM: os retângulos coloridos, os marcadores ▲/●/✗/★ e
     * a ordem das linhas no eixo Y saíram corretos.
     *
     * 1) svg.innerHTML = '' pra limpar o desenho anterior; se
     *    app.historico estiver vazio, retornar.
     * 2) pegar os ids das tarefas a partir de app.historico[0].tarefas,
     *    ordenados CRESCENTE (a lista de tarefas não muda depois de
     *    carregada) -- índice 0 = menor id = linha mais embaixo no
     *    desenho.
     * 3) calcular largura/altura totais a partir de
     *    MARG_E/MARG_T/CEL_W/LIN_H, número de ticks e número de
     *    tarefas; aplicar em width/height/viewBox do <svg>.
     * 4) para cada tick t de app.historico e cada tarefa desse
     *    snapshot: achar a linha (posição Y) pelo id, calcular x = MARG_E
     *    + t*CEL_W, criar um <rect> via
     *    document.createElementNS('http://www.w3.org/2000/svg', 'rect')
     *    com fill/stroke conforme o estado (ver convenções acima). Se
     *    EXECUTANDO, um <text> com o número da CPU centralizado na
     *    célula.
     * 5) marcador de evento: comparar a tarefa nesse tick com a mesma
     *    tarefa no tick anterior (historicoTarefaPorId, abaixo) e
     *    desenhar ▲ (chegada) / ● (término) / ✗ (prazo perdido) / ★
     *    (sorteio) por cima da célula quando aplicável -- mesma
     *    prioridade documentada em marca_em() de gantt.c.
     * 6) rótulos: id de cada tarefa na margem esquerda (eixo Y), e
     *    talvez o número do tick a cada N células no eixo X (evita
     *    poluir visualmente em simulações longas). */
  }

  function historicoTarefaPorId(snap, id) {
    /* TODO: se snap for null/undefined, devolver null. Senão,
     * snap.tarefas.find(t => t.id === id) || null. */
    return null;
  }

  function marcaEm(antes, agora) {
    /* TODO: mesma prioridade documentada em gantt.c (marca_em): prazo
     * perdido > término > chegada > sorteio.
     *   - agora.perdeu_prazo && (!antes || !antes.perdeu_prazo) -> '✗'
     *   - antes existia, não estava INATIVA/CONCLUIDA, e agora está
     *     INATIVA ou CONCLUIDA -> '●'
     *   - antes existia, estava INATIVA, e agora não está mais -> '▲'
     *   - agora.sorteada -> '★'
     *   - senão -> '' (nenhum marcador) */
    return '';
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
     * postJson('/api/editar', corpo). Se resp.ok: sobrescrever
     * app.historico[app.historico.length-1] = resp (editar não avança
     * nem retrocede o tick, só corrige o snapshot atual -- não é um novo
     * tick), aplicarNovoEstado(resp), fecharInspetor(). Se !resp.ok:
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
})();