#ifndef TCB_H
#define TCB_H

/* ============================================================================
 * tcb.h — as estruturas de dados do sistema simulado
 *   TCB    .... tudo o que se sabe de UMA tarefa (req. 1.3)
 *   CPU    .... um processador
 *   Estado .... a "fotografia" do sistema inteiro num tick (relógio, tarefas
 *               e CPUs); o histórico guarda uma por tick (req. 1.5.2)
 * Notação do livro de tempo real: Ci = duracao, Pi = periodo, Di = prazo.
 * ==========================================================================*/

/* Toda tarefa periódica termina depois de 10 execuções (req. 4.4). */
#define MAX_ATIVACOES 10

/* Tamanho do texto da lista de eventos (usada só no Projeto B). */
#define MAX_EVENTOS_TXT 256

/* Os cinco estados de uma tarefa (ciclo de vida, Maziero 4.4). */
typedef enum {
    EST_INATIVA = 0, /* esperando a próxima chegada (ou antes do ingresso) */
    EST_PRONTA,      /* chegou e espera CPU na fila global                 */
    EST_EXECUTANDO,  /* ocupa uma CPU                                      */
    EST_SUSPENSA,    /* parada; no Projeto A só pela edição manual         */
    EST_CONCLUIDA    /* fez as 10 execuções                                */
} EstadoTarefa;

/* O nome do estado como texto ("PRONTA"...), para a página e a edição. */
const char *estado_nome(EstadoTarefa e);


/* TCB — Task Control Block: UMA struct com tudo sobre a tarefa, antes,
 * durante e depois da simulação (req. 1.3). Não há campo de prioridade: ela
 * é calculada pelo algoritmo a cada tick (escalonador.c). */
typedef struct {
    /* ---- vem do arquivo de configuração (não muda na simulação, exceto
     *      duracao e prazo, que a edição manual pode alterar) ---- */
    int  id;                         /* identificador, como no arquivo      */
    unsigned char r, g, b;           /* cor no Gantt (RRGGBB)               */
    int  ingresso;                   /* tick em que entra no sistema        */
    int  duracao;                    /* Ci: ticks de CPU por execução       */
    int  periodo;                    /* Pi: chega uma execução a cada Pi    */
    int  prazo;                      /* Di: prazo contado da chegada        */
    char eventos[MAX_EVENTOS_TXT];   /* lista de eventos (Projeto B)        */

    /* ---- muda a cada tick (núcleo, kernel.c) ---- */
    EstadoTarefa estado;
    int  exec_restante;     /* quanto falta da execução atual (Ci rest.)     */
    int  quantum_restante;  /* quanto falta do quantum na CPU atual          */
    int  ativacao;          /* tick da chegada da execução atual             */
    int  deadline_abs;      /* prazo absoluto = ativacao + prazo             */
    int  ativacoes;         /* execuções já concluídas (a 10ª encerra)       */
    int  perdeu_prazo;      /* 1 se a execução atual passou do prazo (✖)     */
    int  cpu;               /* CPU em uso, ou -1 = nenhuma                   */
    int  sorteada;          /* 1 se ganhou a CPU num sorteio (★ no Gantt)    */
    int  pendentes;   /* execuções que chegaram enquanto a anterior ainda não
                       * tinha terminado; esperam a vez, em ordem de chegada */
} TCB;


/* Um processador. */
typedef struct {
    int id;               /* número da CPU (0, 1, ...)                       */
    int tarefa;           /* índice no vetor de tarefas, ou -1 = DESLIGADA   */
    int ticks_desligada;  /* ticks sem tarefa desde o início (req. 1.2)      */
} CPU;


/* Fotografia do sistema num tick: tudo o que muda com o tempo. Os dois
 * vetores são alocados; por isso copiar exige estado_clonar (cópia
 * profunda), e não '='. */
typedef struct {
    char algoritmo[16];   /* "RM" ou "EDF"; vai junto em cada foto, para a
                           * troca de algoritmo ser desfeita ao retroceder  */
    int  quantum;         /* 0 = sem limite                                  */
    int  ncpus;
    int  ntarefas;
    int  tick;            /* o relógio global (req. 1.1)                     */
    TCB *tarefas;         /* vetor com ntarefas TCBs                         */
    CPU *cpus;            /* vetor com ncpus CPUs                            */
} Estado;

/* Cópia profunda (também os vetores). Se faltar memória, o vetor que falhou
 * volta vazio, com o contador em 0. */
Estado estado_clonar(const Estado *e);
/* Libera os vetores e zera o Estado; seguro chamar duas vezes. */
void   estado_liberar(Estado *e);

#endif
