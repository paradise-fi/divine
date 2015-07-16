project( ltl2dstar )

add_definitions( ${OPT_FLAGS} ${CXX_FLAGS} -fkeep-inline-functions -DINLINE_BITSET -w )

include_directories( src ${Boost_INCLUDE_DIR} )

add_library( ltl2dstar src/common/BitSet.cpp src/StutterSensitivenessInformation.cpp src/LTL2DRA.cpp 
	src/LTL2DSTAR_Scheduler.cpp src/APMonom.cpp src/parsers/nba-parser-lbtt.lex.cpp 
	src/parsers/nba-parser-lbtt.tab.cpp src/parsers/nba-parser-promela.lex.cpp 
	src/parsers/nba-parser-promela.tab.cpp src/plugins/PluginManager.cpp src/ltl2dstar_main.cpp )

if ( LTL3BA )
  target_link_libraries( ltl2dstar ltl3ba )
else()
  target_link_libraries( ltl2dstar ltl2ba )
endif()
