#include <math.h>
#include <omp.h>
#include <stdio.h>
#include <string.h>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>
#include <time.h>
#include <stdlib.h>
#include <x86intrin.h>

using namespace std;

typedef std::chrono::time_point<std::chrono::steady_clock> TimePoint;

int v_num = 0;
int e_num = 0;
int F0 = 0, F1 = 0, F2 = 0;

vector<vector<int>> edge_index;
vector<vector<float>> edge_val;
vector<int> degree;
vector<int> raw_graph;

float *X0, *W1, *W2, *X1, *X1_inter, *X2, *X2_inter;

void readGraph(char *fname)
{
  ifstream infile(fname);
  int source;
  int end;
  infile >> v_num >> e_num;
  while (!infile.eof())
  {
    infile >> source >> end;
    if (infile.peek() == EOF)
      break;
    raw_graph.push_back(source);
    raw_graph.push_back(end);
  }
}

void raw_graph_to_AdjacencyList()
{
  int src;
  int dst;

  edge_index.resize(v_num);
  edge_val.resize(v_num);
  degree.resize(v_num, 0);

  for (int i = 0; i < raw_graph.size() / 2; i++)
  {
    src = raw_graph[2 * i];
    dst = raw_graph[2 * i + 1];
    edge_index[dst].push_back(src);
    degree[src]++;
  }
}

void edgeNormalization()
{
  for (int i = 0; i < v_num; i++)
  {
    for (int j = 0; j < edge_index[i].size(); j++)
    {
      float val = 1 / sqrt(degree[i]) / sqrt(degree[edge_index[i][j]]);
      edge_val[i].push_back(val);
    }
  }
}

void readFloat(char *fname, float *&dst, int num)
{
  dst = (float *)malloc(num * sizeof(float));
  FILE *fp = fopen(fname, "rb");
  fread(dst, num * sizeof(float), 1, fp);
  fclose(fp);
}

void initFloat(float *&dst, int num)
{
  dst = (float *)malloc(num * sizeof(float));
  memset(dst, 0, num * sizeof(float));
}

float *transposeMatrix(float *a, int in_dim, int out_dim)
{
  float *b = new float[in_dim * out_dim];

  for (int idx = 0; idx < in_dim * out_dim; ++idx)
  {
    int i = idx / out_dim, j = idx % out_dim;
    b[j * in_dim + i] = a[i * out_dim + j];
  }
  return b;
}

