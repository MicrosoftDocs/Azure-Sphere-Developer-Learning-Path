CMAKE_MINIMUM_REQUIRED(VERSION 3.8)
PROJECT(azsphere_libs C)

################################################################################
# Source groups
################################################################################
set(Source
    "./src/azure_iot.c"
    "./src/config.c"
    "./src/device_twins.c"
    "./src/direct_methods.c"
    "./src/eventloop_timer_utilities.c"
    "./src/inter_core.c"
    "./src/parson.c"
    "./src/peripheral_gpio.c"
    "./src/terminate.c"
    "./src/timer.c"
    "./src/utilities.c"
)
source_group("Source" FILES ${Source})

set(ALL_FILES
    ${Source}
)

################################################################################
# Target
################################################################################
add_library(${PROJECT_NAME} STATIC ${ALL_FILES})


set(ROOT_NAMESPACE azsphere_libs)
target_include_directories(${PROJECT_NAME} PUBLIC ${AZURE_SPHERE_API_SET_DIR}/usr/include/azureiot ../IntercoreContract)
target_include_directories(${PROJECT_NAME} PUBLIC include )
set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_GLOBAL_KEYWORD "AzureSphere"
)

target_link_libraries (${PROJECT_NAME} applibs pthread gcc_s c azureiot)
