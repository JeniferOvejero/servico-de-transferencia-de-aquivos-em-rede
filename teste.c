#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

#define NUM_THREADS 3
#define BAR_WIDTH 30

pthread_mutex_t print_mutex; // Mutex para sincronizar o acesso ao terminal

typedef struct
{
    int thread_id;
    int socket;
    int line; // Linha fixa para atualizar
} ThreadArgs;

// Função para mover o cursor
void move_cursor(int row, int col)
{
    printf("\033[%d;%dH", row, col);
}

// Função que atualiza o progresso no terminal
void update_progress(int line, const char *filename, int progress, int total_size)
{
    char bar[BAR_WIDTH + 1];
    int filled = (progress * BAR_WIDTH) / total_size;

    // Preenche a barra
    memset(bar, '#', filled);
    memset(bar + filled, ' ', BAR_WIDTH - filled);
    bar[BAR_WIDTH] = '\0';

    // Bloqueia o mutex para garantir exclusividade no terminal
    pthread_mutex_lock(&print_mutex);

    // Move o cursor para a linha correspondente e imprime o progresso
    move_cursor(line, 1);
    printf("(%s) Carregando... %c %d [%s] %dbytes/%dbytes    ",
           filename,
           "|/-\\"[progress % 4], // Spinner animado
           progress,
           bar,
           progress,
           total_size);
    fflush(stdout);

    // Libera o mutex após a escrita
    pthread_mutex_unlock(&print_mutex);
}

// Função da thread
void *thread_function(void *arg)
{
    ThreadArgs *args = (ThreadArgs *)arg;
    int line = args->line; // Linha fixa para a barra de progresso
    const char *filename = (args->thread_id == 0) ? "lorem.txt" : (args->thread_id == 1) ? "ipsum.txt"
                                                                                         : "dolor.txt";
    int total_size = 2769 + args->thread_id * 500; // Tamanho fictício do arquivo

    for (int progress = 0; progress <= total_size; progress += 123)
    {
        update_progress(line, filename, progress, total_size);
        usleep(100000); // Simula trabalho
    }
    return NULL;
}

int main()
{
    pthread_t threads[NUM_THREADS];
    ThreadArgs thread_args[NUM_THREADS];
    int terminal_lines = 24; // Número de linhas padrão do terminal (ajuste conforme necessário)

    // Inicializa o mutex
    pthread_mutex_init(&print_mutex, NULL);

    // Limpa a tela
    printf("\033[2J");

    // Exibe algumas mensagens antes de iniciar as threads
    printf("Iniciando o carregamento de arquivos...\n");
    printf("Monitorando threads de progresso:\n");

    // Configura e cria as threads
    for (int i = 0; i < NUM_THREADS; i++)
    {
        thread_args[i].thread_id = i;
        thread_args[i].socket = i + 1000;                       // Exemplo: socket fictício
        thread_args[i].line = terminal_lines - NUM_THREADS + i; // Aloca as últimas linhas para as threads
        pthread_create(&threads[i], NULL, thread_function, &thread_args[i]);
    }

    // Aguarda as threads terminarem
    for (int i = 0; i < NUM_THREADS; i++)
    {
        pthread_join(threads[i], NULL);
    }

    // Destroi o mutex
    pthread_mutex_destroy(&print_mutex);

    // Move o cursor para uma nova linha após o término
    move_cursor(terminal_lines + 1, 1);
    printf("Todos os arquivos foram carregados.\n");
    return 0;
}
