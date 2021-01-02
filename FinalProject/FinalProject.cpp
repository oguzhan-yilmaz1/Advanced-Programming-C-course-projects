/*
Author: Oguzhan Yilmaz
Class: ECE4122
Last Date Modified: December 4th, 2019
Description: Final Project

Using OpenMPI with 16 nodes and OpenGL to simulate Unmanned Aerial Vehicles putting a
half-time show around a football field.

Compiled with:
    module load mesa gcc mvapich2
    mpic++ FinalProject.cpp -lGLU -lglut -std=c++11
Run with:
    mpirun -np 16 ./a.out

EC: Used football field bitmap.
*/

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include "mpi.h"
#include "iomanip"
#include <cmath>
#include <math.h>
#include <cstdlib>
#ifdef __APPLE__
       #define GL_SILENCE_DEPRECATION
       #include <GLUT/glut.h>
       #include <OpenGL/gl.h>

       #include <OpenGl/glu.h>
#else
       #include <GL/glut.h>
#endif
#include <chrono>
#include <thread>
#include "ECE_Bitmap.h"

#define UAVMASS 1.0
#define MAXF 20.0

// Send location and velocity vector in each direction
const int numElements = 6; // x, y, z, vx, vy, vz

const int rcvSize = 16 * 6; // (Main task + 15 UAVs) * numElements

double* rcvbuffer = new double[rcvSize];

double sendBuffer[numElements];

// x,y,z, vx, vy, vz
double currInfo[rcvSize] = {0,0,0,0,0,0,
           -45.72, 24.384, 0, 0, 0, 0, // Top left
           -22.86, 24.384, 0, 0, 0, 0, // Top left mid
                0, 24.384, 0, 0, 0, 0, // Top mid
            22.86, 24.384, 0, 0, 0, 0, // Top right mid
            45.72, 24.384, 0, 0, 0, 0, // Top right

           -45.72, 0, 0, 0, 0, 0, // Mid left
           -22.86, 0, 0, 0, 0, 0, // Mid left mid
                0, 0, 0, 0, 0, 0, // Mid mid
            22.86, 0, 0, 0, 0, 0, // Mid right mid
            45.72, 0, 0, 0, 0, 0, // Mid right

           -45.72, -24.384, 0, 0, 0, 0, // Bot left
           -22.86, -24.384, 0, 0, 0, 0, // Bot left mid
                0, -24.384, 0, 0, 0, 0, // Bot mid
            22.86, -24.384, 0, 0, 0, 0, // Bot right mid
            45.72, -24.384, 0, 0, 0, 0, // Bot right
};

typedef struct Image {
    unsigned long sizeX;
    unsigned long sizeY;
    char *data;
}Image;

