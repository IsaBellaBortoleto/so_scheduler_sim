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
 *      usuario no seletor de arquivo da pagina, que envia o conteudo
 *      (POST /api/carregar). Funciona direto do pendrive do professor,
 *      sem copiar nada para a maquina;
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

/* Algoritmo inventado, so para o teste: prova que um escalonador novo entra
 * no sistema sem que nenhuma linha do kernel mude (requisito 4.2). */
static long prio_teste(const TCB *t, int tick)
{
    (void)t;
    (void)tick;
    return 42;
}

/* Interface plugavel do escalonador (escalonador.h). */
static void teste_escalonador(void)
{
    const Escalonador *achado;
    Escalonador novo = { "TESTE", "algoritmo inventado para o autoteste", prio_teste };

    /* Comparacao sem diferenciar maiusculas (requisito 3.3.2). O caso
     * "RM" x "RMS" e o que pega uma comparacao que para na string mais curta. */
    assert(str_igual_ci("RM", "rm") == 1);
    assert(str_igual_ci("RM", "RMS") == 0);
    assert(str_igual_ci("RMS", "RM") == 0);
    assert(str_igual_ci(NULL, "RM") == 0);

    /* O main ja chamou escalonador_registrar_padroes(): RM e EDF tem que
     * estar na tabela e ser encontrados com qualquer combinacao de caixa. */
    assert(escalonador_qtde() >= 2);
    assert(escalonador_buscar("rm") != NULL);
    assert(escalonador_buscar("Edf") != NULL);
    assert(escalonador_buscar("RMS") == NULL);   /* nome parecido nao serve */
    assert(escalonador_buscar(NULL) == NULL);

    /* Rate Monotonic: periodo MENOR tem que dar prioridade MAIOR (a
     * convencao do projeto e "valor maior ganha"), e a prioridade e FIXA:
     * nao pode depender do tick. */
    {
        const Escalonador *rm = escalonador_buscar("RM");
        TCB rapida = {0}, lenta = {0};
        rapida.periodo = 6;
        lenta.periodo  = 9;
        assert(rm->prioridade(&rapida, 0) > rm->prioridade(&lenta, 0));
        assert(rm->prioridade(&rapida, 0) == rm->prioridade(&rapida, 999));
    }

    /* Earliest Deadline First: deadline absoluto MAIS CEDO tem que dar
     * prioridade MAIOR. A prioridade e DINAMICA, mas quem a faz mudar e o
     * campo deadline_abs (reescrito pela ativar() do kernel a cada periodo),
     * e nao o tick: com o mesmo deadline, o valor e igual em qualquer tick. */
    {
        const Escalonador *edf = escalonador_buscar("EDF");
        TCB cedo = {0}, tarde = {0};
        long antes;
        cedo.deadline_abs  = 15;
        tarde.deadline_abs = 20;
        assert(edf->prioridade(&cedo, 0) > edf->prioridade(&tarde, 0));
        assert(edf->prioridade(&cedo, 0) == edf->prioridade(&cedo, 999));

        antes = edf->prioridade(&cedo, 0);
        cedo.deadline_abs = 30;                    /* nova ativacao: prazo novo */
        assert(edf->prioridade(&cedo, 0) < antes); /* ficou menos urgente      */
        assert(edf->prioridade(&tarde, 0) > edf->prioridade(&cedo, 0));  /* a ordem virou */
    }

    /* Ordenacao e criterios de desempate (requisito 4.3). 'ind' guarda
     * POSICOES no vetor de tarefas; depois de ordenar, ind[0] e a posicao da
     * tarefa mais prioritaria. */
    {
        const Escalonador *rm = escalonador_buscar("RM");
        TCB t[3];
        Estado e;
        int ind[3], primeiro;

        memset(t, 0, sizeof t);
        memset(&e, 0, sizeof e);
        e.tarefas = t; e.ntarefas = 3; e.ncpus = 1;
        t[0].id = 1; t[1].id = 2; t[2].id = 3;

        /* Sem empate: periodos 12, 6 e 9 -> posicoes na ordem 1, 2, 0. */
        t[0].periodo = 12; t[1].periodo = 6; t[2].periodo = 9;
        ind[0] = 0; ind[1] = 1; ind[2] = 2;
        escalonador_ordenar(rm, &e, ind, 3);
        assert(ind[0] == 1 && ind[1] == 2 && ind[2] == 0);

        /* Daqui em diante todas tem o MESMO periodo: o RM empata, e so os
         * criterios de desempate decidem. Cada bloco muda UM campo. */
        t[0].periodo = t[1].periodo = t[2].periodo = 10;
        t[0].prazo   = t[1].prazo   = t[2].prazo   = 10;
        t[0].duracao = t[1].duracao = t[2].duracao = 2;

        t[2].estado = EST_EXECUTANDO;            /* (1) quem ja executava */
        ind[0] = 0; ind[1] = 1; ind[2] = 2;
        escalonador_ordenar(rm, &e, ind, 3);
        assert(ind[0] == 2);
        t[2].estado = EST_INATIVA;

        t[1].prazo = 5;                          /* (2) menor prazo */
        ind[0] = 0; ind[1] = 1; ind[2] = 2;
        escalonador_ordenar(rm, &e, ind, 3);
        assert(ind[0] == 1);
        t[1].prazo = 10;

        t[0].ingresso = 4; t[1].ingresso = 4;    /* (3) quem chegou antes */
        ind[0] = 0; ind[1] = 1; ind[2] = 2;
        escalonador_ordenar(rm, &e, ind, 3);
        assert(ind[0] == 2);
        t[0].ingresso = 0; t[1].ingresso = 0;

        t[0].duracao = 1;                        /* (4) menor duracao */
        ind[0] = 0; ind[1] = 1; ind[2] = 2;
        escalonador_ordenar(rm, &e, ind, 3);
        assert(ind[0] == 0);
        t[0].duracao = 2;

        /* (5) empate em tudo: o sorteio decide. O vencedor tem que ser o
         * mesmo se as duas tarefas entrarem na ordem contraria, e o mesmo
         * se a ordenacao for repetida (a simulacao retrocede e avanca). */
        ind[0] = 0; ind[1] = 1;
        escalonador_ordenar(rm, &e, ind, 2);
        primeiro = ind[0];
        ind[0] = 1; ind[1] = 0;
        escalonador_ordenar(rm, &e, ind, 2);
        assert(ind[0] == primeiro);
        escalonador_ordenar(rm, &e, ind, 2);
        assert(ind[0] == primeiro);

        /* Marcador do sorteio (4.3-5): com 1 CPU so a vencedora e marcada;
         * com 2 CPUs as duas executam, ninguem ganhou nada na sorte, e a
         * marca antiga tem que ser apagada. */
        assert(t[ind[0]].sorteada == 1 && t[ind[1]].sorteada == 0);
        e.ncpus = 2;
        escalonador_ordenar(rm, &e, ind, 2);
        assert(t[0].sorteada == 0 && t[1].sorteada == 0);
        e.ncpus = 1;

        /* Tres empatadas, 1 CPU: so a que ficou com a vaga e marcada. A
         * segunda tambem empata com a terceira, mas as duas estao esperando. */
        ind[0] = 0; ind[1] = 1; ind[2] = 2;
        escalonador_ordenar(rm, &e, ind, 3);
        assert(t[ind[0]].sorteada == 1);
        assert(t[ind[1]].sorteada == 0 && t[ind[2]].sorteada == 0);

        /* Tres empatadas, 2 CPUs: as duas com vaga ganharam da terceira na
         * sorte, entao as duas sao marcadas. */
        e.ncpus = 2;
        ind[0] = 0; ind[1] = 1; ind[2] = 2;
        escalonador_ordenar(rm, &e, ind, 3);
        assert(t[ind[0]].sorteada == 1 && t[ind[1]].sorteada == 1);
        assert(t[ind[2]].sorteada == 0);
        e.ncpus = 1;

        /* Quem ganhou a vaga por um criterio (aqui, menor duracao) NAO e
         * marcada: nao houve sorteio na disputa pela CPU. */
        t[0].duracao = 1;
        ind[0] = 0; ind[1] = 1; ind[2] = 2;
        escalonador_ordenar(rm, &e, ind, 3);
        assert(ind[0] == 0);
        assert(t[0].sorteada == 0 && t[1].sorteada == 0 && t[2].sorteada == 0);
        t[0].duracao = 2;

        /* Marca antiga em tarefa que nem e mais candidata (terminou): tem
         * que ser apagada mesmo estando fora de 'ind'. */
        t[2].sorteada = 1;
        ind[0] = 0; ind[1] = 1;
        escalonador_ordenar(rm, &e, ind, 2);
        assert(t[2].sorteada == 0);

        /* Vetor vazio ou com uma tarefa: nao pode quebrar. */
        escalonador_ordenar(rm, &e, ind, 0);
        escalonador_ordenar(rm, &e, ind, 1);
    }

    /* Um algoritmo novo: registra, e a busca passa a encontra-lo. A chamada
     * fica FORA do assert de proposito -- se o programa for compilado com
     * -DNDEBUG os asserts somem, e o registro sumiria junto. */
    escalonador_registrar(&novo);
    achado = escalonador_buscar("teste");
    assert(achado != NULL);
    assert(achado->prioridade(NULL, 0) == 42);   /* chamada pelo ponteiro */

    /* A tabela guarda uma COPIA da struct: estragar o original depois de
     * registrar nao pode afetar o que esta registrado. */
    novo.nome = "ESTRAGADO";
    assert(escalonador_buscar("ESTRAGADO") == NULL);
    assert(escalonador_buscar("teste") != NULL);

    printf("autoteste: interface do escalonador ... ok\n");
}

