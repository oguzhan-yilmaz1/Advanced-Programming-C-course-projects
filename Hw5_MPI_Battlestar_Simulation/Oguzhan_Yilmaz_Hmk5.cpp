/*
Author: Oguzhan Yilmaz 
Class: ECE4122 
Last Date Modified: 11/06/2019

Description:
This program simulates the Battlestar Buzzy. Buzzy and 7 yellow jackets are moving at constant
speed and their location gets updated every time step. The information is handled and 
processed in a distributed manner using MPI. Make sure in.dat is in the same directory
as the program.
*/

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <iomanip>
#include "mpi.h"
#include <cstring>
using namespace std;

/* Loads the 'in.dat' file to the program. Parses into relevant variables.
Make sure in.dat is in the same directory.
@param timeL the time limit specifying the number of loops we will run
@param maxTh maximum thruster force that can be applied to the yellow jackets
@param arr array containing initial values in the following format at each row 
for each ship: x y z v dx dy dz status Fx Fy Fz
*/
void LoadInputFile(int &timeL, int &maxTh, double arr[8][11]) 
{
    std::ifstream file;
    file.open("in.dat");
    file >> timeL;
    file >> maxTh;
    //loop through array
    for (int i = 0; i < 8; i++)
    {
        for (int j = 0; j < 7; j++) 
        {
            file >> arr[i][j];
            if (j == 6) {
                arr[i][j + 1] = 1;
                arr[i][j + 2] = 0; // forces are 0, not considered in this implementation
                arr[i][j + 3] = 0; // stored just for displaying purposes
                arr[i][j + 4] = 0;
            }
        }
    }
}

/*
Calculates position of buzzy
@param arr 2D array containing data values in the following format at each row 
for each ship: x y z v dx dy dz status Fx Fy Fz
*/
void CalculateBuzzyXYZ(double infos[8][11]) 
{
    infos[0][0] = infos[0][0] + infos[0][3] * infos[0][4];
    infos[0][1] = infos[0][1] + infos[0][3] * infos[0][5];
    infos[0][2] = infos[0][2] + infos[0][3] * infos[0][6];
}

/*
Calculates position of yellow jacket
@param arr array containing data values of the specific yello jacket in the following format: 
x y z v dx dy dz status Fx Fy Fz
*/
void CalculateYellowJacketXYZ(double infos[11]) 
{
    infos[0] = infos[0] + infos[3] * infos[4];
    infos[1] = infos[1] + infos[3] * infos[5];
    infos[2] = infos[2] + infos[3] * infos[6];

}


/*
main entry of the program
*/
int main(int argc, char**argv)
{
    int  numtasks, rank, rc, i, j;
    int timeLength, maxThrust;
    double shipInfo[8][11]; // array holding data for each ship
    double *recvInfo;  // buffer received from all processes concatenated together
    double sendArray[11]; // buffer to be sent to all processes

    rc = MPI_Init(&argc, &argv);

    if (rc != MPI_SUCCESS)
    {
        printf("Error starting MPI program. Terminating.\n");
        MPI_Abort(MPI_COMM_WORLD, rc);
    }

    MPI_Comm_size(MPI_COMM_WORLD, &numtasks); //get number of tasks/processes
    MPI_Comm_rank(MPI_COMM_WORLD, &rank); // get rank
    recvInfo = (double*) malloc(numtasks*sizeof(double)*11); //allocate receive buffer
    
    // Seed the random number generator to get different results each time
    srand(rank);
    if (rank == 0)
    {
        // Load in.dat file
        LoadInputFile(timeLength, maxThrust, shipInfo);
    }
    
    // Broadcast to yellowjackets
    MPI_Bcast(shipInfo, 88, MPI_DOUBLE, 0, MPI_COMM_WORLD); //share the data for the ships
    MPI_Bcast(&timeLength,1,MPI_INT,0,MPI_COMM_WORLD); // share the time length
    MPI_Bcast(&maxThrust,1,MPI_INT,0,MPI_COMM_WORLD); // share the max thrust value

    MPI_Barrier(MPI_COMM_WORLD); // wait until all processes have completed so far till here
    // Loop through the number of time steps
    for (int round = 0; round < timeLength; ++round)
    {
        if (rank == 0)
        {
            // Calculate Buzzy new location
            CalculateBuzzyXYZ(shipInfo);
            
            for (i = 1; i < 8; i++)
            {
                // print the info to console
                std::cout<< i <<","<<(int)shipInfo[i][7]<<",";
                std::cout<<setprecision(6)<<shipInfo[i][0]<<","<<shipInfo[i][1]<<","<<shipInfo[i][2]<<","<<shipInfo[i][8]<<","<<shipInfo[i][9]<<","<<shipInfo[i][10];
                std::cout<<std::endl;
            }
        }
        else
        {
            // Calculate yellow jacket new location
            CalculateYellowJacketXYZ(shipInfo[rank]);
        }
        // initialize the send buffer to zero and copy the relevant row of shipInfo into that
        memset(sendArray, 0, 11 * sizeof(double));
        memcpy(sendArray, shipInfo[rank], 11 * sizeof(double));
        // gather the information from all of the processes
        MPI_Allgather(sendArray, 11, MPI_DOUBLE, recvInfo, 11, MPI_DOUBLE, MPI_COMM_WORLD);
        // update the local copy of the shipInfo in the current process
        for (j = 0; j < 8; j++)
        {
            memcpy(shipInfo[j], recvInfo + j * 11, sizeof(double) * 11);
        }

    }

    MPI_Finalize();
}