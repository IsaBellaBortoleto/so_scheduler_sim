#include "config.h"
#include "escalonador.h"
#include "kernel.h"
#include "gantt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>


static void mostrar_diagnostico(const Diagnostico *d)
{
    int i;
    for (i = 0; i < d->n_avisos; i++) printf("  [AVISO] %s\n", d->avisos[i]);
    for (i = 0; i < d->n_erros;  i++) printf("  [ERRO ] %s\n", d->erros[i]);
}

static void mostrar_tarefas(const Simulacao *s)
{
}

static void mostrar_ajuda(void)
{

}

static void mostrar_resumo(const Simulacao *s)
{

}


static void autoteste(void)
{
    
}


int main(int argc, char **argv)
{
 
}
