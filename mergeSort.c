#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

#define ARRAY_SIZE 40 // trabalho final com o valores 10.000, 100.000, 1.000.000
#define DELTA 5       // tamanho minimo para dividir o vetor
int debug = 0;        // flag para imprimir mensagens de debug

void bs(int n, int *vetor)
{
   int c = 0, d, troca, trocou = 1;

   while (c < (n - 1) && trocou)
   {
      trocou = 0;
      for (d = 0; d < n - c - 1; d++)
         if (vetor[d] > vetor[d + 1])
         {
            troca = vetor[d];
            vetor[d] = vetor[d + 1];
            vetor[d + 1] = troca;
            trocou = 1;
         }
      c++;
   }
}

void inicializa(int vetor[], int tam_vetor)
{
   for (int i = 0; i < tam_vetor; i++)
      vetor[i] = tam_vetor - i;
}

/* recebe um ponteiro para um vetor que contem as mensagens recebidas dos filhos e            */
/* intercala estes valores em um terceiro vetor auxiliar. Devolve um ponteiro para este vetor */

int *interleaving(int vetor[], int tam)
{
   int *vetor_auxiliar;
   int i1, i2, i_aux;

   vetor_auxiliar = (int *)malloc(sizeof(int) * tam);

   i1 = 0;
   i2 = tam / 2;

   for (i_aux = 0; i_aux < tam; i_aux++)
   {
      if (((vetor[i1] <= vetor[i2]) && (i1 < (tam / 2))) || (i2 == tam))
         vetor_auxiliar[i_aux] = vetor[i1++];
      else
         vetor_auxiliar[i_aux] = vetor[i2++];
   }

   return vetor_auxiliar;
}

// int *interleaving(int vetor[], int left_size, int right_size)
// {
//    int total = left_size + right_size;
//    int *vetor_auxiliar = (int *)malloc(sizeof(int) * total);
//    int i1 = 0, i2 = left_size, i_aux = 0;
//    int i1_limit = left_size, i2_limit = left_size + right_size;

//    while (i_aux < total)
//    {
//       if ((i1 < i1_limit) && ( (i2 >= i2_limit) || (vetor[i1] <= vetor[i2]) ))
//          vetor_auxiliar[i_aux++] = vetor[i1++];
//       else
//          vetor_auxiliar[i_aux++] = vetor[i2++];
//    }

//    return vetor_auxiliar;
// }

