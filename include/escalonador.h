#ifndef ESCALONADOR_H
#define ESCALONADOR_H

#include "tcb.h"

typedef struct {
    const char *nome;       
    const char *descricao;  
    long (*prioridade)(const TCB *t, int tick);
} Escalonador;

void escalonador_registrar(const Escalonador *esc);

void escalonador_registrar_padroes(void);


const Escalonador *escalonador_buscar(const char *nome);


int                escalonador_qtde(void);
const Escalonador *escalonador_em(int i);


void escalonador_ordenar(const Escalonador *esc, Estado *e, int *indices, int n);

#endif 
