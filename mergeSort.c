#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

#define DEBUG 1       // comentar esta linha quando for medir tempo
#define ARRAY_SIZE 40 // trabalho final com o valores 10.000, 100.000, 1.000.000
#define DELTA 5       // tamanho minimo para dividir o vetor

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

// int *mergesort()
// {
//    int vetor[];
//    int tam_vetor;

//    int my_rank, num_procs;
//    MPI_Init(NULL, NULL);
//    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
//    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);

//    // recebo vetor

//    if (my_rank != 0)
//    {
//       MPI_Recv(vetor, pai);                        // não sou a raiz, tenho pai
//       MPI_Get_count(&Status, MPI_INT, &tam_vetor); // descubro tamanho da mensagem recebida
//    }
//    else
//    {
//       tam_vetor = ARRAY_SIZE;       // defino tamanho inicial do vetor
//       inicializa(vetor, tam_vetor); // sou a raiz e portanto gero o vetor - ordem reversa
//    }

//    // dividir ou conquistar?

//    if (tam_vetor <= DELTA)
//       bs(tam_vetor, vetor); // conquisto
//    else
//    {
//       // dividir
//       // quebrar em duas partes e mandar para os filhos

//       MPI_Send(&vetor[0], filho esquerda, tam_vetor / 2);            // mando metade inicial do vetor
//       MPI_Send(&vetor[tam_vetor / 2], filho direita, tam_vetor / 2); // mando metade final

//       // receber dos filhos

//       MPI_Recv(&vetor[0], filho esquerda);
//       MPI_Recv(&vetor[tam_vetor / 2], filho direita);

//       // intercalo vetor inteiro

//       interleaving(vetor, tam_vetor);
//    }

//    // mando para o pai

//    if (my_rank != 0)
//       MPI_Send(vetor, pai, tam_vetor); // tenho pai, retorno vetor ordenado pra ele
//    else
//       Mostra(vetor); // sou o raiz, mostro vetor

//    MPI_Finalize();
// }

/* recebe um ponteiro para um vetor que contem as mensagens recebidas dos filhos e            */
/* intercala estes valores em um terceiro vetor auxiliar. Devolve um ponteiro para este vetor */ */

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

int main(int argc, char *argv[])
{
   if (argc != 2)
   {
      printf("Usage: %s <array_size>\n", argv[0]);
      return 1;
   }

   double start_time, end_time;
   int vetor[];
   int tam_vetor;
   MPI_Status status;

   int my_rank, num_procs;
   MPI_Init(&argc, &argv);
   MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
   MPI_Comm_size(MPI_COMM_WORLD, &num_procs);

   // recebo vetor

   if (my_rank != 0)
   {
      // não sou a raiz, tenho pai
      int rank_pai = (my_rank - 1) / 2;
      MPI_Recv(vetor, tam_vetor, MPI_INT, rank_pai, 0, MPI_COMM_WORLD, &status);
      // descubro tamanho da mensagem recebida
      MPI_Get_count(&status, MPI_INT, &tam_vetor);
   }
   else
   {
      // defino tamanho inicial do vetor
      tam_vetor = atoi(argv[1]);
      vetor = (int *)malloc(sizeof(int) * tam_vetor);

      // sou a raiz e portanto gero o vetor - ordem reversa
      inicializa(vetor, tam_vetor);
#ifdef DEBUG
      printf("\nVetor: ");
      for (int i = 0; i < ARRAY_SIZE; i++) /* print unsorted array */
         printf("[%03d] ", vetor[i]);
#endif
   }

   start_time = MPI_Wtime();

   // dividir ou conquistar?

   if (tam_vetor <= DELTA)
   {
      // conquisto
      bs(tam_vetor, vetor);
   }
   else
   {
      // dividir
      // quebrar em duas partes e mandar para os filhos

      int filho_esq = my_rank * 2 + 1;
      int filho_dir = my_rank * 2 + 2;

      if (filho_esq >= num_procs || filho_dir >= num_procs)
      {
         //sem filhos restantes para dividir
         bs(tam_vetor, vetor);
      }
      else
      {

         MPI_Send(&vetor[0], tam_vetor / 2, MPI_INT, filho_esq, 0, MPI_COMM_WORLD); // mando metade inicial do vetor
         MPI_Send(&vetor[tam_vetor / 2], filho filho_dir, tam_vetor / 2);           // mando metade final
      }

      // receber dos filhos

      MPI_Recv(&vetor[0], filho_esq);
      MPI_Recv(&vetor[tam_vetor / 2], filho_dir);

      // intercalo vetor inteiro

      interleaving(vetor, tam_vetor);
   }

   // mando para o pai

   if (my_rank != 0)
      MPI_Send(vetor, pai, tam_vetor); // tenho pai, retorno vetor ordenado pra ele
   else
   {
#ifdef DEBUG
      printf("\nVetor: ");
      for (int i = 0; i < ARRAY_SIZE; i++) /* print sorted array */
         printf("[%03d] ", vetor[i]);
#endif
   }

   end_time = MPI_Wtime();
   if (my_rank == 0)
   {
      printf("\nTempo total: %f segundos\n", end_time - start_time);
   }

   MPI_Finalize();

   return 0;
}