#include <divine/toolkit/mpi.h>

namespace divine {
struct Mpi::Data *Mpi::s_data = NULL;

Mpi::Mpi( bool forceMpi )
{
    if (!s_data) {
        s_data = new Data;

        s_data->size = 1;
        s_data->rank = 0;
        s_data->is_master = true;
        s_data->progress = 0;

#ifdef O_MPI
        if ( getenv( "OMPI_UNIVERSE_SIZE" ) != nullptr || forceMpi ) {
            s_data->isMpi = true;
            MPI::Init();
            s_data->size = MPI::COMM_WORLD.Get_size();
            s_data->rank = MPI::COMM_WORLD.Get_rank();
            s_data->is_master = !s_data->rank;
        } else
            s_data->isMpi = false;
#else
        wibble::param::discard( forceMpi );
#endif
        s_data->instances = 1;
    } else
        s_data->instances ++;
}

Mpi::Mpi( const Mpi & ) {
    global().instances ++;
}

Mpi::~Mpi() {
    -- global().instances;
    if (!s_data->instances) {
        wibble::sys::MutexLock _lock( global().mutex );
        notifySlaves( _lock, TAG_ALL_DONE, bitblock() );
#ifdef O_MPI
        if ( master() && s_data->isMpi )
            MPI::Finalize();
#endif
        _lock.drop();
        delete s_data;
        s_data = 0;
    }
}

}