/* Parser do arquivo de configuracao (config.h). Como config_carregar_texto
 * recebe TEXTO, o teste nao precisa de arquivo nenhum. */
static void teste_config(void)
{
    /* Uma string com as regras que mais quebram: algoritmo em minusculas
     * (3.3.2), linha com e sem ';' final (3.3.3), linha em branco, espacos
     * nos campos e comentario (3.3.6), CRLF de arquivo do Windows, cor e
     * prazo vazios (3.2) e uma tarefa aperiodica (4.4). */
    static const char texto[] =
        "\xEF\xBB\xBF"                       /* BOM: tem que ser pulado */
        "# comentario\r\n"
        " edf ; 3 ; 2 ;\r\n"
        "\r\n"
        "1;FF0000;0;3;10;8;\r\n"
        " 2 ; ; 1 ; 2 ; 5 \r\n"
        "3;00FF00;0;1;0;7\r\n";
    static Diagnostico d;                    /* static: sao ~25 KB */
    static char grande[8192];
    Estado e;
    int i, pos;

    memset(&d, 0, sizeof d);
    assert(config_carregar_texto(texto, &e, &d) == 1);
    assert(d.n_erros == 0 && d.n_avisos == 1);          /* a aperiodica */
    assert(strcmp(e.algoritmo, "EDF") == 0);
    assert(e.quantum == 3 && e.ncpus == 2 && e.tick == 0);
    assert(e.ntarefas == 2);
    assert(e.tarefas[0].id == 1 && e.tarefas[0].r == 0xFF && e.tarefas[0].prazo == 8);
    assert(e.tarefas[1].id == 2 && e.tarefas[1].r == PADRAO_COR_R);
    assert(e.tarefas[1].ingresso == 1 && e.tarefas[1].duracao == 2);
    assert(e.tarefas[1].prazo == 5);                    /* vazio = periodo */
    assert(e.tarefas[0].cpu == -1 && e.tarefas[0].estado == EST_INATIVA);
    assert(e.cpus[1].id == 1 && e.cpus[1].tarefa == -1);
    estado_liberar(&e);

    /* Cabecalho vazio: valem os padroes (3.2). */
    memset(&d, 0, sizeof d);
    assert(config_carregar_texto(";;\n7;;0;1;4", &e, &d) == 1);
    assert(strcmp(e.algoritmo, PADRAO_ALGORITMO) == 0);
    assert(e.quantum == PADRAO_QUANTUM && e.ncpus == PADRAO_CPUS);
    estado_liberar(&e);

    /* Arquivo ruim: TODOS os erros sao reportados de uma vez (cor, duracao,
     * id repetido) e nada fica alocado. */
    memset(&d, 0, sizeof d);
    assert(config_carregar_texto("RM;2;1\n1;XYZ;0;1;4\n2;;0;abc;4\n3;;0;1;4\n3;;0;1;4",
                                 &e, &d) == 0);
    assert(d.n_erros == 3);
    assert(e.tarefas == NULL && e.cpus == NULL && e.ntarefas == 0);

    memset(&d, 0, sizeof d);
    assert(config_carregar_texto("", &e, &d) == 0 && d.n_erros == 1);
    memset(&d, 0, sizeof d);
    assert(config_carregar_texto("RM;2;0\n1;;0;1;4", &e, &d) == 0);   /* 0 CPUs */
    memset(&d, 0, sizeof d);
    assert(config_carregar_texto("RM;2;1\n", &e, &d) == 0);           /* sem tarefas */

    /* Periodo negativo e ERRO (3.3), e a mensagem traz o numero da linha
     * contando as linhas em branco, para bater com o editor do usuario. */
    memset(&d, 0, sizeof d);
    assert(config_carregar_texto("RM;2;1\n\n1;;0;1;-4", &e, &d) == 0);
    assert(d.n_erros == 1 && strstr(d.erros[0], "linha 3") != NULL);

    /* Tres decisoes de projeto, na mesma leitura:
     *  - quantum 0 e aceito (significa "sem limite de quantum");
     *  - prazo 0 escrito NAO recusa o arquivo: vira o periodo, com aviso;
     *  - a lista de eventos fica inteira, com os ';' do meio, e sem o ';'
     *    final (3.3.3 e 3.3.5). */
    memset(&d, 0, sizeof d);
    assert(config_carregar_texto("RM;0;1\n"
                                 "1;;0;1;6;0\n"
                                 "2;;0;1;8;8;IO:2-1;IO:3-2;ML1:4;\n"
                                 "3;;0;1;9;9;\n", &e, &d) == 1);
    assert(e.quantum == 0);
    assert(e.tarefas[0].prazo == 6 && d.n_avisos == 1 && d.n_erros == 0);
    assert(strcmp(e.tarefas[1].eventos, "IO:2-1;IO:3-2;ML1:4") == 0);
    assert(e.tarefas[2].eventos[0] == '\0');
    estado_liberar(&e);

    /* O arquivo do professor tem ~4 KB: ~190 tarefas tem que caber, o vetor
     * cresce sozinho (3.3.1). */
    pos = snprintf(grande, sizeof grande, "RM;2;4\n");
    for (i = 1; i <= 190; i++)
        pos += snprintf(grande + pos, sizeof grande - (size_t)pos, "%d;A0B0C0;0;1;%d;%d;\n", i, i + 5, i + 5);
    memset(&d, 0, sizeof d);
    assert(config_carregar_texto(grande, &e, &d) == 1);
    assert(e.ntarefas == 190 && e.tarefas[189].id == 190 && e.tarefas[189].periodo == 195);
    estado_liberar(&e);

    printf("autoteste: arquivo de configuracao ... ok\n");
}

