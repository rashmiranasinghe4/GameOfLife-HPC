#include <stdio.h>
#include <cuda_runtime.h>

const int SIZE = 1000;
const int ITERATIONS = 100;
const int THREADS_PER_BLOCK = 256;


__global__ void gameOfLifeKernel(const int* grid, int* nextGrid)
{
    int index = blockIdx.x * blockDim.x + threadIdx.x;

    if (index >= SIZE * SIZE)
    {
        return;
    }

    int row = index / SIZE;
    int col = index % SIZE;

    int neighbors = 0;

    for (int i = -1; i <= 1; i++)
    {
        for (int j = -1; j <= 1; j++)
        {
            if (i == 0 && j == 0)
            {
                continue;
            }

            int newRow = row + i;
            int newCol = col + j;

            if (newRow >= 0 && newRow < SIZE &&
                newCol >= 0 && newCol < SIZE)
            {
                neighbors += grid[newRow * SIZE + newCol];
            }
        }
    }

    int currentState = grid[index];

    if (currentState == 1)
    {
        if (neighbors == 2 || neighbors == 3)
        {
            nextGrid[index] = 1;
        }
        else
        {
            nextGrid[index] = 0;
        }
    }
    else
    {
        if (neighbors == 3)
        {
            nextGrid[index] = 1;
        }
        else
        {
            nextGrid[index] = 0;
        }
    }
}


int main()
{
    const int totalCells = SIZE * SIZE;
    const size_t bytes = totalCells * sizeof(int);

    int* h_grid = new int[totalCells];

    for (int row = 0; row < SIZE; row++)
    {
        for (int col = 0; col < SIZE; col++)
        {
            if ((row * 31 + col * 17) % 5 == 0)
            {
                h_grid[row * SIZE + col] = 1;
            }
            else
            {
                h_grid[row * SIZE + col] = 0;
            }
        }
    }

    int* d_grid;
    int* d_nextGrid;

    cudaMalloc(&d_grid, bytes);
    cudaMalloc(&d_nextGrid, bytes);

    cudaMemcpy(
        d_grid,
        h_grid,
        bytes,
        cudaMemcpyHostToDevice
    );

    int blocks = (totalCells + THREADS_PER_BLOCK - 1)
                 / THREADS_PER_BLOCK;


    cudaEvent_t start, stop;

    cudaEventCreate(&start);
    cudaEventCreate(&stop);

    cudaEventRecord(start);


    for (int iteration = 0; iteration < ITERATIONS; iteration++)
    {
        gameOfLifeKernel<<<blocks, THREADS_PER_BLOCK>>>(
            d_grid,
            d_nextGrid
        );

        int* temp = d_grid;
        d_grid = d_nextGrid;
        d_nextGrid = temp;
    }


    cudaEventRecord(stop);
    cudaEventSynchronize(stop);


    float milliseconds = 0;

    cudaEventElapsedTime(
        &milliseconds,
        start,
        stop
    );


    cudaMemcpy(
        h_grid,
        d_grid,
        bytes,
        cudaMemcpyDeviceToHost
    );


    int aliveCells = 0;

    for (int i = 0; i < totalCells; i++)
    {
        aliveCells += h_grid[i];
    }


    printf("Game of Life - CUDA GPU\n");
    printf("-----------------------\n");
    printf("Grid size       : %d x %d\n", SIZE, SIZE);
    printf("Iterations      : %d\n", ITERATIONS);
    printf("Threads/block   : %d\n", THREADS_PER_BLOCK);
    printf("Blocks          : %d\n", blocks);
    printf("Alive cells     : %d\n", aliveCells);
    printf("Execution time  : %.3f ms\n", milliseconds);


    cudaFree(d_grid);
    cudaFree(d_nextGrid);

    delete[] h_grid;

    cudaEventDestroy(start);
    cudaEventDestroy(stop);

    return 0;
}