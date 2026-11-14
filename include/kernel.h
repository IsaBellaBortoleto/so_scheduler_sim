#ifndef KERNEL_H
#define KERNEL_H

#include "tcb.h"
#include "escalonador.h"
#include "config.h"

/* Limite de ticks (req. 2.4: a simulação não pode ficar presa a um tempo
 * máximo fixo). O limite de cada simulação é calculado a partir do arquivo,
 * em sim_iniciar (ver calcular_limite em kernel.c), e fica sempre depois do
 * fim natural. MAX_TICKS é só o mínimo dele; MAX_MEMORIA_HIST é o máximo de
 * memória que o histórico (uma foto por tick) pode ocupar. */
#define MAX_TICKS 5000
#define MAX_MEMORIA_HIST (1024LL * 1024 * 1024) /* 1 GB */


typedef struct {
    Estado  atual;
    Estado *historico;
    int     n_hist, cap_hist;
    const Escalonador *esc;
    int     terminada;
    int     limite_ticks; /* calculado em sim_iniciar, a partir do arquivo */
    char    ultimo_evento[256];
    int     precisa_escalonar; /* 1 = houve evento: reescalonar (kernel.c) */
} Simulacao;


int  sim_iniciar(Simulacao *s, Estado *inicial, Diagnostico *d);

int  sim_avancar(Simulacao *s);

int  sim_retroceder(Simulacao *s);

void sim_executar_tudo(Simulacao *s);

/* id_tarefa e o id em TEXTO, como no arquivo (ex. "t06"). */
int  sim_editar_tarefa(Simulacao *s, const char *id_tarefa, const char *campo,
                       const char *valor, char *motivo, int motivo_tam);

void sim_liberar(Simulacao *s);

int  sim_indice_por_id(const Estado *e, const char *id);
/* Troca o algoritmo de escalonamento no meio da simulação (req. 3.1).
 * Vale a partir do próximo tick; no tick 0 (nada executou ainda) vale já
 * para o primeiro intervalo. Devolve 1 se trocou; 0 se recusou, com o
 * porquê em 'motivo'. */
int sim_trocar_algoritmo(Simulacao *s, const char *nome,
                         char *motivo, int motivo_tam);

/* Cópia da simulação levada até o fim, para exportar o Gantt INTEIRO sem
 * mexer na simulação da tela (req. 2.4). Devolve 1 se deu certo, 0 se
 * faltou memória. Liberar SEMPRE com sim_liberar_copia. */
int  sim_copiar_ate_o_fim(const Simulacao *s, Simulacao *copia);
void sim_liberar_copia(const Simulacao *s, Simulacao *copia);

/* Teto da quantidade de CPUs ao alterar durante a simulação. Não é limite
 * do enunciado ("no máximo N"): só impede que um número absurdo digitado
 * na tela vá direto para o realloc. */
#define MAX_CPUS 256

/* Muda a quantidade de CPUs com a simulação carregada (req. geral 2 e 3.1:
 * configurar antes e durante). Ao reduzir, as tarefas das CPUs retiradas
 * voltam para a fila de prontas; em seguida o escalonador redistribui.
 * Vale já para o intervalo do tick atual, e retroceder desfaz.
 * Devolve 1 se mudou; 0 se recusou, com o porquê em 'motivo'. */
int sim_alterar_cpus(Simulacao *s, int n, char *motivo, int motivo_tam);
#endif 
