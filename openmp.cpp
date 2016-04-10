#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <omp.h>
#include "common.h"
#include "ADT.cpp"

//
//  benchmarking program
//
int main( int argc, char **argv )
{    
    if( find_option( argc, argv, "-h" ) >= 0 )
    {
        printf( "Options:\n" );
        printf( "-h to see this help\n" );
        printf( "-p <int> to set number of threads\n" );
        printf( "-n <int> to set number of particles\n" );
        printf( "-s <int> to set the number of steps in the simulation\n" );
        printf( "-o <filename> to specify the output file name\n" );
        printf( "-f <int> to set the frequency of saving particle coordinates\n" );
        return 0;
    }

    int n = read_int( argc, argv, "-n", 1000 );
    int n_threads = read_int(argc, argv, "-p", 2);
    int steps = read_int( argc, argv, "-s", NSTEPS );
    int savefreq = read_int( argc, argv, "-f", SAVEFREQ );
    char *savename = read_string( argc, argv, "-o", NULL );

    FILE *fsave = savename ? fopen( savename, "w" ) : NULL;

    particle_t *particles = (particle_t*) malloc( n * sizeof(particle_t) );
    set_size( n );
    init_particles( n, particles );

    //Create new grid and insert particle onto grid
    Hash::ADT* grid = new Hash::ADT(n, size, cutoff);
    insert_into_grid(n, particles, grid);

    //Set number of threads to n_threads
    omp_set_num_threads(n_threads);

    //
    //  simulate a number of time steps
    //
    double simulation_time = read_timer( );

    #pragma omp parallel
    for( int step = 0; step <steps; step++ )
    {
        //
        //  compute all forces
        //
        #pragma omp for
        for( int i = 0; i < n; i++ )
        {
            particles[i].ax = particles[i].ay = 0;
            
            
            //
            // Iterate through the neighbors of the current particle.
            // This is O(n)
            //
            Hash::ADT::surr_iterator iter_neigh;
            Hash::ADT::surr_iterator iter_end = grid->surr_end(
                                particles[i]);

            for ( iter_neigh = grid->surr_begin(particles[i]); iter_neigh != iter_end; ++iter_neigh) {
                apply_force(particles[i], ** iter_neigh);
            }
        }
        
        
    
        //
        //  move particles
        //
        #pragma omp for
        for( int i = 0; i < n; i++ ) 
            move( particles[i] );

        
        //
        //Update manual grid set
        //
        #pragma omp single
        {
            grid->clear();
            insert_into_grid(n, particles, grid);
        }
        
        
        //
        //  save if necessary
        //
        #pragma omp master
        if( fsave && (step % savefreq) == 0 )
            save( fsave, n, particles );
    }
    simulation_time = read_timer( ) - simulation_time;
    
    printf("n = %d, n_threads = %d, steps = %d, savefreq = %d, simulation time = %g seconds\n", n, n_threads, steps, savefreq, simulation_time);
    //
    //  release resources
    //
    delete grid;
    free( particles );
    if( fsave )
        fclose( fsave );
    
    return 0;
}