int main(int argc, char *argv[])
{
   if (argc < 2)
   {
      printf("Usage: %s <array_size> <optional debug>\n", argv[0]);
      return 1;
   }

   debug = (argc == 2 || atoi(argv[2]) == 0) ? 0 : 1;

   double start_time, end_time;
   int *vetor = NULL;
   int tam_vetor;
   int rank_pai;
   MPI_Status status;

   int my_rank, num_procs;
   MPI_Init(&argc, &argv);
   MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
   MPI_Comm_size(MPI_COMM_WORLD, &num_procs);

   //---------------------
   // Versao Sequencial
   //---------------------

   if (num_procs == 1)
   {
      // defino vetor
      tam_vetor = atoi(argv[1]);
      vetor = (int *)malloc(sizeof(int) * tam_vetor);

      // sou a raiz e gero vetor
      inicializa(vetor, tam_vetor);
      if (debug)
      {
         printf("\nVetor: ");
         for (int i = 0; i < tam_vetor; i++) /* print unsorted array */
            printf("[%03d] ", vetor[i]);
      }

      start_time = MPI_Wtime();

      // ordeno o vetor
      bs(tam_vetor, vetor);

      end_time = MPI_Wtime();

      if (debug)
      {
         printf("\nVetor: ");
         for (int i = 0; i < tam_vetor; i++)
            printf("[%03d] ", vetor[i]);
      }

      printf("\n[Final] Tempo total: %f segundos\n", end_time - start_time);

      free(vetor);
      MPI_Finalize();
      return 0;
   }

   //---------------------
   // Versao Paralela
   //---------------------
   // recebo vetor
   if (my_rank != 0)
   {
      // não sou a raiz, tenho pai
      rank_pai = (my_rank - 1) / 2;
      if (debug)
      {
         printf("[Filho %d] Esperando vetor do pai %d \n", my_rank, rank_pai);
      }

      // verifica tamanho do vetor
      MPI_Probe(rank_pai, 0, MPI_COMM_WORLD, &status);
      MPI_Get_count(&status, MPI_INT, &tam_vetor);

      vetor = (int *)malloc(sizeof(int) * tam_vetor);
      MPI_Recv(vetor, tam_vetor, MPI_INT, rank_pai, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
   }
   else
   {
      // defino tamanho inicial do vetor
      tam_vetor = atoi(argv[1]);
      vetor = (int *)malloc(sizeof(int) * tam_vetor);

      // sou a raiz e portanto gero o vetor - ordem reversa
      inicializa(vetor, tam_vetor);
      if (debug)
      {
         printf("\nVetor: ");
         for (int i = 0; i < tam_vetor; i++) /* print unsorted array */
            printf("[%03d] ", vetor[i]);
      }
   }

   start_time = MPI_Wtime();

   // dividir ou conquistar?

   if (tam_vetor <= DELTA)
   {
      // conquisto
      if (debug)
      {
         printf("[Filho %d] Ordenando por BS vetor de tamanho %d\n", my_rank, tam_vetor);
      }
      bs(tam_vetor, vetor);
   }
   else
   {
      // dividir
      // quebrar em duas partes e mandar para os filhos
      if (debug)
      {
         printf("[Filho %d] Dividindo vetor de tamanho %d\n", my_rank, tam_vetor);
      }
      int filho_esq = my_rank * 2 + 1;
      int filho_dir = my_rank * 2 + 2;

      int tam_esq = tam_vetor / 2;
      int tam_dir = tam_vetor - tam_esq;

      if (filho_esq >= num_procs || filho_dir >= num_procs)
      {
         // sem filhos restantes para dividir
         if (debug || 1)
         {
            printf("[Filho %d] Ordenando por BS vetor de tamanho %d (sem filhos)\n", my_rank, tam_vetor);
         }
         bs(tam_vetor, vetor);
      }
      else
      {
         if (debug)
         {
            printf("[Filho %d] Enviando para filhos %d (tamanho %d) e %d (tamanho %d)\n",
                   my_rank, filho_esq, tam_esq, filho_dir, tam_dir);
         }
         MPI_Send(&vetor[0], tam_esq, MPI_INT, filho_esq, 0, MPI_COMM_WORLD);       // mando metade inicial do vetor
         MPI_Send(&vetor[tam_esq], tam_dir, MPI_INT, filho_dir, 0, MPI_COMM_WORLD); // mando metade final
         // MPI_Request reqs[2];
         // int num_reqs = 0;

         // printf("Processo %d enviando para filhos %d (tamanho %d) e %d (tamanho %d)\n",
         //        my_rank, filho_esq, tam_esq, filho_dir, tam_dir);

         // // Envio não bloqueante para o filho esquerdo (se existir)
         // if (filho_esq < num_procs)
         // {
         //    MPI_Isend(vetor, tam_esq, MPI_INT, filho_esq, 0, MPI_COMM_WORLD, &reqs[num_reqs++]);
         // }

         // // Envio não bloqueante para o filho direito (se existir)
         // if (filho_dir < num_procs)
         // {
         //    MPI_Isend(&vetor[tam_esq], tam_dir, MPI_INT, filho_dir, 0, MPI_COMM_WORLD, &reqs[num_reqs++]);
         // }

         // if (num_reqs > 0) MPI_Waitall(num_reqs, reqs, MPI_STATUSES_IGNORE);

         // if (filho_esq < num_procs)
         //    MPI_Recv(&vetor[0], tam_esq, MPI_INT, filho_esq, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
         // if (filho_dir < num_procs)
         //    MPI_Recv(&vetor[tam_esq], tam_dir, MPI_INT, filho_dir, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

         // receber dos filhos

         MPI_Recv(&vetor[0], tam_esq, MPI_INT, filho_esq, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
         MPI_Recv(&vetor[tam_esq], tam_dir, MPI_INT, filho_dir, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

         // intercalo vetor inteiro
         if (debug)
         {
            printf("[Filho %d] Isntercalando vetor de tamanho %d\n", my_rank, tam_vetor);
         }
         int *vetor_intercalado = interleaving(vetor, tam_vetor);
         // int *vetor_intercalado = interleaving(vetor, tam_esq, tam_dir);

         free(vetor);
         vetor = vetor_intercalado;
      }
   }

   // mando para o pai

   if (my_rank != 0)
   {
      if (debug)
      {
         printf("[Filho %d] Enviando vetor ordenado de tamanho %d para o pai %d\n", my_rank, tam_vetor, rank_pai);
      }
      MPI_Send(vetor, tam_vetor, MPI_INT, rank_pai, 0, MPI_COMM_WORLD);
      free(vetor);
   }
   else
   {
      if (debug)
      {
         printf("\nVetor: ");
         for (int i = 0; i < tam_vetor; i++) /* print sorted array */
            printf("[%03d] ", vetor[i]);
      }
      free(vetor);
   }

   end_time = MPI_Wtime();
   if (my_rank == 0)
   {
      printf("\n[Final] Tempo total: %f segundos\n", end_time - start_time);
   }

   MPI_Finalize();

   return 0;
}
