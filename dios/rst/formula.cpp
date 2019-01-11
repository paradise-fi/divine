#include <rst/formula.h>

#if __cplusplus >= 201103L

#include <rst/common.h>
#include <sys/metadata.h>
#include <sys/bitcode.h>

using abstract::peek_object;
using abstract::poke_object;

namespace lart::sym {

__invisible __dios::Array< Formula * > __orphan_formulae( _VM_Frame * frame ) noexcept {
    auto *meta = __md_get_pc_meta( frame->pc );
    auto *inst = meta->inst_table;
    auto base = reinterpret_cast< uint8_t * >( frame );

    __dios::Array< Formula * > orphans;
    for ( int i = 0; i < meta->inst_table_size; ++i, ++inst ) {
        if ( inst->val_width == 8 ) {
            auto addr = *reinterpret_cast< void ** >( base + inst->val_offset );
            auto old_flag = __vm_ctl_flag( 0, _DiOS_CF_IgnoreFault );

            if ( inst->opcode == OpCode::Alloca )  {
                // TODO fix peek of undefined value
                if ( auto * formula = peek_object< Formula >( addr ) ) {
                    formula->refcount_decrement();
                }
            }

            if ( __dios_pointer_get_type( addr ) == _VM_PT_Marked ) {
                Formula * formula = static_cast< Formula * >( abstract::weaken( addr ) );
                if ( formula->refcount() == 0 ) {
                    orphans.push_back( formula );
                }
            }

            if ( ( old_flag & _DiOS_CF_IgnoreFault ) == 0 )
                __vm_ctl_flag( _DiOS_CF_IgnoreFault, 0 );
        }
    }

    std::sort(orphans.begin(), orphans.end());
    auto end = std::unique(orphans.begin(), orphans.end());
    orphans.erase( end, orphans.end() );

    return orphans;
}

__invisible void __formula_cleanup( Formula * formula ) noexcept {
    auto clean_child = [] (auto * child) {
        child->refcount_decrement();
        if ( child->refcount() == 0 )
            __formula_cleanup( child );
    };

    if ( isUnary( formula->op() ) ) {
        clean_child( formula->unary.child );
    }
    else if ( isBinary( formula->op() ) ) {
        clean_child( formula->binary.left );
        clean_child( formula->binary.right );
    }

    __dios_safe_free( formula );
}

__invisible void __cleanup_orphan_formulae( _VM_Frame * frame ) noexcept {
    for ( auto * ptr : __orphan_formulae( frame ) ) {
        __formula_cleanup( ptr );
    }
}

} // namespace lart::sym

#endif