int onSphere[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; // flag to indicate UAV on virtual sphere

GLuint texture[1];
// bmp figure
BMP field;

/*
 * Used by the glutReshapeFunc when  window is resized.
 * @param w: the new width of the screen
 * @param h: the new height of the screen
 */
void changeSize(int w, int h)
{
    float ratio = ((float)w) / ((float)h); // window aspect ratio
    glMatrixMode(GL_PROJECTION); // projection matrix is active
    glLoadIdentity(); // reset the projection
    gluPerspective(45.0, ratio, 0.1, 1000.0); // perspective transformation
    glMatrixMode(GL_MODELVIEW); // return to modelview mode
    glViewport(0, 0, w, h); // set viewport (drawing area) to entire window
}

/*
 * Creates a football field in the XY plane, centered on the origin.
 * uses texture from the bmp file
 */
void drawFootballField()
{
    glPushMatrix();
        glBindTexture(GL_TEXTURE_2D, texture[0]);
        glBegin(GL_QUADS);
            glTranslatef(0.0, 0.0, 0.0);
            glTexCoord2f(0, 0);
            glVertex3f(-57.25, -27.5, 0.0);
            glTexCoord2f(1, 0);
            glVertex3f(57.25, -27.5, 0.0);
            glTexCoord2f(1, 1);
            glVertex3f(57.25, 27.5, 0.0);
            glTexCoord2f(0, 1);
            glVertex3f(-57.25, 27.5, 0.0);
        glEnd();
    glBindTexture(GL_TEXTURE_2D, 0);
    glPopMatrix();
}

/*
 * Draws UAVs accoding to specifications (yellow Dodecahedron)
 */
void drawUAVs()
{
    for (int i = 1; i < 16; i++)
    {
        glPushMatrix();
        glColor3ub(255, 255, 0);
        glTranslatef(float(rcvbuffer[i * 6]), float(rcvbuffer[i * 6 + 1]), float(rcvbuffer[i * 6 + 2]));
        glScalef(0.5f / sqrt(3), 0.5f / sqrt(3), 0.5f / sqrt(3));
        glutSolidDodecahedron();
        glPopMatrix();
    }
}

/*
 * Creates a virtual sphere along which the UAVs are going to fly
 */
void drawVirtualSphere()
{
    glColor3ub(0,0,255);
    glPushMatrix();
    glTranslatef(0, 0, 50);
    glutWireSphere(10.0, 10, 8);
    glPopMatrix();
}

//----------------------------------------------------------------------
// Draw the entire scene
//
// We first update the camera location based on its distance from the
// origin and its direction.
//----------------------------------------------------------------------
void renderScene()
{
    glClearColor(0.5, 0.8, 0.9, 0.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // Reset transformations
    glLoadIdentity();
    
    gluLookAt(0.0, 80.0, 120.0, 0.0, 0.0, 25.0, 0.0, 0.0, 1.0);

    glMatrixMode(GL_MODELVIEW);

    drawFootballField();
    drawVirtualSphere();
    drawUAVs();

    glutSwapBuffers(); // Make it all visible
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Allgather(sendBuffer, numElements, MPI_DOUBLE, rcvbuffer, numElements, MPI_DOUBLE, MPI_COMM_WORLD);
}

/*
* Implement multiple gl and glut initializations
*/
void init()
{
    // Set initial parameters
    glDepthMask(GL_TRUE);
    glMatrixMode(GL_PROJECTION);
 
    // Set black background
    glClearColor(0.0, 0.0, 0.0, 0.0);
 
    // Set smooth objects
    glShadeModel(GL_SMOOTH);
    glEnable(GL_DEPTH_TEST);

    field.read("ff.bmp"); // read input
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // Create textures
    glGenTextures(1, texture);
    glBindTexture(GL_TEXTURE_2D, texture[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, 3, field.bmp_info_header.width, field.bmp_info_header.height, 0,
        GL_BGR_EXT, GL_UNSIGNED_BYTE, &field.data[0]);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
    glEnable(GL_TEXTURE_2D);

}

//----------------------------------------------------------------------
// timerFunction  - called whenever the timer fires
// @param id unused
//----------------------------------------------------------------------
void timerFunction(int id)
{
    glutPostRedisplay();
    glutTimerFunc(100, timerFunction, 0);
}

//----------------------------------------------------------------------
// mainOpenGL  - standard GLUT initializations and callbacks
//----------------------------------------------------------------------
void mainOpenGL(int argc, char**argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowPosition(100, 100);
    glutInitWindowSize(400, 400);

    glutCreateWindow("Drone Show");
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_COLOR_MATERIAL);
    glEnable(GL_NORMALIZE);
    // glEnable(GL_LIGHTING);
    // glEnable(GL_LIGHT0);

    // Setup lights as needed
    // ...

    init();

    glutReshapeFunc(changeSize);
    glutDisplayFunc(renderScene);

    glutTimerFunc(100, timerFunction, 0);
    glutMainLoop();
}

/*
 * Function to check if there is an elastic collision
 * Swaps velocities if detects one.
 *
 * @params rank: current process rank
 */
void checkCollision(int rank)
{
    double dx, dy, dz, tmpX, tmpY, tmpZ;
    for (int i = 1; i < 16; i++)
    {
        if (i != rank)
        {
            dx = rcvbuffer[rank * 6] - rcvbuffer[i * 6];
            dy = rcvbuffer[rank * 6 + 1] - rcvbuffer[i * 6 + 1];
            dz = rcvbuffer[rank * 6 + 2] - rcvbuffer[i * 6 + 2];

            if (sqrt(pow(dx, 2.0) + pow(dy, 2.0) + pow(dz, 2.0)) <= 0.01)
            {
                tmpX = rcvbuffer[rank * 6 + 3];
                tmpY = rcvbuffer[rank * 6 + 4];
                tmpZ = rcvbuffer[rank * 6 + 5];
                rcvbuffer[rank * 6 + 3] = rcvbuffer[i * 6 + 3];
                rcvbuffer[rank * 6 + 4] = rcvbuffer[i * 6 + 4];
                rcvbuffer[rank * 6 + 5] = rcvbuffer[i * 6 + 5];
                rcvbuffer[i * 6 + 3] = tmpX;
                rcvbuffer[i * 6 + 4] = tmpY;
                rcvbuffer[i * 6 + 5] = tmpZ;
            }
        }
    }
}

/*
* calculates the location and velocities of UAVs and stores to gather buffers for all processes
* @param rank current process rank
*/
void calculateUAVsLocation(int rank)
{
    double myUAV[numElements], acc_x, acc_y, acc_z, fx, fy, fz;
    checkCollision(rank);
    memcpy(myUAV, rcvbuffer + rank * 6, sizeof(double) * numElements);
    double distToCenter = sqrt(pow(myUAV[0], 2) + pow(myUAV[1], 2) + pow((myUAV[2] - 50.0), 2));
    double velocity = sqrt(pow(myUAV[3], 2) + pow(myUAV[4], 2) + pow(myUAV[5], 2));
    double dirX = -myUAV[0] / distToCenter;
    double dirY = -myUAV[1] / distToCenter;
    double dirZ = (50.0 - myUAV[2]) / distToCenter;
    double effectiveR = (-20 * dirZ + sqrt(400 * pow(dirZ, 2.0) + 1200)) / 2.0; // from solving the physics
    if (distToCenter > 10.1) 
    {
        if (velocity <= 1.8)
        {
            acc_x = effectiveR * dirX;
            acc_y = effectiveR * dirY;
            acc_z = effectiveR * dirZ;
            sendBuffer[0] = myUAV[0] + (myUAV[3] * 0.1) + (acc_x * 0.1 * 0.1 * 0.5);
            sendBuffer[1] = myUAV[1] + (myUAV[4] * 0.1) + (acc_y * 0.1 * 0.1 * 0.5);
            sendBuffer[2] = myUAV[2] + (myUAV[5] * 0.1) + (acc_z * 0.1 * 0.1 * 0.5);
            sendBuffer[3] = myUAV[3] + acc_x * 0.1;
            sendBuffer[4] = myUAV[4] + acc_y * 0.1;
            sendBuffer[5] = myUAV[5] + acc_z * 0.1;
        } else 
        {
            sendBuffer[0] = myUAV[0] + (myUAV[3] * 0.1);
            sendBuffer[1] = myUAV[1] + (myUAV[4] * 0.1);
            sendBuffer[2] = myUAV[2] + (myUAV[5] * 0.1);
            sendBuffer[3] = myUAV[3];
            sendBuffer[4] = myUAV[4];
            sendBuffer[5] = myUAV[5];
        }
    }
    else if (onSphere[rank] == 0)
    {
        onSphere[rank] = 1;
    }

    if (onSphere[rank] == 1)
    {
        double fS = 2 * (distToCenter - 10);
        fx = fS * dirX * UAVMASS;
        fy = fS * dirY * UAVMASS;
        fz = fS * dirZ * UAVMASS;

        double randomX = (double)(rand() % 11);
        double randomY = (double)(rand() % 11);
        double randomZ = (double)(rand() % 3) - 1;
        double cx = -dirY * randomZ - randomY * -dirZ;
        double cy = -dirX * randomZ - randomX * -dirZ;
        double cz = -dirX * randomY - (-dirY) * randomX;
        double magnitude = sqrt(pow(cx, 2) + pow(cy, 2) + pow(cz, 2));
        cx = cx / magnitude;
        cy = cy / magnitude;
        cz = cz / magnitude;
        fx += 0.1 * cx;
        fy += 0.1 * cy;
        fz += 0.1 * cz;
        magnitude = sqrt(pow(fx, 2) + pow(fy, 2) + pow(fz, 2)) / 20.0;
        fx /= magnitude;
        fy /= magnitude;
        fz /= magnitude;
        acc_z = fz / UAVMASS - 10.0;
        acc_x = fx / UAVMASS;
        acc_y = fy / UAVMASS;
        sendBuffer[0] = myUAV[0] + (myUAV[3] * 0.1) + (acc_x * 0.1 * 0.1 * 0.5);
        sendBuffer[1] = myUAV[1] + (myUAV[4] * 0.1) + (acc_y * 0.1 * 0.1 * 0.5);
        sendBuffer[2] = myUAV[2] + (myUAV[5] * 0.1) + (acc_z * 0.1 * 0.1 * 0.5);
        sendBuffer[3] = myUAV[3] + acc_x * 0.1;
        sendBuffer[4] = myUAV[4] + acc_y * 0.1;
        sendBuffer[5] = myUAV[5] + acc_z * 0.1;
    }
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
// Main entry point determines rank of the process and follows the 
// correct program path
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
int main(int argc, char**argv)
{
    srand(time(NULL));

    int numTasks, rank;

    int rc = MPI_Init(&argc, &argv);

    if (rc != MPI_SUCCESS) 
    {
        printf("Error starting MPI program. Terminating.\n");
        MPI_Abort(MPI_COMM_WORLD, rc);
    }

    MPI_Comm_size(MPI_COMM_WORLD, &numTasks);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    int gsize = 0;

    MPI_Comm_size(MPI_COMM_WORLD, &gsize);

    memcpy(rcvbuffer, currInfo, rcvSize * sizeof(double));

    
    if (rank == 0) 
    {
        mainOpenGL(argc, argv);
    }
    else
    {
        // Sleep for 5 seconds
        std::this_thread::sleep_for(std::chrono::seconds(5));
        for (int ii = 0; ii < 600 ; ii++)
        {
            if (ii == 0)
            {
                memcpy(sendBuffer, currInfo + rank * numElements, sizeof(double) * numElements);
            }
            else
            {
                calculateUAVsLocation(rank);
                MPI_Barrier(MPI_COMM_WORLD);
                MPI_Allgather(sendBuffer, numElements, MPI_DOUBLE, rcvbuffer, numElements, MPI_DOUBLE, MPI_COMM_WORLD);  
            }
        }
    }
    MPI_Finalize();
    return 0;
}