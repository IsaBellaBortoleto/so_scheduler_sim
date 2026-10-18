/* ============================================================================
 * gerar_recursos.c — ferramenta de BUILD (não vai pro executável final)
 * ----------------------------------------------------------------------------
 * Le cada arquivo passado na linha de comando e escreve um .h com o
 * conteudo embutido como array de bytes, pra servidor.c poder servir a
 * pagina web sem depender de uma pasta web/ do lado do executavel --
 * requisito do professor: "um unico executavel pra clicar e rodar".
 *
 * Uso: gerar_recursos <saida.h> <arquivo1> [arquivo2 ...]
 *
 * O nome de cada recurso na tabela RECURSOS_WEB e derivado do nome do
 * arquivo: "index.html" vira o caminho "/", qualquer outro nome vira
 * "/nome.ext" (ex. "app.js" -> "/app.js").
 *
 * Roda uma vez por build (chamado pelo Makefile ANTES de compilar
 * servidor.c), gera build/recursos_web.h, e esse .h é #include'd
 * normalmente pelo resto do projeto. Não faz parte do binário final.
 *
 * FORMATO DO .h GERADO (o que o servidor.c vai encontrar):
 *
 *   typedef struct { caminho, tipo_mime, dados, tamanho } RecursoWeb;
 *   static const unsigned char RECURSO_APP_JS[] = { 0x2f,0x2a, ... };
 *   static const RecursoWeb RECURSOS_WEB[] = {
 *       { "/app.js", "application/javascript; charset=utf-8",
 *         RECURSO_APP_JS, 9431UL },
 *       ...
 *   };
 *   static const int RECURSOS_WEB_QTDE = 3;
 *
 * Para responder a um GET, o servidor percorre RECURSOS_WEB comparando o
 * campo 'caminho' com o caminho pedido e envia 'tamanho' bytes de 'dados'.
 * O tamanho vai explicito na tabela porque os dados sao bytes crus, nao uma
 * string: nao ha '\0' no fim, e strlen() daria o valor errado.
 * ==========================================================================*/

/*
    Lembrete: gerar_recursos e uma file de pré-compilação inteligente, cumprindo o requisito nº5    
    O executável em C não é apenas um simulador; 
    ele converte-se num micro servidor web autocontido, que 
    traz todo o frontend já fundido lá dentro! 
    Um único ficheiro .exe resolve tudo. 
*/
 #include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>




/* Deriva um identificador C valido a partir do nome do arquivo, em
 * MAIUSCULAS (ex. "estilo.css" -> "ESTILO_CSS"). 
 * 
 * POR QUE ISSO É NECESSÁRIO?
 * Para que não ocorra erro de sintaxe imediato, pois para embutir os arquivos num .h, 
 * precisaremos declarar variáveis em C e caso tenha caracteres especiais (ponto, hífen) causará erro.  
 */
static void identificador_de(const char *nome_arquivo, char *out, size_t out_tam)
{
    size_t i = 0;
    if (out_tam == 0) return;

    /* i + 1 < out_tam: garante que nunca escreveremos fora do limite da
     * string 'out', deixando sempre 1 byte reservado no fim para 
     * o terminador nulo ('\0'). Prática essencial de segurança em C. */
    for (; nome_arquivo[i] != '\0' && i + 1 < out_tam; i++) {
        /* O cast para unsigned char e obrigatorio: isalnum/toupper tem
         * comportamento indefinido com valores negativos, e um char com
         * acento e negativo quando 'char' tem sinal (caso do gcc no x86).
         * O cast evita comportamentos indefinidos no C.  */
        unsigned char c = (unsigned char)nome_arquivo[i];
        /* O operador ternário ( ? : ) verifica: se for alfanumérico (isalnum),
         * converte para maiúscula (toupper). Se for um ponto, traço ou espaço,
         * ele converte para o caractere de sublinhado '_'.*/
        out[i] = isalnum(c) ? (char)toupper(c) : '_';
    }
    out[i] = '\0'; // Fecha a string corretamente com o terminador nulo.
}


/* ============================================================================
 * Descobre o tipo MIME a partir da extensão do arquivo. 
 *
 * TEORIA DE REDES / WEB E REQUISITOS:
 * O navegador usa esse valor (que será colocado no cabeçalho HTTP Content-Type) 
 * para decidir o que fazer com a resposta. 
 * Para cumprir o requisito de apresentar um Gráfico de Gantt e uma interface
 * visual[cite: 50, 53], o frontend depende fortemente de CSS e JS. 
 * Navegadores modernos possuem proteções rigorosas de segurança: com o tipo 
 * MIME errado (ou genérico), o navegador recusa-se a aplicar o CSS e a executar 
 * o JS, quebrando completamente a interface da simulação.
 * ==========================================================================*/
