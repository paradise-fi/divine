appendFlag( "CXX;C" "-Wno-write-strings" )
appendFlag( "CXX;C" "-Wno-parentheses" )
appendFlag( "CXX;C" "-Wno-old-style-cast" )

project( libutap )

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/src ${Boost_INCLUDE_DIR} )

add_library( utap src/xmlreader.cpp src/system.cpp src/expression.cpp src/symbols.cpp src/type.cpp src/position.cpp src/statement.cpp src/typeexception.cpp src/typechecker.cpp src/statementbuilder.cpp src/expressionbuilder.cpp src/abstractbuilder.cpp src/systembuilder.cpp src/parser.cc )

target_link_libraries( utap xml2 )
