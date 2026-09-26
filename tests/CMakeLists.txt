qt_add_executable(copper_m3uparser_tests test_m3uparser.cpp)
qt_add_executable(copper_channelutils_tests test_channelutils.cpp)
qt_add_executable(copper_database_tests test_database.cpp)

foreach(test_target copper_m3uparser_tests copper_channelutils_tests copper_database_tests)
    target_link_libraries(${test_target} PRIVATE
        CopperCore
        Qt6::Test
    )
endforeach()

foreach(test_target copper_m3uparser_tests copper_channelutils_tests copper_database_tests)
    set_target_properties(${test_target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    add_test(NAME ${test_target} COMMAND ${test_target})
endforeach()