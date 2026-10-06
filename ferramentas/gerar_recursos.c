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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Deriva um identificador C valido a partir do nome do arquivo, em
 * MAIUSCULAS (ex. "estilo.css" -> "ESTILO_CSS"). */
static void identificador_de(const char *nome_arquivo, char *out, size_t out_tam)
{
    size_t i = 0;
    if (out_tam == 0) return;

    /* i + 1 < out_tam: para sempre sobrar uma posicao para o '\0'. */
    for (; nome_arquivo[i] != '\0' && i + 1 < out_tam; i++) {
        /* O cast para unsigned char e obrigatorio: isalnum/toupper tem
         * comportamento indefinido com valores negativos, e um char com
         * acento e negativo quando 'char' tem sinal (caso do gcc no x86). */
        unsigned char c = (unsigned char)nome_arquivo[i];
        out[i] = isalnum(c) ? (char)toupper(c) : '_';
    }
    out[i] = '\0';
}

/* Descobre o tipo MIME a partir da extensao do arquivo. O navegador usa esse
 * valor (cabecalho Content-Type) para decidir o que fazer com a resposta:
 * com o tipo errado, ele se recusa a aplicar o CSS e a executar o JS. */
static const char *mime_de(const char *nome_arquivo)
{
    const char *ext = strrchr(nome_arquivo, '.');   /* ultimo ponto do nome */

    if (ext != NULL) {
        if (strcmp(ext, ".html") == 0) return "text/html; charset=utf-8";
        if (strcmp(ext, ".css")  == 0) return "text/css; charset=utf-8";
        if (strcmp(ext, ".js")   == 0) return "application/javascript; charset=utf-8";
        if (strcmp(ext, ".svg")  == 0) return "image/svg+xml";
    }
    return "application/octet-stream";   /* "bytes quaisquer" */
}

/* Extrai só o nome do arquivo de um caminho (ignora diretorios). */
static const char *nome_base(const char *caminho)
{
    const char *barra = strrchr(caminho, '/');
    const char *contra = strrchr(caminho, '\\');   /* caminho do Windows */

    if (contra != NULL && (barra == NULL || contra > barra)) barra = contra;
    return barra != NULL ? barra + 1 : caminho;
}

/* Le o arquivo inteiro em memoria. Devolve o tamanho em *tam, ou NULL em erro. */
static unsigned char *ler_arquivo(const char *caminho, long *tam)
{
    /* "rb" (binario) e nao "r": em modo texto o Windows converte "\r\n" em
     * "\n" durante a leitura. Os bytes embutidos deixariam de ser os do
     * arquivo, e o total lido nao bateria com o tamanho medido pelo ftell. */
    FILE *f = fopen(caminho, "rb");
    unsigned char *buf;
    long tamanho;

    if (f == NULL) return NULL;

    if (fseek(f, 0, SEEK_END) != 0 || (tamanho = ftell(f)) < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);

    /* malloc(0) pode devolver NULL sem ser erro; pedir ao menos 1 byte
     * evita confundir "arquivo vazio" com "faltou memoria". */
    buf = malloc(tamanho > 0 ? (size_t)tamanho : 1);
    if (buf == NULL || fread(buf, 1, (size_t)tamanho, f) != (size_t)tamanho) {
        free(buf);
        fclose(f);
        return NULL;
    }

    fclose(f);
    *tam = tamanho;
    return buf;
}

/* Escreve os bytes de 'dados' como uma lista "0x1a,0x2b,..." em C,
 * quebrando linha a cada 16 valores (só por legibilidade do .h gerado). */
static void escrever_bytes(FILE *saida, const unsigned char *dados, long tam)
{
    long i;
    for (i = 0; i < tam; i++) {
        if (i % 16 == 0) fprintf(saida, "\n    ");
        fprintf(saida, "0x%02x,", dados[i]);
    }
    fprintf(saida, "\n");
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr, "uso: %s <saida.h> <arquivo1> [arquivo2 ...]\n", argv[0]);
        return 1;
    }

    const char *caminho_saida = argv[1];
    int n_arquivos = argc - 2;
    long total = 0;
    char id[128];
    int i;

    /* Os tamanhos sao descobertos na 1a passada (ao ler cada arquivo) e
     * usados de novo na 2a (ao escrever a tabela). */
    long *tamanhos = malloc((size_t)n_arquivos * sizeof *tamanhos);
    if (tamanhos == NULL) {
        fprintf(stderr, "erro: memoria insuficiente\n");
        return 1;
    }

    FILE *saida = fopen(caminho_saida, "w");
    if (saida == NULL) {
        fprintf(stderr, "erro: não consegui abrir '%s' para escrita\n", caminho_saida);
        free(tamanhos);
        return 1;
    }

    /* ---------------------------- cabeçalho do .h -------------------------- */
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

        identificador_de(nome, id, sizeof id);
        fprintf(saida, "/* %s -- %ld bytes */\n"
                       "static const unsigned char RECURSO_%s[] = {",
                nome, tamanhos[i], id);
        if (tamanhos[i] == 0)
            fprintf(saida, " 0 ");   /* C nao permite vetor vazio: "{}" e erro */
        else
            escrever_bytes(saida, dados, tamanhos[i]);
        fprintf(saida, "};\n\n");

        total += tamanhos[i];
        free(dados);   /* ja esta no arquivo de saida; nao precisa mais */
    }

    /* ---------------------- 2a passada: a tabela final --------------------- */
    fprintf(saida, "static const RecursoWeb RECURSOS_WEB[] = {\n");
    for (i = 0; i < n_arquivos; i++) {
        const char *nome = nome_base(argv[2 + i]);
        /* index.html responde pela raiz do site ("/"); os demais, por "/nome". */
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
