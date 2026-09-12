#ifndef KERNEL_H
#define KERNEL_H

#include "tcb.h"
#include "escalonador.h"
#include "config.h"

#define MAX_TICKS 5000


typedef struct {
    Estado  atual;
    Estado *historico;      
    int     n_hist, cap_hist;
    const Escalonador *esc;
    int     terminada;
    char    ultimo_evento[256];
} Simulacao;


int  sim_iniciar(Simulacao *s, Estado *inicial, Diagnostico *d);

int  sim_avancar(Simulacao *s);

int  sim_retroceder(Simulacao *s);

void sim_executar_tudo(Simulacao *s);

int  sim_editar_tarefa(Simulacao *s, int id_tarefa, const char *campo,
                       const char *valor, char *motivo, int motivo_tam);

void sim_liberar(Simulacao *s);

int  sim_indice_por_id(const Estado *e, int id);

#endif 