static const char *mime_de(const char *nome_arquivo)
{
    /* strrchr encontra a ÚLTIMA ocorrência de um caractere na string.
     * Por que a última? Porque arquivos podem ter nomes compostos,
     * como "app.min.js" ou "estilo.teste.css". Queremos sempre a 
     * verdadeira e última extensão. */
    //Assim pode ser verificado qual é o tipo da página .html, .css, .js.
    
    const char *ext = strrchr(nome_arquivo, '.');   /* ultimo ponto do nome */

    if (ext != NULL) {
        /* Compara a extensão extraída com as que o nosso projeto web utiliza. 
         * Note a adição explícita de "charset=utf-8". Isso garante que 
         * caracteres acentuados na sua interface em português sejam 
         * exibidos corretamente no navegador*/
        if (strcmp(ext, ".html") == 0) return "text/html; charset=utf-8";
        if (strcmp(ext, ".css")  == 0) return "text/css; charset=utf-8";
        if (strcmp(ext, ".js")   == 0) return "application/javascript; charset=utf-8";
        /* O SVG é tratado como uma imagem XML pura. Útil se o frontend
         * precisar renderizar ícones vetoriais. */
        if (strcmp(ext, ".svg")  == 0) return "image/svg+xml";
    }
    /* Fallback (Plano B): "application/octet-stream"
     * Se a extensão for desconhecida ou não houver extensão, o servidor 
     * avisa que o arquivo é apenas um "fluxo de bytes crus". O navegador 
     * geralmente reage a isso forçando o download do arquivo, em vez de 
     * tentar exibi-lo na tela. */
    return "application/octet-stream";   
}
/* ============================================================================
 * Extrai só o nome do arquivo de um caminho (ignora diretórios).
 *
 * TEORIA DE SISTEMAS / SEGURANÇA:
 * Em C, não existe abstração nativa de diretórios (como a classe Path em Java
 * ou pathlib no Python). Um argumento passado pela linha de comando no momento
 * do build pode vir como "web/app.js" (Linux/Mac) ou "web\app.js" (Windows).
 * Para isso que serve essa função para cuidar que a rota ignore os diretórios e 
 * que não ocorra erro nos links HTML quando o comando make fosse executado
 * 
 * ==========================================================================*/
static const char *nome_base(const char *caminho)
{
    /* Procura a última ocorrência da barra normal (padrão POSIX/Unix) */
    const char *barra = strrchr(caminho, '/');
    /* Procura a última ocorrência da contrabarra (padrão Windows).
     * Nota: O C exige o escape '\\' para representar uma única contrabarra. */
    const char *contra = strrchr(caminho, '\\');   
    /* Avalia qual dos dois padrões de barra aparece por último no caminho.
     * Isto permite que o conversor funcione independentemente de o utilizador 
     * compilar o projeto no Windows, no Linux ou num sistema misto. */
    if (contra != NULL && (barra == NULL || contra > barra)) barra = contra;
    /* Se encontrou uma barra (qualquer que seja o tipo), o nome do ficheiro 
     * começa logo no caractere seguinte (barra + 1). 
     * Se não encontrou barra nenhuma (ex: chamaram "app.js" diretamente na 
     * mesma pasta), o próprio 'caminho' já é o nome base. */
    return barra != NULL ? barra + 1 : caminho;
}

/* Le o arquivo inteiro em memoria. Devolve o tamanho em *tam, ou NULL em erro. */
static unsigned char *ler_arquivo(const char *caminho, long *tam)
{
    /* "rb" (binario) e nao "r": em modo texto o Windows converte "\r\n" em
     * "\n" durante a leitura. Os bytes embutidos deixariam de ser os do
     * arquivo, e o total lido nao bateria com o tamanho medido pelo ftell. */

    /* Abre o ficheiro requisitando ao SO o modo binário estrito. */
    FILE *f = fopen(caminho, "rb");
    unsigned char *buf;
    long tamanho;

    if (f == NULL) return NULL;

    /* DESCOBRINDO O TAMANHO DO ARQUIVO VIA METADADOS DO SO:
     * Em vez de ler o ficheiro byte a byte num loop lento, usamos ponteiros 
     * de ficheiro nativos da biblioteca padrão do C.
     * 1. fseek(..., SEEK_END): Pede ao SO para colocar o "cursor" de leitura
     *    exatamente no último byte do ficheiro.
     * 2. ftell: Pergunta ao SO: "Em que posição o cursor está agora?". 
     *    O resultado é exatamente o tamanho do ficheiro em bytes! */
    if (fseek(f, 0, SEEK_END) != 0 || (tamanho = ftell(f)) < 0) {
        fclose(f);
        return NULL;
    }
    /* rewind: Volta o "cursor" de leitura para o byte 0 (início do ficheiro),
     * preparando-o para a função fread ler tudo de uma vez. */
    rewind(f);

    /* malloc(0) pode devolver NULL sem ser erro;
     * Se tivéssemos um ficheiro vazio (ex: "estilo.css" com 0 bytes) e o 
     * malloc devolvesse NULL, a função abaixo assumiria que faltou memória RAM 
     * (Out of Memory) e daria erro. Pedir pelo menos 1 byte (tamanho > 0 ? tamanho : 1)
     * previne este falso positivo. */
    buf = malloc(tamanho > 0 ? (size_t)tamanho : 1);
    /* fread: Tenta carregar todo o tamanho do ficheiro para a memória RAM (buf) 
     * de uma só vez (chamada de sistema eficiente). 
     * A verificação '!=' garante que o SO leu exatamente a quantidade de bytes
     * que nós medimos com o ftell. Se não ler, houve falha de disco/leitura. */
    if (buf == NULL || fread(buf, 1, (size_t)tamanho, f) != (size_t)tamanho) {
        free(buf);
        fclose(f);
        return NULL;
    }

    fclose(f);
    /* Atribui o tamanho lido ao ponteiro passado por referência (*tam) 
     * para que a função main saiba exatamente quantos bytes deve iterar. */
    *tam = tamanho;
    return buf;
}

