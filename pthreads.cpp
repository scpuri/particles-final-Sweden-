#include "common.h"
#include "ADT.cpp"
#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

//
//  global variables
//
int n, n_threads, steps, savefreq;
particle_t * particles;
particle_t * particles_next;
Hash::ADT* grid;
FILE *fsave;

pthread_barrier_t barrier;

//
//  check that pthreads routine call was successful
//
#define P( condition ) {if( (condition) != 0 ) { printf( "\n FAILURE in %s, line %d\n", __FILE__, __LINE__ );exit( 1 );}}

//
//  This is where the action happens
//
void *thread_routine( void *pthread_id )
{
    int thread_id = *(int*)pthread_id;
    int particles_per_thread = (n + n_threads - 1) / n_threads;
    int first = min(  thread_id    * particles_per_thread, n );
    int last  = min( (thread_id+1) * particles_per_thread, n );
    
    //
    //  simulate a number of time steps
    //
    for( int step = 0; step < steps; step++ )
    {
        //
        //  compute forces
        //
        for( int i = first; i < last; i++ )
        {
            particles[i].ax = particles[i].ay = 0;
            
            //
            // Iterate through the neighbors of the current particle.
            // This is O(n)
            //
            Hash::ADT::surr_iterator iter_neigh;
            Hash::ADT::surr_iterator iter_end = grid->surr_end(particles[i]);
            
            for (iter_neigh = grid->surr_begin(particles[i]);
                 iter_neigh != iter_end; ++iter_neigh) {
                apply_force(particles[i], **iter_neigh);
            }
        }
        
        
        //Remove this extra barrier, there is no need to barrier before moving particle.
        //pthread_barrier_wait( &barrier );
        
        //
        //  move particles
        //
        for (int i = first; i < last; i++) {
            particles_next[i] = particles[i];
            move(particles_next[i]);
        }
        
        pthread_barrier_wait( &barrier );
        
        
        // Update grid hash set using thread #0 and use same save if necessary code.
        if (thread_id == 0) {
            grid->clear();
            insert_into_grid(n, particles_next, grid);
            
            //
            //  save if necessary
            //
            if (fsave && (step %savefreq) == 0) {
                save(fsave, n, particles_next);
            }
            
            std::swap(particles, particles_next);
        }
        pthread_barrier_wait( &barrier );
    }
    
    return NULL;
}

//
//  benchmarking program
//
int main( int argc, char **argv )
{
    //
    //  process command line
    //
    if( find_option( argc, argv, "-h" ) >= 0 )
    {
        printf( "Options:\n" );
        printf( "-h to see this help\n" );
        printf( "-n <int> to set the number of particles\n" );
        printf( "-s <int> to set the number of steps in the simulation\n" );
        printf( "-p <int> to set the number of threads\n" );
        printf( "-o <filename> to specify the output file name\n" );
        printf( "-f <int> to the the frequency of saving particle coordinates\n" );
        return 0;
    }
    
    n = read_int( argc, argv, "-n", 1000 );
    steps = read_int( argc, argv, "-s", NSTEPS );
    savefreq = read_int( argc, argv, "-f", SAVEFREQ );
    n_threads = read_int( argc, argv, "-p", 2 );
    char *savename = read_string( argc, argv, "-o", NULL );
    
    //
    //  allocate resources
    //
    fsave = savename ? fopen( savename, "w" ) : NULL;
    
    particles = (particle_t*) malloc( n * sizeof(particle_t) );
    particles_next = (particle_t*) malloc( n * sizeof(particle_t) );
    set_size( n );
    init_particles( n, particles );
    
    
    //Create new grid and insert particle onto grid
    grid = new Hash::ADT(n, size, cutoff);
    insert_into_grid(n, particles, grid);
    
    
    
    pthread_attr_t attr;
    P( pthread_attr_init( &attr ) );
    P( pthread_barrier_init( &barrier, NULL, n_threads ) );
    
    int *thread_ids = (int *) malloc( n_threads * sizeof( int ) );
    for( int i = 0; i < n_threads; i++ )
        thread_ids[i] = i;
    
    pthread_t *threads = (pthread_t *) malloc( n_threads * sizeof( pthread_t ) );
    
    //
    //  do the parallel work
    //
    double simulation_time = read_timer( );
    for( int i = 1; i < n_threads; i++ )
        P( pthread_create( &threads[i], &attr, thread_routine, &thread_ids[i] ) );
    
    thread_routine( &thread_ids[0] );
    
    for( int i = 1; i < n_threads; i++ )
        P( pthread_join( threads[i], NULL ) );
    simulation_time = read_timer( ) - simulation_time;
    
    printf("n = %d, steps = %d, savefreq = %d, n_threads = %d, simulation time = %g seconds\n", n, steps, savefreq, n_threads, simulation_time);
    
    //
    //  release resources
    //
    
    P( pthread_barrier_destroy( &barrier ) );
    P( pthread_attr_destroy( &attr ) );
    free( thread_ids );
    free( threads );
    delete grid;
    free( particles );
    if( fsave )
        fclose( fsave );
    
    return 0;
}
