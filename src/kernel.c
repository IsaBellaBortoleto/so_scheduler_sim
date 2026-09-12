/* ============================================================================
 * kernel.c — O coração da simulação
 * ----------------------------------------------------------------------------
 * ORDEM DOS ACONTECIMENTOS EM UM TICK
 *
 * O estado guardado em `s->atual` está SEMPRE "pronto para ser desenhado":
 * as ativações do instante já ocorreram e o escalonador já decidiu quem ocupa
 * cada CPU. Avançar um tick é, portanto:
 *
 *    executar()          consome 1 unidade de tempo das tarefas nas CPUs
 *    tick++              o relógio global anda (requisito 1.1)
 *    verificar_prazos()  quem passou do deadline é marcado (requisito 3.3)
 *    ativar()            tarefas periódicas que vencem agora viram PRONTAS
 *    escalonar()         escolhe quem ocupa cada CPU no próximo intervalo
 *    snapshot()          congela tudo em historico[tick] (requisito 1.5.2)
 *
 * Manter essa ordem fixa é o que torna a simulação DETERMINÍSTICA: rodar,
 * retroceder e avançar de novo produz exatamente o mesmo resultado.
 *
 * NOTA: estado_clonar/estado_liberar/estado_nome moram em tcb.c (é lá que
 * `tcb.h` os declara), não duplicar aqui.
 *
 * DUAS PEÇAS QUE FALTAVAM NESTE ARQUIVO (em relação ao que kernel.h
 * declara/ao comentário acima) e que eu acrescentei como TODO:
 *   - sim_iniciar(): estava declarada em kernel.h mas sem nenhum stub.
 *   - ativar(): citada no comentário de ordem acima, mas sem stub também.
 * ==========================================================================*/
#include "kernel.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int sim_indice_por_id(const Estado *e, int id)
{
    /* TODO: percorrer e->tarefas procurando id igual; devolver o índice
     * ou -1 se não achar. */
    (void)e;
    (void)id;
    return -1;
}

/* TODO (função nova, use interna): devolve 1 se TODAS as tarefas estão
 * EST_CONCLUIDA (inclusive o caso de ntarefas==0 -- vacuamente verdadeiro:
 * nada pra simular). Usada por sim_iniciar/sim_avancar pra decidir
 * s->terminada. */
static int todas_concluidas(const Estado *e)
{
    (void)e;
    return 0;
}

/* ------------------------------- histórico ------------------------------- */

/* Guarda a fotografia do tick corrente em historico[n_hist].
 *
 * ponytail: guardamos o estado INTEIRO a cada tick, não um diário de
 * diferenças. Custo = ntarefas * ticks * sizeof(TCB) — para 20 tarefas e
 * 5000 ticks dá poucos megabytes, e em troca retroceder é um memcpy.
 * Trocar por undo-log só se a simulação precisar de milhões de ticks. */
static void snapshot(Simulacao *s)
{
    /* TODO: se n_hist >= cap_hist, realloc(historico, nova_cap *
     * sizeof(Estado)) dobrando a capacidade (comece com algo como 64).
     * Depois: historico[n_hist] = estado_clonar(&s->atual); n_hist++. */
    (void)s;
}

/* ATIVAÇÃO — uma tarefa periódica INATIVA vira PRONTA quando o tick atual
 * cai exatamente em ingresso + k*periodo, para algum k >= 0. */
static void ativar(Simulacao *s)
{
    /* TODO: para cada tarefa com estado==EST_INATIVA e periodo>0: se
     * tick >= ingresso e (tick - ingresso) % periodo == 0, ativar:
     *   estado = EST_PRONTA
     *   exec_restante = duracao
     *   quantum_restante = 0 (só é definido de verdade quando ela ganhar
     *                          uma CPU, em escalonar())
     *   ativacao = tick (guarda quando esta ativação começou)
     *   deadline_abs = tick + prazo
     *   perdeu_prazo = 0
     *   cpu = -1
     *   sorteada = 0
     * Dica: vale registrar isso em s->ultimo_evento (snprintf) pra
     * facilitar o debug/gantt. */
    (void)s;
}