void avx_matrix(float *a, float *b_ed, float *c, int v_num, int in_dim, int out_dim)
{
  float *b = new float[in_dim * out_dim];
  b = transposeMatrix(b, in_dim, out_dim);
  int i, j, k;
  float sum = 0.0;
  float assist = 0.0;
  __m256 r0, r1, r2, r3, r4, r5, r6, r7;
  __m256 c0, c1, c2, c3, c4, c5, c6, c7;
  __m256 avx_mul0, avx_mul1, avx_mul2, avx_mul3,
      avx_mul4, avx_mul5, avx_mul6, avx_mul7;
  __m256 avx_sum0 = _mm256_setzero_ps();
  __m256 avx_sum1 = _mm256_setzero_ps();
  __m256 avx_sum2 = _mm256_setzero_ps();
  __m256 avx_sum3 = _mm256_setzero_ps();
  __m256 avx_sum4 = _mm256_setzero_ps();
  __m256 avx_sum5 = _mm256_setzero_ps();
  __m256 avx_sum6 = _mm256_setzero_ps();
  __m256 avx_sum7 = _mm256_setzero_ps();
  __m256 avx_zero = _mm256_setzero_ps();
  int copy_M = in_dim - in_dim % 64;
  int reserve = in_dim % 64;
  for (i = 0; i < v_num; i++)
  {
    for (j = 0; j < out_dim; j++)
    {
      for (k = 0; k < copy_M; k = k + 64)
      {
        r0 = _mm256_loadu_ps(&a[i * in_dim + k]);
        r1 = _mm256_loadu_ps(&a[i * in_dim + k + 8]);
        r2 = _mm256_loadu_ps(&a[i * in_dim + k + 16]);
        r3 = _mm256_loadu_ps(&a[i * in_dim + k + 24]);
        r4 = _mm256_loadu_ps(&a[i * in_dim + k + 32]);
        r5 = _mm256_loadu_ps(&a[i * in_dim + k + 40]);
        r6 = _mm256_loadu_ps(&a[i * in_dim + k + 48]);
        r7 = _mm256_loadu_ps(&a[i * in_dim + k + 56]);

        c0 = _mm256_loadu_ps(&b[j * in_dim + k]);
        c1 = _mm256_loadu_ps(&b[j * in_dim + k + 8]);
        c2 = _mm256_loadu_ps(&b[j * in_dim + k + 16]);
        c3 = _mm256_loadu_ps(&b[j * in_dim + k + 24]);
        c4 = _mm256_loadu_ps(&b[j * in_dim + k + 32]);
        c5 = _mm256_loadu_ps(&b[j * in_dim + k + 40]);
        c6 = _mm256_loadu_ps(&b[j * in_dim + k + 48]);
        c7 = _mm256_loadu_ps(&b[j * in_dim + k + 56]);

        avx_mul0 = _mm256_mul_ps(r0, c0);
        avx_mul1 = _mm256_mul_ps(r1, c1);
        avx_mul2 = _mm256_mul_ps(r2, c2);
        avx_mul3 = _mm256_mul_ps(r3, c3);
        avx_mul4 = _mm256_mul_ps(r4, c4);
        avx_mul5 = _mm256_mul_ps(r5, c5);
        avx_mul6 = _mm256_mul_ps(r6, c6);
        avx_mul7 = _mm256_mul_ps(r7, c7);

        avx_sum0 = _mm256_add_ps(avx_sum0, avx_mul0);
        avx_sum1 = _mm256_add_ps(avx_sum1, avx_mul1);
        avx_sum2 = _mm256_add_ps(avx_sum2, avx_mul2);
        avx_sum3 = _mm256_add_ps(avx_sum3, avx_mul3);
        avx_sum4 = _mm256_add_ps(avx_sum4, avx_mul4);
        avx_sum5 = _mm256_add_ps(avx_sum5, avx_mul5);
        avx_sum6 = _mm256_add_ps(avx_sum6, avx_mul6);
        avx_sum7 = _mm256_add_ps(avx_sum7, avx_mul7);
      }
      // 每次向量乘并求和
      avx_sum0 = _mm256_add_ps(avx_sum0, avx_sum1);
      avx_sum2 = _mm256_add_ps(avx_sum2, avx_sum3);
      avx_sum4 = _mm256_add_ps(avx_sum4, avx_sum5);
      avx_sum6 = _mm256_add_ps(avx_sum6, avx_sum7);
      avx_sum0 = _mm256_add_ps(avx_sum0, avx_sum2);
      avx_sum2 = _mm256_add_ps(avx_sum4, avx_sum6);
      avx_sum0 = _mm256_add_ps(avx_sum0, avx_sum2);
      // 每次求出的c[i,j]
      avx_sum0 = _mm256_hadd_ps(avx_sum0, avx_zero);
      avx_sum0 = _mm256_hadd_ps(avx_sum0, avx_zero);
      assist = avx_sum0[0] + avx_sum0[4];
      c[i * out_dim + j] += assist;
      // 寄存器归0
      avx_sum0 = _mm256_setzero_ps();
      avx_sum1 = _mm256_setzero_ps();
      avx_sum2 = _mm256_setzero_ps();
      avx_sum3 = _mm256_setzero_ps();
      avx_sum4 = _mm256_setzero_ps();
      avx_sum5 = _mm256_setzero_ps();
      avx_sum6 = _mm256_setzero_ps();
      avx_sum7 = _mm256_setzero_ps();
    }
  }
  // 处理剩余的
  assist = 0.0;
  for (i = 0; i < v_num; i++)
  {
    for (j = 0; j < out_dim; j = j + 1)
    {
      for (k = 0; k < reserve; k++)
      {
        assist += a[i * in_dim + copy_M + k] * b[j * in_dim + copy_M + k];
      }
      c[i * out_dim + j] += assist;
      assist = 0.0;
    }
  }
}

void XW(int in_dim, int out_dim, float *in_X, float *out_X, float *W)
{
  avx_matrix(in_X, W, out_X, v_num, in_dim, out_dim);
}

