Write-Output "Submodule AzureSphereDevX and HardwareDefinitions into all high-level example apps`n`n"

$dirlist = Get-ChildItem -Recurse -Directory -Name -Depth 2

# Remove existing submoduled directories
foreach ($dir in $dirList) {

    $devxPath = Join-Path -Path $dir -ChildPath "AzureSphereDevX"
    $hwdPath = Join-Path -Path $dir -ChildPath "HardwareDefinitions"

    Remove-Item -Recurse -Force -ErrorAction SilentlyContinue -path $devxPath
    Remove-Item -Recurse -Force -ErrorAction SilentlyContinue -path $hwdPath
    
}

Remove-Item -Recurse -Force -ErrorAction SilentlyContinue -path intercore_example/RealTimeAppOne/mt3620_m4_software
Remove-Item -Recurse -Force -ErrorAction SilentlyContinue -path intercore_example/RealTimeAppTwo/mt3620_m4_software
Remove-Item -Recurse -Force -ErrorAction SilentlyContinue -path little_fs_on_mutable_storage/littlefs

git add .
git commit -m "Initial check in"

# Submodule AzureSphereDevX and HardwareDefinitions to each high-level application
foreach ($dir in $dirList) {

    $manifest = Join-Path -Path $dir -ChildPath "app_manifest.json"
    $cmakefile = Join-Path -Path $dir -ChildPath "CMakeLists.txt"

    if ((Test-Path -path $manifest) -and (Test-Path -path $cmakefile) -and (Select-String -Path $manifest -Pattern 'Default' -Quiet) ) {
        Write-Output $dir

        $devxpath = "$dir/AzureSphereDevX" -replace "\\", "/"
        $hwdpath = "$dir/HardwareDefinitions" -replace "\\", "/"

        git submodule add https://github.com/Azure-Sphere-DevX/AzureSphereDevX.git $devxpath
        git submodule add https://github.com/Azure-Sphere-DevX/AzureSphereDevX.HardwareDefinitions.git $hwdpath
    }
}

git submodule add https://github.com/littlefs-project/littlefs.git "little_fs_on_mutable_storage/littlefs"
git submodule add https://github.com/MediaTek-Labs/mt3620_m4_software.git "intercore_example/RealTimeAppOne/mt3620_m4_software"
git submodule add https://github.com/MediaTek-Labs/mt3620_m4_software.git "intercore_example/RealTimeAppTwo/mt3620_m4_software"
