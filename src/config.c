#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

void diag_erro(Diagnostico *d, const char *fmt, ...)
{

}

void diag_aviso(Diagnostico *d, const char *fmt, ...)
{

}

int str_igual_ci(const char *a, const char *b)
{

}

static char *trim(char *s)
{

}

static void str_maiuscula(char *s)
{

}

static int split_campos(char *linha, char *campos[], int max)
{

}

static int campo_int(const char *txt, int padrao, int *ok)
{

}

static int campo_cor(const char *txt, TCB *t)
{

}

int config_carregar(const char *caminho, Estado *e, Diagnostico *d)
{
}