/* ESCALONAMENTO — requisito 1.2.
 * A fila de prontos é GLOBAL (um único vetor de candidatas para todas as
 * CPUs, não uma fila por processador) e a ociosidade é minimizada: só sobra
 * CPU desligada se realmente não houver tarefa pronta.
 *
 * Reavalia do zero a cada tick quem deve ocupar cada CPU (é isso que torna o
 * escalonamento PREEMPTIVO): uma tarefa só continua na mesma CPU se ainda
 * estiver entre as mais prioritárias segundo escalonador_ordenar (que já
 * aplica os critérios de desempate do requisito 4.3, dando vantagem a quem
 * já estava executando só como critério de desempate, não como prioridade
 * absoluta). */
static void escalonar(Simulacao *s)
{
    /* TODO:
     * 1) montar a lista de candidatos: índices de tarefas com estado
     *    EST_PRONTA ou EST_EXECUTANDO (quem já está rodando também entra
     *    na disputa de novo, pra permitir preempção de verdade).
     * 2) escalonador_ordenar(s->esc, &s->atual, candidatos, n_candidatos).
     * 3) vencedores = min(n_candidatos, e->ncpus) primeiros da lista
     *    ordenada.
     * 4) quem estava EST_EXECUTANDO mas NÃO está entre os vencedores foi
     *    preemptado: solta a CPU (cpus[cpu].tarefa=-1), volta pra
     *    EST_PRONTA, mantendo o exec_restante que já tinha.
     * 5) vencedores que ainda não têm CPU (cpu==-1) recebem a primeira
     *    CPU livre: estado=EST_EXECUTANDO, cpu=essa CPU,
     *    cpus[essa].tarefa=índice da tarefa, quantum_restante =
     *    e->quantum (reinicia o quantum a cada nova atribuição).
     * 6) CPUs que sobraram sem ninguém: cpus[c].ticks_desligada++
     *    (requisito 1.2: acumular o período ocioso pro relatório final). */
    (void)s;
}

/* EXECUÇÃO — consome uma unidade de tempo de cada tarefa que está na CPU,
 * e trata tanto o término da ativação quanto o estouro do quantum
 * (requisito 3.3: "quantum indica o período máximo de tempo que uma tarefa
 * pode executar" antes de precisar ceder a CPU). */
static void executar(Simulacao *s)
{
    /* TODO: para cada tarefa EST_EXECUTANDO:
     *   exec_restante--
     *   se e->quantum > 0 e quantum_restante > 0: quantum_restante--
     *
     *   se exec_restante <= 0 (ativação concluída):
     *     solta a CPU, cpu=-1, quantum_restante=0, ativacoes++
     *     estado = EST_CONCLUIDA se ativacoes >= MAX_ATIVACOES (10, ver
     *     requisito 4.4), senão EST_INATIVA (aguarda a próxima ativação)
     *
     *   senão se quantum > 0 e quantum_restante <= 0 (quantum esgotado,
     *   ainda falta trabalho): solta a CPU, cpu=-1, estado=EST_PRONTA
     *   (ela volta a competir no próximo escalonar(), agora sem a
     *   vantagem do critério de desempate 1, já que não está mais
     *   EST_EXECUTANDO)
     *
     *   senão: nada muda, ela continua executando. */
    (void)s;
}

static void verificar_prazos(Simulacao *s)
{
    /* TODO: para cada tarefa que não seja EST_INATIVA/EST_CONCLUIDA, se
     * tick > deadline_abs e ainda não estava marcada (!perdeu_prazo):
     * marcar perdeu_prazo=1 (ela continua executando normalmente depois
     * disso -- só fica marcada pro gantt mostrar o erro, requisito 3.3). */
    (void)s;
}

/* ------------------------------ ciclo de vida ----------------------------- */

