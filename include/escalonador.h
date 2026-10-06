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
 * precisa saber. Um algoritmo e UMA funcao (o campo 'prioridade' abaixo).
 *
 * PARA INCLUIR UM ALGORITMO NOVO (nenhuma linha de kernel.c muda):
 *   1. escrever uma funcao  long minha_prio(const TCB *t, int tick);
 *   2. registra-la em escalonador_registrar_padroes(), com um nome;
 *   3. usar esse nome na primeira linha do arquivo de configuracao.
 *
 * Como o registro e feito por uma chamada de funcao em tempo de execucao (e
 * nao por uma tabela fixa em tempo de compilacao), o mesmo mecanismo serviria
 * para um algoritmo vindo de uma biblioteca dinamica, como o requisito 4.2
 * sugere: bastaria a biblioteca chamar escalonador_registrar().
 * ==========================================================================*/

typedef struct {
    /* Nome usado no arquivo de configuracao (ex. "RM"). A comparacao ignora
     * maiusculas/minusculas (requisito 3.3.2). */
    const char *nome;

    /* Texto para mostrar ao usuario na interface. */
    const char *descricao;

    /* O ALGORITMO. Devolve a prioridade da tarefa 't' no instante 'tick'.
     *
     * CONVENCAO (vale para todo algoritmo do projeto):
     *     valor MAIOR = tarefa MAIS prioritaria.
     * Algoritmos em que "o menor ganha" trocam o sinal:
     *     RM  -> -(periodo)         periodo menor     => valor maior
     *     EDF -> -(deadline_abs)    deadline mais cedo => valor maior
     *
     * 'tick' existe para algoritmos cuja prioridade depende do instante
     * atual. RM e EDF nao precisam dele e o ignoram.
     *
     * A funcao deve apenas CALCULAR: nao pode alterar a tarefa (por isso
     * 'const') nem guardar estado entre chamadas. O mesmo (t, tick) tem que
     * dar sempre o mesmo valor -- e disso que depende poder retroceder e
     * avancar a simulacao com o mesmo resultado (requisito 1.5.2).
     *
     * Empates NAO sao problema do algoritmo: quando dois valores sao iguais,
     * escalonador_ordenar aplica os criterios do requisito 4.3. */
    long (*prioridade)(const TCB *t, int tick);
} Escalonador;

/* Acrescenta um algoritmo a tabela. Copia a struct, mas NAO os textos: so os
 * ponteiros 'nome' e 'descricao' sao copiados. Por isso eles precisam apontar
 * para memoria que dure o programa inteiro -- na pratica, strings literais.
 * Passar um buffer local deixaria a tabela apontando para lixo.
 *
 * A tabela tem tamanho fixo; 'esc' NULL ou registro alem do limite e
 * ignorado em silencio. */
void escalonador_registrar(const Escalonador *esc);

/* Registra os algoritmos que acompanham o simulador (RM e EDF, requisito 4).
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