/* Escreve os bytes de 'dados' como uma lista "0x1a,0x2b,..." em C,
 * quebrando linha a cada 16 valores (só por legibilidade do .h gerado). 
 * Isso evita que ocorra buffer oveflow se for inserido em única linha*/
static void escrever_bytes(FILE *saida, const unsigned char *dados, long tam)
{
    long i;
    for (i = 0; i < tam; i++) {
        /* Quebra a linha a cada 16 valores. 
         * Porquê 16? Porque num visualizador hexadecimal (Hex Editor),
         * a representação clássica da memória é agrupar os bytes em linhas
         * de 16 colunas (16 bytes = 128 bits). */
        if (i % 16 == 0) fprintf(saida, "\n    ");
        /* O especificador de formato de C:
         * 0x   : Imprime o prefixo literal "0x" (indicador de hexadecimal em C).
         * %02x : Formata o número (dados[i]) como hexadecimal minúsculo ('x'),
         *        completando com um zero à esquerda ('0') se necessário,
         *        para garantir que ocupa exatamente dois caracteres ('2').
         *        Ex: O valor decimal 10 vira "0x0a", e não "0xa". */
        fprintf(saida, "0x%02x,", dados[i]);
    }
    /* Uma última quebra de linha no final do array para garantir que
     * a chave de fecho da matriz "};" na função principal (main) 
     * fique isolada na linha de baixo, mantendo o código limpo. */
    fprintf(saida, "\n");
}

/* ============================================================================
 * Função Principal (Entry Point) da ferramenta de BUILD.
 *
 * Esta função recebe os argumentos da linha de comandos do Make. 
 * O seu objetivo é empacotar a interface do simulador  e visualizar 
 * o gráfico de gantt.
 * ==========================================================================*/
