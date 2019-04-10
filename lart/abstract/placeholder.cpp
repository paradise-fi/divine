// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>

#include <lart/abstract/placeholder.h>

namespace lart::abstract
{
    const Bimap< Placeholder::Type, std::string > Placeholder::TypeTable = {
         { Placeholder::Type::PHI    , "phi"     }
        ,{ Placeholder::Type::GEP    , "gep"     }
        ,{ Placeholder::Type::Thaw   , "thaw"    }
        ,{ Placeholder::Type::Freeze , "freeze"  }
        ,{ Placeholder::Type::Stash  , "stash"   }
        ,{ Placeholder::Type::Unstash, "unstash" }
        ,{ Placeholder::Type::ToBool , "tobool"  }
        ,{ Placeholder::Type::Assume , "assume"  }
        ,{ Placeholder::Type::Store  , "store"   }
        ,{ Placeholder::Type::Load   , "load"    }
        ,{ Placeholder::Type::Cmp    , "cmp"     }
        ,{ Placeholder::Type::Cast   , "cast"    }
        ,{ Placeholder::Type::Binary , "binary"  }
        ,{ Placeholder::Type::Lift   , "lift"    }
        ,{ Placeholder::Type::Lower  , "lower"   }
        ,{ Placeholder::Type::Call   , "call"    }
        ,{ Placeholder::Type::Memcpy , "memcpy"  }
        ,{ Placeholder::Type::Memmove, "memmove" }
        ,{ Placeholder::Type::Memset , "memset"  }
        ,{ Placeholder::Type::Union , "union"  }
    };

    const Bimap< Placeholder::Level, std::string > Placeholder::LevelTable = {
         { Placeholder::Level::Abstract, "abstract" }
        ,{ Placeholder::Level::Concrete, "concrete" }
    };
} // namespace lart::abstract
