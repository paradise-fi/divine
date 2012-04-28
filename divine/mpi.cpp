#include <divine/mpi.h>

namespace divine {
struct Mpi::Data *Mpi::s_data = NULL;

Mpi::Mpi()
{
    if (!s_data) {
        s_data = new Data;

        MPI::Init();
        s_data->size = MPI::COMM_WORLD.Get_size();
        s_data->rank = MPI::COMM_WORLD.Get_rank();
        s_data->is_master = !s_data->rank;
        s_data->progress = 0;
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
        std::cerr << "MPI DONE" << std::endl;
        notifySlaves( TAG_ALL_DONE );
        if ( master() )
            MPI::Finalize();
        delete s_data;
        s_data = 0;
    }
}

}
