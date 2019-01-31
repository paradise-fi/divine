// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>

#include <lart/abstract/placeholder.h>

namespace lart::abstract
{
    const Bimap< Placeholder::Type, std::string > Placeholder::TypeTable = {
         { Placeholder::Type::PHI    , "phi"     }
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
    };

    const Bimap< Placeholder::Level, std::string > Placeholder::LevelTable = {
         { Placeholder::Level::Abstract , "abstract" }
        ,{ Placeholder::Level::Taint    , "taint" }
    };
} // namespace lart::abstract
