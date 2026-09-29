#include <stdio.h>
#include <chrono>

const int SIZE = 1000;
const int ITERATIONS = 100;

// Global 2D arrays
// Global is used so large arrays do not overflow the stack.
int grid[SIZE][SIZE];
int nextGrid[SIZE][SIZE];


// Count the alive neighbours of one cell
int countNeighbors(int row, int col)
{
    int count = 0;

    for (int i = -1; i <= 1; i++)
    {
        for (int j = -1; j <= 1; j++)
        {
            // Do not count the current cell
            if (i == 0 && j == 0)
            {
                continue;
            }

            int newRow = row + i;
            int newCol = col + j;

            // Check that the neighbour is inside the grid
            if (newRow >= 0 && newRow < SIZE &&
                newCol >= 0 && newCol < SIZE)
            {
                count += grid[newRow][newCol];
            }
        }
    }

    return count;
}


// Apply Conway's Game of Life rules
int getNextState(int currentState, int neighbors)
{
    // Current cell is alive
    if (currentState == 1)
    {
        // Alive cell survives with 2 or 3 neighbours
        if (neighbors == 2 || neighbors == 3)
        {
            return 1;
        }
        else
        {
            return 0;
        }
    }

    // Current cell is dead
    else
    {
        // Dead cell becomes alive with exactly 3 neighbours
        if (neighbors == 3)
        {
            return 1;
        }
        else
        {
            return 0;
        }
    }
}


// Copy nextGrid into grid
void copyGrid()
{
    for (int row = 0; row < SIZE; row++)
    {
        for (int col = 0; col < SIZE; col++)
        {
            grid[row][col] = nextGrid[row][col];
        }
    }
}


// Count alive cells
int countAliveCells()
{
    int count = 0;

    for (int row = 0; row < SIZE; row++)
    {
        for (int col = 0; col < SIZE; col++)
        {
            count += grid[row][col];
        }
    }

    return count;
}


int main()
{
    // Create the initial grid
    // This creates the same deterministic starting pattern
    // every time the program runs.
    for (int row = 0; row < SIZE; row++)
    {
        for (int col = 0; col < SIZE; col++)
        {
            if ((row * 31 + col * 17) % 5 == 0)
            {
                grid[row][col] = 1;
            }
            else
            {
                grid[row][col] = 0;
            }
        }
    }


    // Start timing
    auto start = std::chrono::high_resolution_clock::now();


    // Run Game of Life for 100 iterations
    for (int iteration = 0; iteration < ITERATIONS; iteration++)
    {
        // Calculate the next state of every cell
        for (int row = 0; row < SIZE; row++)
        {
            for (int col = 0; col < SIZE; col++)
            {
                int neighbors = countNeighbors(row, col);

                nextGrid[row][col] =
                    getNextState(grid[row][col], neighbors);
            }
        }

        // Make nextGrid the current grid
        copyGrid();
    }


    // Stop timing
    auto end = std::chrono::high_resolution_clock::now();


    // Calculate execution time
    std::chrono::duration<double, std::milli> duration =
        end - start;


    // Print results
    printf("Game of Life - CPU 2D Array\n");
    printf("---------------------------\n");
    printf("Grid size       : %d x %d\n", SIZE, SIZE);
    printf("Iterations      : %d\n", ITERATIONS);
    printf("Alive cells     : %d\n", countAliveCells());
    printf("Execution time  : %.3f ms\n", duration.count());


    return 0;
}