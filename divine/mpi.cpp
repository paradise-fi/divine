#include <divine/mpi.h>

namespace divine {
struct Mpi::Data *Mpi::s_data = NULL;

Mpi::Mpi()
{
    if (!s_data) {
        s_data = new Data;

        s_data->size = 1;
        s_data->rank = 0;
        s_data->is_master = true;
        s_data->progress = 0;

#ifdef O_MPI
        MPI::Init();
        s_data->size = MPI::COMM_WORLD.Get_size();
        s_data->rank = MPI::COMM_WORLD.Get_rank();
        s_data->is_master = !s_data->rank;
#endif
        s_data->instances = 1;
    } else
        s_data->instances ++;
    // std::cerr << "MPI! " << global().instances << std::endl;
}

Mpi::Mpi( const Mpi & ) {
    global().instances ++;
    // std::cerr << "MPI! " << global().instances << std::endl;
}

Mpi::~Mpi() {
    // std::cerr << "~MPI? " << global().instances << std::endl;
    -- global().instances;
    if (!s_data->instances) {
        debug() << "MPI DONE" << std::endl;
        wibble::sys::MutexLock _lock( global().mutex );
        notifySlaves( _lock, TAG_ALL_DONE );
#ifdef O_MPI
        if ( master() )
            MPI::Finalize();
#endif
        _lock.drop();
        delete s_data;
        s_data = 0;
    }
}

}
