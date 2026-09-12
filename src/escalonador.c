#include "escalonador.h"
#include "config.h"    

#include <stddef.h>

#define MAX_ALGORITMOS 8
static Escalonador tabela[MAX_ALGORITMOS];
static int         n_algoritmos = 0;

void escalonador_registrar(const Escalonador *esc)
{

}

int                escalonador_qtde(void)      {  }
const Escalonador *escalonador_em(int i)       {    }

const Escalonador *escalonador_buscar(const char *nome)
{

}

static long prio_rm(const TCB *t, int tick)
{

}

static long prio_edf(const TCB *t, int tick)
{

}

void escalonador_registrar_padroes(void)
{

}

static int sorteio(int tick, int ida, int idb)
{

}

static int empate_ate_sorteio(const Escalonador *esc, const TCB *a, const TCB *b, int tick)
{

}

static int comparar(const Escalonador *esc, const TCB *a, const TCB *b, int tick)
{

}

void escalonador_ordenar(const Escalonador *esc, Estado *e, int *indices, int n)
{
    }