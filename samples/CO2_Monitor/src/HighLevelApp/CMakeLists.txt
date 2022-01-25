#  Copyright (c) Microsoft Corporation. All rights reserved.
#  Licensed under the MIT License.

###################################################################################################################

# Select your developer board by removing the # tag from the beginning of the line

# The default board selected is the AVNET Starter Kit.

# If you are NOT using the AVNET board be sure to comment out the AVNET board

# set(AVNET TRUE "AVNET Azure Sphere Starter Kit Revision 1 ")    
# set(AVNET_REV_2 TRUE "AVNET Azure Sphere Starter Kit Revision 2 ")             
# set(SEEED_STUDIO_RDB TRUE "Seeed Studio Azure Sphere MT3620 Development Kit (aka Reference Design Board or rdb)")
set(SEEED_STUDIO_MINI TRUE "Seeed Studio Azure Sphere MT3620 Mini Dev Board")

###################################################################################################################

cmake_minimum_required (VERSION 3.10)
project (AzureSphereIoTCentral C)

azsphere_configure_tools(TOOLS_REVISION "21.01")
azsphere_configure_api(TARGET_API_SET "11")


add_subdirectory("AzureSphereDevX" out)
add_subdirectory("./embedded-scd" scd30)

set(Source
    "buzzer_alert.c"
    "co2_manager.c"
    "connected_status.c"
    "httpGet.c"
    "hvac_status.c"
    "location_from_ip.c"
    "main.c"
)
source_group("Source" FILES ${Source})

set(ALL_FILES
    ${Source}
)

# Create executable
add_executable(${PROJECT_NAME} ${ALL_FILES})
target_compile_definitions(${PROJECT_NAME} PUBLIC AZURE_IOT_HUB_CONFIGURED)
target_link_libraries(${PROJECT_NAME} applibs pthread gcc_s c azure_sphere_devx scd30_lib curl)

target_include_directories(${PROJECT_NAME} PUBLIC AzureSphereDevX/include )


if(AVNET)
    add_definitions( -DOEM_AVNET=TRUE )
    azsphere_target_hardware_definition(${PROJECT_NAME} TARGET_DIRECTORY "Hardware/avnet_mt3620_sk" TARGET_DEFINITION "azure_sphere_learning_path.json")
endif(AVNET)

if(AVNET_REV_2)
    add_definitions( -DOEM_AVNET=TRUE )
    azsphere_target_hardware_definition(${PROJECT_NAME} TARGET_DIRECTORY "Hardware/avnet_mt3620_sk_rev2" TARGET_DEFINITION "azure_sphere_learning_path.json")
endif(AVNET_REV_2)

if (SEEED_STUDIO_RDB)
    add_definitions( -DOEM_SEEED_STUDIO_RDB=TRUE )
    azsphere_target_hardware_definition(${PROJECT_NAME} TARGET_DIRECTORY "Hardware/mt3620_rdb" TARGET_DEFINITION "azure_sphere_learning_path.json")
endif()

if (SEEED_STUDIO_MINI)
    add_definitions( -DOEM_SEEED_STUDIO_MINI=TRUE )
    azsphere_target_hardware_definition(${PROJECT_NAME} TARGET_DIRECTORY "Hardware/seeed_mt3620_mdb" TARGET_DEFINITION "azure_sphere_learning_path.json")
endif(SEEED_STUDIO_MINI)

azsphere_target_add_image_package(${PROJECT_NAME})