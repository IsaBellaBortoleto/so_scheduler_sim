#ifndef SERVIDOR_H
#define SERVIDOR_H

#include "kernel.h"

//SUPONDO INTERFACE EM HTTP
/* Servidor HTTP embutido (so sockets -- Winsock no Windows, BSD
 * sockets no Linux/Mac -- sem nenhuma biblioteca externa) que expoe a
 * Simulacao pra interface web. O servidor nao guarda estado proprio:
 * cada requisicao le/comanda a MESMA struct Simulacao que o main.c
 * mantem viva durante toda a execucao.
 *
 * 'sim' precisa continuar valido durante toda a vida do servidor
 * (normalmente e uma variavel local de main() cujo endereco e passado
 * aqui). Bloqueia a thread chamadora rodando o loop de accept ate o
 * processo ser encerrado. Retorna 0 se nao conseguiu abrir a porta
 * (ex. porta ja em uso), 1 em encerramento normal. */
int servidor_iniciar(Simulacao *sim, int porta);


void servidor_abrir_navegador(int porta);

#endif