static void autoteste(void)
{
    teste_escalonador();
    teste_config();

    /* TODO: iniciar a simulacao com o Estado pequeno e conferir:
     *   - sim_executar_tudo termina com MAX_ATIVACOES em cada tarefa;
     *   - DETERMINISMO: retroceder 2 ticks e avancar 2 volta exatamente ao
     *     mesmo estado (e a garantia da qual o requisito 1.5.2 depende);
     *   - sim_editar_tarefa RECUSA uma edicao invalida (ex. prazo 0) e
     *     preenche o motivo (req. 3.4);
     *   - gantt_svg devolve 1 para a simulacao inteira (0..n_hist-1) e
     *     para uma janela, e 0 para um intervalo invalido (tick_ini >
     *     tick_fim). Grave num arquivo de nome fixo na pasta atual com
     *     fopen e apague com remove() -- tmpfile() falha no Windows (ver
     *     gantt.h).
     *
     * TODO: depois que estado_json.c existir, conferir tambem que
     * estado_json_atual devolve -1 com um buffer pequeno demais em vez de
     * escrever pela metade -- e o contrato documentado em estado_json.h, e
     * um JSON cortado quebraria a pagina sem mensagem nenhuma.
     *
     * No fim: sim_liberar e um printf dizendo que tudo passou. */
}

/* ========================================================================== */

int main(int argc, char **argv)
{
    Simulacao sim;
    int porta = PORTA_PADRAO;

    /* Tem que ser a PRIMEIRA coisa. config_carregar_texto/sim_iniciar procuram o
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
