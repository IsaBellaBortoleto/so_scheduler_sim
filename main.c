/* ============================================================================
 * main.c — Ponto de entrada do simulador
 * ----------------------------------------------------------------------------
 * Com a interface em HTML, o main.c deixa de SER a interface: quem mostra as
 * tarefas, o Gantt, os erros do arquivo e os botoes e a pagina em web/,
 * servida pelo proprio executavel (servidor.h). O main so monta as pecas e
 * sai do caminho:
 *
 *   1. registra os algoritmos de escalonamento;
 *   2. atende --teste, se pedido;
 *   3. cria a Simulacao VAZIA -- o arquivo de configuracao e escolhido pelo
 *      usuario na pagina (POST /api/carregar), nao aqui;
 *   4. abre o navegador e entrega o controle ao servidor, que so devolve
 *      quando o programa for encerrado;
 *   5. libera a memoria da simulacao.
 *
 * O QUE SAIU DAQUI E POR QUE:
 * mostrar_diagnostico, mostrar_tarefas, mostrar_ajuda e mostrar_resumo eram
 * a interface de TEXTO. Na versao web cada uma virou um painel da pagina
 * (area-diagnostico, painel-tarefas, painel-cpus...), alimentado pelo JSON de
 * estado_json.h. Deixa-las aqui seria codigo morto concorrendo com a pagina:
 * duas implementacoes do mesmo painel, uma delas nunca executada.
 * ==========================================================================*/
#include "kernel.h"        /* traz tcb.h, config.h e escalonador.h */
#include "gantt.h"
#include "estado_json.h"
#include "servidor.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

/* Porta do servidor local. Fixa porque o usuario nunca precisa digita-la:
 * o proprio programa abre o navegador ja no endereco certo. */
#define PORTA_PADRAO 8080

/* ==========================================================================
 *  AUTOTESTE — roda com  simulador --teste
 *  Verifica as partes com logica de verdade. Se algo for quebrado numa
 *  refatoracao, o assert avisa na hora, com arquivo e linha.
 * ========================================================================*/
static void autoteste(void)
{
    /* TODO: montar um arquivo de configuracao temporario com fopen/fprintf
     * que exercite, de uma vez, as regras do parser que mais quebram:
     *   - algoritmo em minusculas              (req. 3.3.2)
     *   - linha terminando com ';'              (req. 3.3.3)
     *   - linha em branco e espacos nos campos  (req. 3.3.6)
     *   - uma tarefa aperiodica (periodo = 0)   (req. 4.4: ignorada + aviso)
     * e conferir com assert o que config_carregar extraiu dele.
     *
     * TODO: iniciar a simulacao com esse Estado e conferir:
     *   - sim_executar_tudo termina com MAX_ATIVACOES em cada tarefa;
     *   - DETERMINISMO: retroceder 2 ticks e avancar 2 volta exatamente ao
     *     mesmo estado (e a garantia da qual o requisito 1.5.2 depende);
     *   - sim_editar_tarefa RECUSA uma edicao invalida (ex. prazo 0) e
     *     preenche o motivo (req. 3.4);
     *   - gantt_svg consegue gravar o arquivo.
     *
     * TODO: depois que estado_json.c existir, conferir tambem que
     * estado_json_atual devolve -1 com um buffer pequeno demais em vez de
     * escrever pela metade -- e o contrato documentado em estado_json.h, e
     * um JSON cortado quebraria a pagina sem mensagem nenhuma.
     *
     * No fim: remove() nos arquivos temporarios, sim_liberar, e um printf
     * dizendo que tudo passou. */
}

/* ========================================================================== */

int main(int argc, char **argv)
{
    Simulacao sim;
    int porta = PORTA_PADRAO;

    /* Tem que ser a PRIMEIRA coisa. config_carregar/sim_iniciar procuram o
     * algoritmo do arquivo pelo nome na tabela de escalonadores; com a
     * tabela vazia, todo arquivo valido falharia com "algoritmo inexistente"
     * -- e o erro apontaria para o arquivo, nao para a causa real. */
    escalonador_registrar_padroes();

    if (argc >= 2 && strcmp(argv[1], "--teste") == 0) {
        autoteste();
        return 0;
    }

    /* Simulacao zerada significa "nenhum arquivo carregado ainda". O
     * servidor precisa reconhecer esse caso: um /api/avancar antes de
     * /api/carregar deve responder com um erro claro (req. 5), nao
     * dereferenciar lixo. Com memset, esc == NULL e n_hist == 0 servem
     * exatamente como esse sinal. */
    memset(&sim, 0, sizeof sim);

    /* TODO: imprimir no console o endereco http://localhost:<porta>/.
     * Se o navegador nao abrir sozinho(antivirus, navegador padrao nao
     * configurado), essa linha e a unica forma de o usuario descobrir onde
     * entrar -- a janela do console e a unica interface garantida. */

    /* TODO: servidor_abrir_navegador(porta);
     *
     * ATENCAO a ordem: servidor_iniciar BLOQUEIA (roda o laco de accept ate
     * o programa fechar), entao o navegador precisa ser aberto ANTES dele.
     * Isso cria uma corrida: se o navegador pedir a pagina antes de o
     * servidor terminar o bind/listen, aparece "conexao recusada". Na
     * pratica abrir o navegador demora mais que o bind, mas nao ha garantia.
     * A correcao definitiva e dividir servidor.h em duas funcoes -- uma que
     * abre a porta e retorna, outra que roda o laco -- e abrir o navegador
     * entre as duas. */

    /* TODO: if (!servidor_iniciar(&sim, porta)) { ... }
     * Retorno 0 = nao conseguiu abrir a porta. O caso comum e o proprio
     * simulador ja estar aberto em outra janela, entao a mensagem deve dizer
     * isso (req. 5: motivo claro), liberar a simulacao e retornar 1. */
    (void)porta;

    sim_liberar(&sim);
    return 0;
}