void AX(int dim, float *in_X, float *out_X)
{
  int threads = 20;
#pragma omp parallel for num_threads(threads)
  {
    float(*tmp_in_X)[dim] = (float(*)[dim])in_X;
    float(*tmp_out_X)[dim] = (float(*)[dim])out_X;
    for (int i = 0; i < v_num; i++)
    {
      vector<int> &nlist = edge_index[i];
      for (int j = 0; j < nlist.size(); j++)
      {
        int nbr = nlist[j];
        for (int k = 0; k < dim; k++)
        {
          tmp_out_X[i][k] += tmp_in_X[nbr][k] * edge_val[i][j];
        }
      }
    }
  }
}

void ReLU(int dim, float *X)
{
  for (int i = 0; i < v_num * dim; i++)
    if (X[i] < 0)
      X[i] = 0;
}

void LogSoftmax(int dim, float *X)
{
  int threads = 20;
#pragma omp parallel for num_threads(threads)
  {
    float(*tmp_X)[dim] = (float(*)[dim])X;
    for (int i = 0; i < v_num; i++)
    {
      float max = tmp_X[i][0];
      for (int j = 1; j < dim; j++)
      {
        if (tmp_X[i][j] > max)
          max = tmp_X[i][j];
      }

      float sum = 0;
      for (int j = 0; j < dim; j++)
      {
        sum += exp(tmp_X[i][j] - max);
      }
      sum = log(sum);

      for (int j = 0; j < dim; j++)
      {
        tmp_X[i][j] = tmp_X[i][j] - max - sum;
      }
    }
  }
}

float MaxRowSum(float *X, int dim)
{
  float(*tmp_X)[dim] = (float(*)[dim])X;
  float max = -__FLT_MAX__;

  for (int i = 0; i < v_num; i++)
  {
    float sum = 0;
    for (int j = 0; j < dim; j++)
    {
      sum += tmp_X[i][j];
    }
    if (sum > max)
      max = sum;
  }
  return max;
}

void freeFloats()
{
  free(X0);
  free(W1);
  free(W2);
  free(X1);
  free(X2);
  free(X1_inter);
  free(X2_inter);
}

void somePreprocessing()
{
  // The graph  will be transformed into adjacency list, you can use other data
  // structure such as CSR
  raw_graph_to_AdjacencyList();
}

int main(int argc, char **argv)
{
  // Do NOT count the time of reading files, malloc, and memset
  F0 = atoi(argv[1]);
  F1 = atoi(argv[2]);
  F2 = atoi(argv[3]);

  readGraph(argv[4]);
  readFloat(argv[5], X0, v_num * F0);
  readFloat(argv[6], W1, F0 * F1);
  readFloat(argv[7], W2, F1 * F2);

  initFloat(X1, v_num * F1);
  initFloat(X1_inter, v_num * F1);
  initFloat(X2, v_num * F2);
  initFloat(X2_inter, v_num * F2);

  // Time point at the start of the computation
  TimePoint start = chrono::steady_clock::now();

  // Preprocessing time should be included
  somePreprocessing();

  edgeNormalization();

  // printf("Layer1 XW\n");
  XW(F0, F1, X0, X1_inter, W1);

  // printf("Layer1 AX\n");
  AX(F1, X1_inter, X1);

  // printf("Layer1 ReLU\n");
  ReLU(F1, X1);

  // printf("Layer2 XW\n");
  XW(F1, F2, X1, X2_inter, W2);

  // printf("Layer2 AX\n");
  AX(F2, X2_inter, X2);

  // printf("Layer2 LogSoftmax\n");
  LogSoftmax(F2, X2);

  // You need to compute the max row sum for result verification
  float max_sum = MaxRowSum(X2, F2);

  // Time point at the end of the computation
  TimePoint end = chrono::steady_clock::now();
  chrono::duration<double> l_durationSec = end - start;
  double l_timeMs = l_durationSec.count() * 1e3;

  // Finally, the max row sum and the computing time
  // should be print to the terminal in the following format
  printf("%.8f\n", max_sum);
  printf("%.8lf\n", l_timeMs);

  // Remember to free your allocated memory
  freeFloats();
}
