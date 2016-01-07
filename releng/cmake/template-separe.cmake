set( separe ${CMAKE_SOURCE_DIR}/releng/cmake/template-separe.sh )

macro( template_separe V )
  foreach( file ${ARGN} )
    set( filep "${CMAKE_CURRENT_SOURCE_DIR}/${file}" )

    execute_process(
      COMMAND sh ${separe} "${filep}" "${file}"
      OUTPUT_VARIABLE instances_
    )

    string( REPLACE "\n" ";" instances ${instances_} )

    foreach( i ${instances} )
      add_custom_command(
        OUTPUT ${i}
        DEPENDS "${filep}" ${separe}
        COMMAND sh ${separe} ${filep} ${file} ${i}
        VERBATIM
      )
      list( APPEND ${V}_TS_OUTPUTS "${i}" )
    endforeach()
    list( APPEND ${V}_TS_INPUTS ${filep} )
  endforeach()
endmacro()
