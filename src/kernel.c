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
 * ==========================================================================*/
#include "kernel.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------- cópia profunda do estado ---------------------- */

Estado estado_clonar(const Estado *e)
{

}

void estado_liberar(Estado *e)
{

}

const char *estado_nome(EstadoTarefa e)
{
    switch (e) {
        case EST_INATIVA:    return "INATIVA";
        case EST_PRONTA:     return "PRONTA";
        case EST_EXECUTANDO: return "EXECUTANDO";
        case EST_SUSPENSA:   return "SUSPENSA";
        case EST_CONCLUIDA:  return "CONCLUIDA";
    }
 
}

int sim_indice_por_id(const Estado *e, int id)
{

}

/* ------------------------------- histórico ------------------------------- */

/* Guarda a fotografia do tick corrente em historico[tick].
 *
 * ponytail: guardamos o estado INTEIRO a cada tick, não um diário de
 * diferenças. Custo = ntarefas * ticks * sizeof(TCB) — para 20 tarefas e
 * 5000 ticks dá poucos megabytes, e em troca retroceder é um memcpy.
 * Trocar por undo-log só se a simulação precisar de milhões de ticks. */
static void snapshot(Simulacao *s)
{

}

/* ESCALONAMENTO — requisito 1.2.
 * A fila de prontos é GLOBAL (um único vetor de candidatas para todas as
 * CPUs, não uma fila por processador) e a ociosidade é minimizada: só sobra
 * CPU desligada se realmente não houver tarefa pronta. */
static void escalonar(Simulacao *s)
{
}

/* EXECUÇÃO — consome uma unidade de tempo de cada tarefa que está na CPU. */
static void executar(Simulacao *s)
{

}

static void verificar_prazos(Simulacao *s)
{

}

int sim_avancar(Simulacao *s)
{

}

int sim_retroceder(Simulacao *s)
{
}

void sim_executar_tudo(Simulacao *s)
{

}

int sim_editar_tarefa(Simulacao *s, int id_tarefa, const char *campo,
                      const char *valor, char *motivo, int motivo_tam)
{

}

void sim_liberar(Simulacao *s)
{

}
