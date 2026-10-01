if(WITH_XC_BROWSER)
    target_sources(testbrowser PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/TestBrowserAdditionalUrlQuery.cpp")
endif()

add_unit_test(NAME testcurrenttotp SOURCES TestCurrentTotp.cpp LIBS testsupport ${TEST_LIBRARIES})
