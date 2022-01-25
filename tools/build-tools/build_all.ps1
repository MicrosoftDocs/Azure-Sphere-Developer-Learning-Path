# Project discovery assumes there is a cmake/azsphere_config.cmake folder/file for the high-level and real-time projects to be built 

Write-Output "`n`nBuild all test tool for AzureSphereDevX examples`n`n"

if ($IsWindows) {
    $files = Get-ChildItem "C:\Program Files (x86)" -Recurse -Filter arm-none-eabi-gcc-*.exe -ErrorAction SilentlyContinue | Sort-Object
    if ($files.count -gt 0){
        $gnupath = $files[0].Directory.FullName
    }
    else {
        Write-Output("Error: GNU Arm Embedded Toolchain not found in the C:\Program Files (x86) folder")
        Write-Output("Browse: https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm/downloads")
        Write-Output("Download: gcc-arm-none-eabi-<version>-win32.exe")
        Write-Output("Install: install the gcc-arm-none-eabi-<version>-win32.exe, accepting the defaults.")
        exit 1
    }
}
else {
    if ($IsLinux) {
        $gnufolder = Get-ChildItem /opt/gcc-arm-none* -Directory
        if ($gnufolder.count -eq 1) {
            $gnupath = Join-Path -Path $gnufolder -ChildPath "bin"
        }
        else {
            Write-Output("Error: GNU Arm Embedded Toolchain not found in the /opt folder")
            Write-Output("Browse: https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm/downloads")
            Write-Output("Download: gcc-arm-none-eabi-<version>-x86_64-linux.tar.bz2")
            Write-Output("Install: sudo tar -xjvf gcc-arm-none-eabi-<version>-x86_64-linux.tar.bz2 -C /opt")
            Write-Output("Path: nano ~/.bashrc, at end of file add export PATH=`$PATH:/opt/gcc-arm-none-eabi-<version>/bin")
            exit 1            
        }
    }
}

$exit_code = 0

function build-high-level {
    param (
        $dir
    )

    New-Item -Name "./build" -ItemType "directory" -ErrorAction SilentlyContinue
    Set-Location "./build"

    if ($IsWindows) {
        cmake `
            -G "Ninja" `
            -DCMAKE_TOOLCHAIN_FILE="C:\Program Files (x86)\Microsoft Azure Sphere SDK\CMakeFiles\AzureSphereToolchain.cmake" `
            -DAZURE_SPHERE_TARGET_API_SET="latest-lts" `
            -DCMAKE_BUILD_TYPE="Release" `
            $dir

        cmake --build .
    }
    else {
        if ($IsLinux) {
            cmake `
                -G "Ninja" `
                -DCMAKE_TOOLCHAIN_FILE="/opt/azurespheresdk/CMakeFiles/AzureSphereToolchain.cmake" `
                -DAZURE_SPHERE_TARGET_API_SET="latest-lts" `
                -DCMAKE_BUILD_TYPE="Release" `
                $dir

            ninja
        }
        else {
            Write-Output "`nERROR: Tool supported on Windows and Linux.`n"
        }
    }
}

function build-real-time {
    param (
        $dir
    )

    New-Item -Name "./build" -ItemType "directory" -ErrorAction SilentlyContinue
    Set-Location "./build"

    if ($IsWindows) {
        cmake `
            -G "Ninja" `
            -DCMAKE_TOOLCHAIN_FILE="C:\Program Files (x86)\Microsoft Azure Sphere SDK\CMakeFiles\AzureSphereRTCoreToolchain.cmake" `
            -DARM_GNU_PATH:STRING=$gnupath `
            -DCMAKE_BUILD_TYPE="Release" `
            $dir

        cmake --build .
    }
    else {
        if ($IsLinux) {
            cmake `
                -G "Ninja" `
                -DCMAKE_TOOLCHAIN_FILE="/opt/azurespheresdk/CMakeFiles/AzureSphereRTCoreToolchain.cmake" `
                -DARM_GNU_PATH:STRING=$gnupath  `
                -DCMAKE_BUILD_TYPE="Release" `
                $dir

            ninja
        }
        else {
            Write-Output "`nERROR: Tool supported on Windows and Linux.`n"
        }
    }
}

function build_application {

    param (
        $dir
    )

    $manifest = Join-Path -Path $dir -ChildPath "app_manifest.json"
    $cmakefile = Join-Path -Path $dir -ChildPath "CMakeLists.txt"

    if (Test-Path -path $cmakefile) {

        # Only build executable apps not libraries
        if (Select-String -Path $cmakefile -Pattern 'add_executable' -Quiet ) {

            # Is this a high-level app?
            if (Select-String -Path $manifest -Pattern 'Default' -Quiet) {

                build-high-level $dir
            }
            else {
                # Is this a real-time app?
                if (Select-String -Path $manifest -Pattern 'RealTimeCapable' -Quiet) {

                    build-real-time $dir
                }
                else {
                    # Not a high-level or real-time app so just continue
                    continue
                }
            }

            # was the imagepackage created - this is a proxy for sucessful build
            $imagefile = Get-ChildItem "*.imagepackage" -Recurse
            if ($imagefile.count -ne 0) {
                Write-Output "`nSuccessful build: $dir.`n"
            }
            else {
                Set-Location ".."
                Write-Output "`nBuild failed: $dir.`n"

                $script:exit_code = 1

                break
            }

            Set-Location ".."
            Remove-Item -Recurse -Force -ErrorAction SilentlyContinue -path ./build
        }
    }
}

$StartTime = $(get-date)

Remove-Item -Recurse -Force -ErrorAction SilentlyContinue -path ./build

# Project discovery assumes there is a cmake/azsphere_config.cmake folder/file for the high-level and real-time projects to be built 
$files = Get-ChildItem -Recurse -Filter azsphere_board.txt | Split-Path -Parent | Sort-Object

# Write-Output "Building $files.count projects"
foreach ($file in $files) {
    Write-Output ( -join ("BUILDING:", $file))
}

Write-Output "`n`nBuild all process starting.`n"


foreach ($file in $files) {
    Write-Output ( -join ("BUILDING:", $file, "`n"))
    build_application $file
}

Remove-Item -Recurse -Force -ErrorAction SilentlyContinue -path ./build

$elapsedTime = $(get-date) - $StartTime
$totalTime = "{0:HH:mm:ss}" -f ([datetime]$elapsedTime.Ticks)

if ($exit_code -eq 0) {
    Write-Output "Build All completed successfully. Elapsed time: $totalTime"
}
else {
    Write-Output "Build All failed. Elapsed time: $totalTime"
}

exit $exit_code
