#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <omp.h>
#include <iostream>
#include <cmath>
#include <memory>

double e = 0.00001;

std::unique_ptr<double[]> A;
std::unique_ptr<double[]> b;
int m = 4, n = 4;

char end = 'f';
double t = 0.1;

double loss = 0;
int parallel_loop = 0;

double cpuSecond() {
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return ((double)ts.tv_sec + (double)ts.tv_nsec * 1.e-9);
}

int t_prov = 0;

void proverka(double* x, double* x_predict, int numThread, double sqe, double sqe_b) {
    int sm = 0;
    double r = sqrt(sqe) / sqrt(sqe_b);
    if (loss < r && loss != 0 && loss > 0.999999999) {
        t = t / 10;
        t_prov = 1;
        loss = 0;
        for (int i = 0; i < n; i++) {
            x[i] = 0;
        }
    }
    else {
        if (abs(loss - r) < 0.00001 && sm == 0) {
            t = t * -1;
            sm = 1;
        }
        loss = r;
        if (r < e) end = 't';
    }
}

std::unique_ptr<double[]> matrix_vector_product_omp(double* x, int numThread, double sqe_b) {
    std::unique_ptr<double[]> x_predict(new double[n]);
    double sqe = 0;
#pragma omp parallel num_threads(numThread)
    {
        double pred_res = 0;
        int nthreads = omp_get_num_threads();
        int threadid = omp_get_thread_num();
        int items_per_thread = m / nthreads;
        int lb = threadid * items_per_thread;
        int ub = (threadid == nthreads - 1) ? (m - 1) : (lb + items_per_thread - 1);

        for (int i = lb; i <= ub; i++) {
            x_predict[i] = 0;
            for (int j = 0; j < n; j++) {
                x_predict[i] = x_predict[i] + A[i * n + j] * x[j];
            }
            x_predict[i] = x_predict[i] - b[i];
            pred_res += x_predict[i] * x_predict[i];
            x_predict[i] = x[i] - t * x_predict[i];
        }
#pragma omp atomic
        sqe += pred_res;
    }

    proverka(x, x_predict.get(), numThread, sqe, sqe_b);
    if (t_prov == 1) {
        x_predict.reset(x);
        t_prov = 0;
    }
    return x_predict;
}

std::unique_ptr<double[]> matrix_vector_product_omp_second(double* x, int numThread, double sqe_b) {
    std::unique_ptr<double[]> x_predict(new double[n]);
    double sqe = 0;
#pragma omp parallel num_threads(numThread)
    {
#pragma omp for schedule(dynamic, int(m / (numThread * 3))) nowait reduction(+:sqe)
        for (int i = 0; i < m; i++) {
            x_predict[i] = 0;
            for (int j = 0; j < n; j++) {
                x_predict[i] = x_predict[i] + A[i * n + j] * x[j];
            }
            x_predict[i] = x_predict[i] - b[i];
            sqe += x_predict[i] * x_predict[i];
            x_predict[i] = x[i] - t * x_predict[i];
        }
    }

    proverka(x, x_predict.get(), numThread, sqe, sqe_b);
    if (t_prov == 1) {
        x_predict.reset(x);
        t_prov = 0;
    }
    return x_predict;
}

void run_parallel(int numThread) {
    std::unique_ptr<double[]> x(new double[n]);
    A = std::make_unique<double[]>(m * n);
    b = std::make_unique<double[]>(m);
    double sqe_b = 0;
    if (!A || !b || !x)
    {
        printf("Error allocate memory!\n");
        exit(1);
    }

    double time = cpuSecond();
#pragma omp parallel num_threads(numThread)
    {
        int nthreads = omp_get_num_threads();
        int threadid = omp_get_thread_num();
        int items_per_thread = m / nthreads;
        int lb = threadid * items_per_thread;
        int ub = (threadid == nthreads - 1) ? (m - 1) : (lb + items_per_thread - 1);
        for (int i = lb; i <= ub; i++) {
            for (int j = 0; j < n; j++) {
                if (i == j)
                    A[i * n + j] = 2;
                else
                    A[i * n + j] = 1;
            }
            b[i] = n + 1;
            x[i] = b[i] / A[i * n + i];
#pragma omp atomic
            sqe_b += b[i] * b[i];
        }
    }

    if (parallel_loop == 0) {
        while (end == 'f') {
            x = matrix_vector_product_omp(x.get(), numThread, sqe_b);
        }
    }
    else {
        while (end == 'f') {
            x = matrix_vector_product_omp_second(x.get(), numThread, sqe_b);
        }
    }

    for (int i = 0; i < n; i++) {
        printf("%f ", x[i]);
    }

    time = cpuSecond() - time;

    printf("Elapsed time (parallel): %.6f sec.\n", time);
}
int main(int argc, char** argv) {

    int numThread = 2;
    if (argc > 1)
        numThread = atoi(argv[1]);
    if (argc > 2) {
        m = atoi(argv[2]);
        n = atoi(argv[2]);
    }
    if (argc > 3) {
        parallel_loop = atoi(argv[3]);
    }
    run_parallel(numThread);
}
