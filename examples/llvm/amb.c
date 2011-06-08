int ambfork( int n ) {
    while ( n > 0 ) {
        if ( amb() )
            return n;
        -- n;
    }
}

int main() {
    int fork;
    fork = ambfork( 3 );
    if ( amb() )
        fork = 2;
    trace( "ambfork: %d", fork );
    return 0;
}
