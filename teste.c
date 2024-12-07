#include <stdio.h>
#include <unistd.h> // Para usleep
#include <string.h> // Para strlen

void print_progress_bar(int progress, int total, const char *filename)
{
    char spinner[] = {'|', '/', '-', '\\'};
    int spinner_index = progress % 4;               // Altera o spinner conforme o progresso
    int bar_width = 20;                             // Largura da barra de progresso
    int completed = (progress * bar_width) / total; // Quantidade de blocos completos

    // Barra de progresso
    char bar[bar_width + 1];
    for (int i = 0; i < bar_width; i++)
    {
        if (i < completed)
            bar[i] = '#';
        else
            bar[i] = ' ';
    }
    bar[bar_width] = '\0'; // Certifique-se de que é uma string terminada em '\0'

    // Imprime a linha
    printf("\r(%s) Carregando... %c %d%% [%s] %dbytes", filename, spinner[spinner_index], progress, bar, total);
    fflush(stdout); // Atualiza imediatamente a saída
}

int main()
{
    const char *filename = "arquivo.txt"; // Nome do arquivo
    int total = 100;                      // Total para o carregamento (em %)
    int total_bytes = 200;                // Total de bytes para carregar
    int loaded_bytes = 0;                 // Inicialmente 0 bytes carregados

    for (int progress = 0; progress <= total; progress++)
    {
        loaded_bytes = (progress * total_bytes) / total; // Calcula os bytes carregados
        print_progress_bar(progress, total, filename);
        usleep(100000); // Espera 100ms
    }

    printf("\nCarregamento completo!\n");
    return 0;
}
