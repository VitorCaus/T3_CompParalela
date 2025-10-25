# T3_CompParalela - MergeSort (Divisão e Conquista)

## Automação:

### Estrutura de pastas necessária:
```
.
├── Makefile.auto
├── Makefile.mpi
├── mergeSort.c
├── listas/
│   ├── sequencial/
│   ├── forte/
│   ├── fraca/
├── resultados/
│   ├── sequencial/
│   ├── forte/
│   ├── fraca/ 

```
### Execução:
```
make -f Makefile.auto
```

### Padrão de arquivos de teste:
```
Nome: ms_N<nodos>_T<threads>.txt

Conteúdo:
V=<tamanho do array>
D=<Debug (0|1)> - opcional (default 0)
```
