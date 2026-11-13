#ifndef ESCALONADOR_H
#define ESCALONADOR_H

#include "tcb.h"

/* ============================================================================
 * escalonador.h — a interface PLUGAVEL dos algoritmos de escalonamento
 * ----------------------------------------------------------------------------
 * Requisito 4.2: novos algoritmos devem poder ser incluidos "sem a
 * necessidade de modificar o codigo da simulacao. Idealmente, o escalonador e
 * apenas uma funcao que retorna qual e a proxima tarefa a ser executada".
 *
 * COMO ISSO E ATENDIDO
 * O kernel nunca menciona "RM" nem "EDF". Ele faz duas coisas so:
 *   1. ao carregar:   s->esc = escalonador_buscar(nome_lido_do_arquivo);
 *   2. a cada tick:   escalonador_ordenar(s->esc, ...);
 * Qual algoritmo esta por tras do ponteiro s->esc, o kernel nao sabe e nao
 * precisa saber.
 *
 * UM ALGORITMO E DADO, NAO CODIGO: uma lista de criterios (campo do TCB +
 * "menor" ou "maior" ganha) e a flag preemptivo. Por isso o USUARIO inclui
 * um algoritmo novo com o programa rodando, sem recompilar:
 *   1. escreve a definicao -- num .txt ou no formulario "novo algoritmo":
 *          FIFO;First In First Out;nao     <- nome;descricao;preemptivo
 *          ativacao;menor                  <- um criterio por linha
 *   2. a pagina manda o texto para escalonador_carregar_texto();
 *   3. o nome passa a valer na primeira linha do arquivo de configuracao e
 *      no seletor da pagina, ate o programa fechar.
 * RM e EDF sao registrados por esse mesmo caminho.
 *
 * LIMITE ASSUMIDO: criterios por campo cobrem RM, EDF, FIFO, SJF, DM...
 * Algoritmo que exige conta (ex. menor folga) nao cabe em criterios; para
 * esse caso ficou o campo opcional 'prioridade' (uma funcao em C), que
 * serviria tambem a um algoritmo vindo de biblioteca dinamica (req. 4.2).
 * ==========================================================================*/

/* Quantos criterios um algoritmo pode ter. O 2o so e consultado se o 1o
 * empatar, e assim por diante; depois deles vem o desempate do req. 4.3. */
#define MAX_CRITERIOS 4

typedef struct {
    int campo;        /* índice na tabela de campos (escalonador.c) */
    int maior_ganha;  /* 0 = menor valor ganha, 1 = maior valor ganha */
} Criterio;

typedef struct {
    char nome[16];        /* cópia: o nome pode vir digitado pelo usuário */
    char descricao[64];
    int  preemptivo;      /* 0 = quem está na CPU não é tirado por outra tarefa */
    int  n_criterios;
    Criterio criterios[MAX_CRITERIOS];
    /* Opcional: algoritmo escrito em C. NULL = usa a lista de critérios. */
    long (*prioridade)(const TCB *t, int tick);
} Escalonador;

/* Lê a definição de um algoritmo em texto (formato no topo deste arquivo)
 * e o registra. Devolve 1 se registrou; 0 se recusou, com o porquê em
 * 'motivo' (nome repetido, campo que nao existe, sem criterio...). */
int escalonador_carregar_texto(const char *texto, char *motivo, int motivo_tam);

/* Acrescenta um algoritmo ja montado a tabela, copiando a struct inteira
 * (nome e descricao vao junto: sao vetores dentro dela). Uso normal:
 * escalonador_carregar_texto, que valida antes de chamar esta.
 *
 * A tabela tem tamanho fixo; 'esc' NULL ou registro alem do limite e
 * ignorado em silencio. */
void escalonador_registrar(const Escalonador *esc);

/* Registra os algoritmos que acompanham o simulador (RM e EDF, requisito 4),
 * pelo mesmo texto que o usuario usaria.
 * Tem que ser chamada antes de qualquer escalonador_buscar -- o main.c faz
 * isso na primeira linha. */
void escalonador_registrar_padroes(void);

/* Procura pelo nome, ignorando maiusculas/minusculas. Devolve um ponteiro
 * para a tabela interna (nao dar free) ou NULL se nao existir: quem chamou e
 * que informa o erro ao usuario, listando os algoritmos disponiveis
 * (requisito 5). */
const Escalonador *escalonador_buscar(const char *nome);

/* Para listar os algoritmos disponiveis (mensagens de erro, interface).
 * escalonador_em exige 0 <= i < escalonador_qtde(). */
int                escalonador_qtde(void);
const Escalonador *escalonador_em(int i);

/* Ordena 'indices' (posicoes em e->tarefas) da tarefa MAIS prioritaria para
 * a menos, usando esc->prioridade e, em caso de empate, os criterios do
 * requisito 4.3. E a unica funcao que o kernel chama a cada tick: as
 * 'ncpus' primeiras posicoes do resultado ganham processador. */
void escalonador_ordenar(const Escalonador *esc, Estado *e, int *indices, int n);

#endif