int main(int argc, char *argv[])
{
    /* Validação da leitura do ficheiro: */
    if (argc < 3) {
        fprintf(stderr, "uso: %s <saida.h> <arquivo1> [arquivo2 ...]\n", argv[0]);
        return 1;
    }

    const char *caminho_saida = argv[1];
    int n_arquivos = argc - 2;
    long total = 0;
    char id[128];
    int i;

    /* Os tamanhos são descobertos na 1a passada (ao ler cada arquivo) e
     * usados de novo na 2a (ao escrever a tabela).
     * Como a quantidade de arquivos (n_arquivos) é dinâmica (vem dos
     * argumentos), usamos alocação dinâmica (malloc) para o array de tamanhos. */
    long *tamanhos = malloc((size_t)n_arquivos * sizeof *tamanhos);
    if (tamanhos == NULL) {
        fprintf(stderr, "erro: memoria insuficiente\n");
        return 1;
    }
    /* Abre o ficheiro de saída (.h) em modo de escrita ('w'). */
    FILE *saida = fopen(caminho_saida, "w");
    if (saida == NULL) {
        fprintf(stderr, "erro: não consegui abrir '%s' para escrita\n", caminho_saida);
        free(tamanhos);
        return 1;
    }

    /* ---------------------------- cabeçalho do .h -------------------------- */
    /* Este bloco de 'fprintf' cria as proteções estruturais do código C gerado. 
     * O #ifndef previne erros de compilação caso este ficheiro .h seja 
     * importado (#include) mais do que uma vez em múltiplos ficheiros do 
     * projeto do servidor.
     * Além disso, a definição da struct RecursoWeb agrupa as informações de
     * rota HTTP, tipo MIME e os dados crus do ficheiro que vão alimentar a
     * funcionalidade visual da simulação. */
    fprintf(saida, "/* GERADO AUTOMATICAMENTE por ferramentas/gerar_recursos.c.\n"
                   " * NAO EDITE: qualquer alteracao some no proximo build.\n"
                   " * Para mudar a pagina, edite os arquivos de origem:\n");
    for (i = 0; i < n_arquivos; i++)
        fprintf(saida, " *   %s\n", argv[2 + i]);
    fprintf(saida, " */\n"
                   "#ifndef RECURSOS_WEB_H\n"
                   "#define RECURSOS_WEB_H\n\n"
                   "typedef struct {\n"
                   "    const char *caminho;          /* ex. \"/\" ou \"/app.js\" */\n"
                   "    const char *tipo_mime;        /* valor do Content-Type */\n"
                   "    const unsigned char *dados;   /* bytes crus, SEM '\\0' no fim */\n"
                   "    unsigned long tamanho;        /* quantos bytes ha em dados */\n"
                   "} RecursoWeb;\n\n");

    /* ------------------ 1a passada: um vetor de bytes por arquivo ---------- */

    for (i = 0; i < n_arquivos; i++) {
        const char *nome = nome_base(argv[2 + i]);
        unsigned char *dados = ler_arquivo(argv[2 + i], &tamanhos[i]);
        /* 
         * O comando 'make' decide recompilar os ficheiros avaliando as 
         * datas de modificação (timestamps). Se o nosso script falhar e 
         * deixar um "recursos_web.h" corrompido ou pela metade no disco, 
         * na próxima vez que rodarmos 'make', ele verá que o .h "já existe" 
         * e é recente, assumindo falsamente sucesso e prosseguindo.
         * Remover ativamente o ficheiro corrompido garante que o Make saiba 
         * que a tarefa de pré-compilação falhou criticamente. */
        if (dados == NULL) {
            fprintf(stderr, "erro: não consegui ler '%s'\n", argv[2 + i]);
            fclose(saida);
            /* Apagar a saida pela metade: se ela ficasse no disco, o make a
             * veria como "atualizada" e o proximo build falharia longe
             * daqui, ao compilar o servidor, com um erro bem mais confuso. */
            remove(caminho_saida);
            free(tamanhos);
            return 1;
        }
        /* Utiliza as funções anteriores para criar variáveis válidas em C
         * e escreve os arrays formatados na memória estática gerada. */
        identificador_de(nome, id, sizeof id);
        fprintf(saida, "/* %s -- %ld bytes */\n"
                       "static const unsigned char RECURSO_%s[] = {",
                nome, tamanhos[i], id);
        /* Se houver um ficheiro vazio, 
         * simulamos que tem 1 elemento irrelevante (zero) para não quebrar 
         * o compilador C a seguir. */
        if (tamanhos[i] == 0)
            fprintf(saida, " 0 ");   /* C nao permite vetor vazio: "{}" e erro */
        else
            escrever_bytes(saida, dados, tamanhos[i]);
        fprintf(saida, "};\n\n");

        total += tamanhos[i];
        free(dados);   /* ja esta no arquivo de saida; nao precisa mais */
    }

    /* ---------------------- 2a passada: a tabela final --------------------- */
    /* Este bloco cria a estrutura principal  
     * que o simulador vai iterar sempre que receber um pedido HTTP do navegador 
     * que renderiza os gráficos e exibe as informações sobre o
     * estado das tarefas e uso da CPU[cite: 52]. */
    fprintf(saida, "static const RecursoWeb RECURSOS_WEB[] = {\n");
    for (i = 0; i < n_arquivos; i++) {
        const char *nome = nome_base(argv[2 + i]);
        /* Se o ficheiro for "index.html", servimos na 
         * raiz "/" para o utilizador não precisar escrever localhost:8080/index.html */
        int eh_indice = (strcmp(nome, "index.html") == 0);

        identificador_de(nome, id, sizeof id);
        fprintf(saida, "    { \"/%s\", \"%s\", RECURSO_%s, %ldUL },\n",
                eh_indice ? "" : nome, mime_de(nome), id, tamanhos[i]);
    }
    fprintf(saida, "};\n\n"
                   "static const int RECURSOS_WEB_QTDE = %d;\n\n"
                   "#endif /* RECURSOS_WEB_H */\n", n_arquivos);

    free(tamanhos);

    /* O fclose descarrega o que ainda estava em buffer; e nele que um erro
     * de escrita (disco cheio, por exemplo) finalmente aparece. */
    if (fclose(saida) != 0) {
        fprintf(stderr, "erro: falha ao gravar '%s'\n", caminho_saida);
        remove(caminho_saida);
        return 1;
    }

    fprintf(stderr, "gerado: %s (%d recurso(s), %ld bytes)\n",
            caminho_saida, n_arquivos, total);
    return 0;
}
