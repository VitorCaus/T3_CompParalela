// mandelbrot_mpi.c
// Compile: mpicc -O2 -o mandelbrot_mpi mandelbrot_mpi.c
// Run example: mpirun -n 25 ./mandelbrot_mpi output.ppm 1920 1080 1000
// If you run with 1 process it will run sequentially.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "mpi.h"

typedef struct {
    double x_min, x_max;
    double y_min, y_max;
    int width, height;
    int start_row;   // inclusive
    int num_rows;    // number of rows in this task
    int max_iter;
} MandelTask;

static int mandelbrot(double real, double imag, int max_iter) {
    double zr = 0.0, zi = 0.0;
    int iter = 0;
    while (zr*zr + zi*zi <= 4.0 && iter < max_iter) {
        double tmp = zr*zr - zi*zi + real;
        zi = 2.0*zr*zi + imag;
        zr = tmp;
        iter++;
    }
    return iter;
}

static void compute_block(const MandelTask *task, int *out_pixels) {
    // out_pixels must be of size task->num_rows * task->width
    double dx = 0.0, dy = 0.0;
    if (task->width > 1) dx = (task->x_max - task->x_min) / (task->width - 1);
    if (task->height > 1) dy = (task->y_max - task->y_min) / (task->height - 1);

    for (int r = 0; r < task->num_rows; ++r) {
        int i = task->start_row + r; // absolute row index in full image
        double imag = task->y_max - i * dy; // y decreases downwards
        for (int c = 0; c < task->width; ++c) {
            double real = task->x_min + c * dx;
            int iter = mandelbrot(real, imag, task->max_iter);
            int color;
            if (iter >= task->max_iter) {
                color = 0; // black for points inside
            } else {
                // map iter to 1..255 (simple grayscale); you can replace with nicer palette
                color = (int)(255.0 * iter / (task->max_iter - 1));
                if (color < 1) color = 1;
                if (color > 255) color = 255;
            }
            out_pixels[r * task->width + c] = color;
        }
    }
}

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc < 5) {
        if (rank == 0) {
            fprintf(stderr, "Usage: %s out.ppm width height max_iter [x_min x_max y_min y_max]\n", argv[0]);
            fprintf(stderr, "Example: %s mandelbrot.ppm 1920 1080 1000 -2.0 1.0 -1.0 1.0\n", argv[0]);
        }
        MPI_Finalize();
        return 1;
    }

    const char *out_filename = argv[1];
    int width = atoi(argv[2]);
    int height = atoi(argv[3]);
    int max_iter = atoi(argv[4]);

    double x_min = -2.0, x_max = 1.0, y_min = -1.0, y_max = 1.0;
    if (argc >= 9) {
        x_min = atof(argv[5]);
        x_max = atof(argv[6]);
        y_min = atof(argv[7]);
        y_max = atof(argv[8]);
    }

    if (width <= 0 || height <= 0 || max_iter <= 0) {
        if (rank == 0) fprintf(stderr, "Invalid width/height/max_iter\n");
        MPI_Finalize();
        return 1;
    }

    // If only one process, compute sequentially and write file
    if (size == 1) {
        MandelTask task_all = {x_min, x_max, y_min, y_max, width, height, 0, height, max_iter};
        int *pixels = (int *)malloc(sizeof(int) * width * height);
        if (!pixels) { fprintf(stderr, "Alloc failed\n"); MPI_Finalize(); return 1; }
        compute_block(&task_all, pixels);

        // Write PPM (P6 binary)
        FILE *fp = fopen(out_filename, "wb");
        if (!fp) { perror("fopen"); free(pixels); MPI_Finalize(); return 1; }
        fprintf(fp, "P6\n%d %d\n255\n", width, height);
        for (int i = 0; i < width * height; ++i) {
            unsigned char rgb[3];
            rgb[0] = (unsigned char)pixels[i];
            rgb[1] = (unsigned char)pixels[i];
            rgb[2] = (unsigned char)(255 - pixels[i]);
            fwrite(rgb, 1, 3, fp);
        }
        fclose(fp);
        free(pixels);
        printf("Image written to %s (sequential)\n", out_filename);
        MPI_Finalize();
        return 0;
    }

    // MPI parallel
    int num_workers = size - 1;
    if (rank == 0) {
        // Coordinator
        // divide rows among workers as evenly as possible
        int base_rows = height / num_workers;
        int remainder = height % num_workers;

        MandelTask *tasks = (MandelTask *)malloc(sizeof(MandelTask) * num_workers);
        int cur_row = 0;
        for (int w = 0; w < num_workers; ++w) {
            int nr = base_rows + (w < remainder ? 1 : 0);
            tasks[w].x_min = x_min;
            tasks[w].x_max = x_max;
            tasks[w].y_min = y_min;
            tasks[w].y_max = y_max;
            tasks[w].width = width;
            tasks[w].height = height;
            tasks[w].start_row = cur_row;
            tasks[w].num_rows = nr;
            tasks[w].max_iter = max_iter;
            cur_row += nr;
        }

        // allocate full image buffer (int per pixel)
        int *full_image = (int *)malloc(sizeof(int) * width * height);
        if (!full_image) { fprintf(stderr, "Alloc failed\n"); free(tasks); MPI_Abort(MPI_COMM_WORLD, 1); }

        // send tasks to each worker rank = w+1
        for (int w = 0; w < num_workers; ++w) {
            int dest = w + 1;
            // send the task struct as bytes
            MPI_Send(&tasks[w], sizeof(MandelTask), MPI_BYTE, dest, 1, MPI_COMM_WORLD);
        }

        // receive results from each worker
        for (int w = 0; w < num_workers; ++w) {
            int src = w + 1;
            int rows = tasks[w].num_rows;
            int count = rows * width;
            if (count == 0) continue;
            int *buffer = (int *)malloc(sizeof(int) * count);
            if (!buffer) { fprintf(stderr, "Alloc failed recv\n"); free(full_image); free(tasks); MPI_Abort(MPI_COMM_WORLD, 1); }
            MPI_Recv(buffer, count, MPI_INT, src, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            // copy into full image at correct offset
            int start = tasks[w].start_row * width;
            memcpy(&full_image[start], buffer, sizeof(int) * count);
            free(buffer);
        }

        // write PPM (P6)
        FILE *fp = fopen(out_filename, "wb");
        if (!fp) { perror("fopen"); free(full_image); free(tasks); MPI_Abort(MPI_COMM_WORLD, 1); }
        fprintf(fp, "P6\n%d %d\n255\n", width, height);
        for (int i = 0; i < width * height; ++i) {
            unsigned char rgb[3];
            rgb[0] = (unsigned char)full_image[i];
            rgb[1] = (unsigned char)full_image[i];
            rgb[2] = (unsigned char)(255 - full_image[i]);
            fwrite(rgb, 1, 3, fp);
        }
        fclose(fp);
        printf("Image written to %s (parallel with %d workers)\n", out_filename, num_workers);

        free(full_image);
        free(tasks);
    } else {
        // Worker
        MandelTask task;
        MPI_Recv(&task, sizeof(MandelTask), MPI_BYTE, 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        if (task.num_rows > 0) {
            int count = task.num_rows * task.width;
            int *buffer = (int *)malloc(sizeof(int) * count);
            if (!buffer) { fprintf(stderr, "Worker %d alloc failed\n", rank); MPI_Abort(MPI_COMM_WORLD, 1); }
            compute_block(&task, buffer);
            MPI_Send(buffer, count, MPI_INT, 0, 2, MPI_COMM_WORLD);
            free(buffer);
        } else {
            // send nothing
        }
    }

    MPI_Finalize();
    return 0;
}

