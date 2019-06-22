namespace divine::vm
{
    std::string opname( int );

    template< typename I >
    decltype( I::opcode, std::string() ) opname( I &insn );
}
