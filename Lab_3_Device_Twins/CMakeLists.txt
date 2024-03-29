#  Copyright (c) Microsoft Corporation. All rights reserved.
#  Licensed under the MIT License.

###################################################################################################################

# Select your developer board by removing the # tag from the beginning of the line

# The default board selected is the AVNET Azure Sphere Starter Kit Revision 1.

# If you are NOT using the AVNET Revision 1 board be sure to comment out the AVNET board

set(AVNET TRUE "AVNET Azure Sphere Starter Kit Revision 1 ")    
# set(AVNET_REV_2 TRUE "AVNET Azure Sphere Starter Kit Revision 2 ")             
# set(SEEED_STUDIO_RDB TRUE "Seeed Studio Azure Sphere MT3620 Development Kit (aka Reference Design Board or rdb)")
# set(SEEED_STUDIO_MINI TRUE "Seeed Studio Azure Sphere MT3620 Mini Dev Board")

###################################################################################################################

cmake_minimum_required (VERSION 3.10)
project (AzureSphereAzureIoT C)

# These labs are evergreen
# The CMake Azure Sphere configuration updates to match the Azure Sphere SDK you've installed on your computer.
# For production applications, setting the Azure Sphere configuration in CMake should be intentional and tested.
get_filename_component(PARENT_DIR ${PROJECT_SOURCE_DIR} DIRECTORY)

if (EXISTS "${PARENT_DIR}/tools/cmake/azsphere_config.cmake")
    include(../tools/cmake/azsphere_config.cmake)
    auto_generate_azsphere_config()
else()

    # For information on setting tools revision and target api set see 
    # https://docs.microsoft.com/en-us/azure-sphere/app-development/using-cmake-functions

    azsphere_configure_tools(TOOLS_REVISION "21.10")
    azsphere_configure_api(TARGET_API_SET "11")

endif()


add_subdirectory("../LearningPathLibrary" out)

set(Source
    "main.c"
)
source_group("Source" FILES ${Source})

if(AVNET OR AVNET_REV_2)

    set(Oem

        "../LearningPathLibrary/AVNET/board.c"
    )
    source_group("Oem" FILES ${Oem})    

    add_definitions( -DOEM_AVNET=TRUE )

endif(AVNET OR AVNET_REV_2)

if(SEEED_STUDIO_RDB OR SEEED_STUDIO_MINI)

    set(Oem
        "../LearningPathLibrary/SEEED_STUDIO/board.c"
    )
    source_group("Oem" FILES ${Oem})

    add_definitions( -DOEM_SEEED_STUDIO=TRUE )

endif(SEEED_STUDIO_RDB OR SEEED_STUDIO_MINI)

set(ALL_FILES
    ${Source}
    ${Oem}
)

# Create executable
add_executable(${PROJECT_NAME} ${ALL_FILES})

target_compile_definitions(${PROJECT_NAME} PUBLIC AZURE_IOT_HUB_CONFIGURED)
target_link_libraries(${PROJECT_NAME} applibs pthread gcc_s c azsphere_libs )

target_include_directories(${PROJECT_NAME} PUBLIC ../LearningPathLibrary/include )

target_compile_options(${PROJECT_NAME} PRIVATE -Wno-unknown-pragmas)

if (SEEED_STUDIO_RDB OR SEEED_STUDIO_MINI)
    target_include_directories(${PROJECT_NAME} PUBLIC 
                            ../LearningPathLibrary/SEEED_STUDIO
                            )
endif (SEEED_STUDIO_RDB OR SEEED_STUDIO_MINI)

if(AVNET OR AVNET_REV_2)
    add_subdirectory("../Drivers/AVNET_SK/HighLevel" drivers)
    target_link_libraries(${PROJECT_NAME} avnet_drivers)
    target_include_directories(${PROJECT_NAME} PUBLIC 
                            ../LearningPathLibrary/AVNET
                            ../Drivers/AVNET_SK/HighLevel
                            ../Drivers/AVNET_SK/Common 
                            )
endif(AVNET OR AVNET_REV_2)

set(BOARD_COUNTER 0)

if(AVNET)
    MATH(EXPR BOARD_COUNTER "${BOARD_COUNTER}+1")
    azsphere_target_hardware_definition(${PROJECT_NAME} TARGET_DIRECTORY "../HardwareDefinitions/avnet_mt3620_sk" TARGET_DEFINITION "azure_sphere_learning_path.json")
    message(STATUS "Azure Sphere board selected: AVNET REV 1")
endif(AVNET)

if(AVNET_REV_2)
    MATH(EXPR BOARD_COUNTER "${BOARD_COUNTER}+1")
    azsphere_target_hardware_definition(${PROJECT_NAME} TARGET_DIRECTORY "../HardwareDefinitions/avnet_mt3620_sk_rev2" TARGET_DEFINITION "azure_sphere_learning_path.json")
    message(STATUS "Azure Sphere board selected: AVNET REV 2")
endif(AVNET_REV_2)

if (SEEED_STUDIO_RDB)
    MATH(EXPR BOARD_COUNTER "${BOARD_COUNTER}+1")
    azsphere_target_hardware_definition(${PROJECT_NAME} TARGET_DIRECTORY "../HardwareDefinitions/mt3620_rdb" TARGET_DEFINITION "azure_sphere_learning_path.json")
    message(STATUS "Azure Sphere board selected: SEEED STUDIO RDB")
endif()

if (SEEED_STUDIO_MINI)
    MATH(EXPR BOARD_COUNTER "${BOARD_COUNTER}+1")
    azsphere_target_hardware_definition(${PROJECT_NAME} TARGET_DIRECTORY "../HardwareDefinitions/seeed_mt3620_mdb" TARGET_DEFINITION "azure_sphere_learning_path.json")
    message(STATUS "Azure Sphere board selected: SEEED STUDIO MINI")
endif(SEEED_STUDIO_MINI)

if(BOARD_COUNTER EQUAL 0)
    message(FATAL_ERROR "No Azure Sphere boards selected. Ensure one board set")
endif()

if(BOARD_COUNTER GREATER 1)
    message(FATAL_ERROR "Multiple (${BOARD_COUNTER}) Azure Sphere boards selected. Ensure only one board set")
endif()

azsphere_target_add_image_package(${PROJECT_NAME})