int sim_iniciar(Simulacao *s, Estado *inicial, Diagnostico *d)
{
    /* TODO:
     * 1) memset(s, 0, sizeof(*s)) pra começar limpo.
     * 2) s->esc = escalonador_buscar(inicial->algoritmo); se vier NULL,
     *    diag_erro(d, "algoritmo '%s' não reconhecido", ...) e devolver 0.
     * 3) s->atual = estado_clonar(inicial) -- cópia independente, pra
     *    quem chamou poder liberar o 'inicial' original sem afetar a
     *    simulação.
     * 4) chamar ativar(s) (ativações que já valem no tick 0) e depois
     *    escalonar(s) (primeira atribuição de CPUs).
     * 5) checar todas_concluidas(&s->atual) e ajustar s->terminada.
     * 6) snapshot(s) -- historico[0] = fotografia inicial (requisito
     *    1.5.2: o histórico começa no tick 0, não no tick 1).
     * 7) devolver 1. */
    (void)s;
    (void)inicial;
    (void)d;
    return 0;
}

int sim_avancar(Simulacao *s)
{
    /* TODO:
     * 1) se s->terminada, devolver 0 (nada a fazer).
     * 2) se s->atual.tick >= MAX_TICKS, marcar terminada=1 (rede de
     *    segurança contra loop infinito por bug de escalonamento) e
     *    devolver 0.
     * 3) na ordem do comentário no topo do arquivo: executar(s);
     *    s->atual.tick++; verificar_prazos(s); ativar(s); escalonar(s).
     * 4) checar todas_concluidas e ajustar terminada.
     * 5) snapshot(s).
     * 6) devolver 1. */
    (void)s;
    return 0;
}

int sim_retroceder(Simulacao *s)
{
    /* TODO: se n_hist < 2, devolver 0 (não há pra onde voltar -- o
     * primeiro snapshot é o tick 0). Senão: estado_liberar no último
     * snapshot do histórico (o do tick atual, que vamos descartar),
     * n_hist--, estado_liberar em s->atual, e então s->atual =
     * estado_clonar(&historico[n_hist-1]) (restaura o snapshot anterior).
     * Lembrar de zerar terminada=0 (retroceder sempre reabre a
     * simulação, mesmo que ela já tivesse terminado). Devolver 1. */
    (void)s;
    return 0;
}

void sim_executar_tudo(Simulacao *s)
{
    /* TODO: while (!s->terminada) sim_avancar(s); (parar se sim_avancar
     * devolver 0 por algum motivo além de 'terminada', pra não travar). */
    (void)s;
}

int sim_editar_tarefa(Simulacao *s, int id_tarefa, const char *campo,
                      const char *valor, char *motivo, int motivo_tam)
{
    /* TODO:
     * 1) achar a tarefa com sim_indice_por_id; se não existir,
     *    snprintf(motivo, ...) explicando e devolver 0.
     * 2) decidir quais campos são editáveis e validar cada um antes de
     *    aplicar (requisito 3.4: edição semanticamente inválida deve ser
     *    rejeitada com uma mensagem clara, não só ignorada). Pelo menos:
     *      - "estado": converter o texto pro enum (INATIVA/PRONTA/
     *        EXECUTANDO/SUSPENSA/CONCLUIDA, case-insensitive); não deixar
     *        virar EXECUTANDO sem CPU atribuída, e liberar a CPU se sair
     *        de EXECUTANDO pra outro estado.
     *      - "exec_restante": número entre 0 e a duração da tarefa.
     *      - "prazo": número >= 0 (e recalcular deadline_abs = ativacao +
     *        prazo).
     *    Sinta-se livre pra editar outros campos também, seguindo o
     *    mesmo padrão (validar -> aplicar -> devolver 1, ou motivo claro
     *    -> devolver 0).
     * 3) campo desconhecido: motivo claro e devolver 0. */
    (void)s;
    (void)id_tarefa;
    (void)campo;
    (void)valor;
    (void)motivo;
    (void)motivo_tam;
    return 0;
}

void sim_liberar(Simulacao *s)
{
    /* TODO: estado_liberar em cada entrada de s->historico, depois
     * free(s->historico) e zerar n_hist/cap_hist. Por fim,
     * estado_liberar(&s->atual). */
    (void)s;
}