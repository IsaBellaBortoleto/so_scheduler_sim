#include "gantt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <windows.h>   
#endif

typedef enum {
    MARCA_NENHUMA = 0,
    MARCA_SORTEIO,     
    MARCA_CHEGADA,     
    MARCA_TERMINO,    
    MARCA_PRAZO        /
} Marca;


static Marca marca_em(const Simulacao *s, int t, int i)
{

}


static void ordem_por_id(const Estado *e, int *ordem)
{

}



void gantt_preparar_terminal(void)
{

}


static void cor_fundo(unsigned char r, unsigned char g, unsigned char b)
{

}
static void cor_reset(void) { printf("\x1b[0m"); }

static char simbolo(Marca m)
{

}

void gantt_terminal(const Simulacao *s, int janela)
{

}


#define CEL_W  14   /* largura de 1 tick, em pixels                          */
#define LIN_H  22   /* altura de uma linha do gráfico                        */
#define MARG_E 90   /* margem esquerda, onde vão os rótulos das linhas       */
#define MARG_T 50   /* margem superior, para o título                        */

int gantt_svg(const Simulacao *s, const char *caminho)
{
  
}
