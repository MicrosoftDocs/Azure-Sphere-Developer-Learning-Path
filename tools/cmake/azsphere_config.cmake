function(auto_generate_tools_revision)
    # Generate the Azure Sphere tools version from the azsphere show-version SDK tool
    execute_process(COMMAND azsphere show-version OUTPUT_VARIABLE SDK_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
    
    if(SDK_VERSION STREQUAL "")
        message(FATAL_ERROR "ERROR: azsphere cli not found. Have you installed the Azure Sphere SDK with CLI Version 2?")
    endif()
    
    message(STATUS "Azure Sphere SDK Version: ${SDK_VERSION}")
    azsphere_configure_tools(TOOLS_REVISION ${SDK_VERSION})
endfunction()

# Auto generate azsphere_configure_tools and azsphere_configure_api
function(auto_generate_azsphere_config)

    auto_generate_tools_revision()

    # Generate the Azure Sphere latest sysroot number
    if (EXISTS "/opt/azurespheresdk/Sysroots/")
        FILE(GLOB children "/opt/azurespheresdk/Sysroots/*")
    else()        
        FILE(GLOB children "C:/Program Files (x86)/Microsoft Azure Sphere SDK/Sysroots/*")
    endif()

    list(LENGTH children len)

    if(len EQUAL 0)
        message(FATAL_ERROR "No Sysroots found in the default location. Check you have installed the Azure Sphere SDK")
    endif()

    set(latest_api_version 0)
    
    foreach(child ${children})
        string(REPLACE "/" ";" child ${child})

        # grab the api version number - it's the last element
        list(LENGTH child len)
        MATH(EXPR len "${len}-1")
        list(GET child ${len} api_version)

        # Is new api version numeric and greater than latest api version then update latest api version with new version
        if (api_version MATCHES "^[0-9]+$" AND api_version GREATER latest_api_version)
            set(latest_api_version ${api_version})
        endif()
    endforeach()

    message(STATUS "Azure Sphere latest sysroot: ${latest_api_version}")
    azsphere_configure_api(TARGET_API_SET ${latest_api_version})    

endfunction()