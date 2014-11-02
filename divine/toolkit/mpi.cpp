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

#if OPT_MPI
        if ( getenv( "OMPI_UNIVERSE_SIZE" ) != nullptr || forceMpi ) {
            s_data->isMpi = true;
            MPI::Init();
            s_data->size = MPI::COMM_WORLD.Get_size();
            s_data->rank = MPI::COMM_WORLD.Get_rank();
            s_data->is_master = !s_data->rank;
        } else
            s_data->isMpi = false;
#else
        brick::_assert::unused( forceMpi );
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
        {   std::unique_lock< std::mutex > _lock( global().mutex );
            notifySlaves( _lock, TAG_ALL_DONE, bitblock() );
#if OPT_MPI
            if ( master() && s_data->isMpi )
                MPI::Finalize();
#endif
        }
        delete s_data;
        s_data = 0;
    }
}

}
