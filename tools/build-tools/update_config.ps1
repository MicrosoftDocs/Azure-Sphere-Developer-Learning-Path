Write-Output "`n`nAzure Sphere CMake Build Config update tool starting."

$exit_code = 0
$target_api_version = $null
$tools_version = $null

function update_config {
    param (
        $file,
        $target_api_version,
        $tools_version
    )

    Set-Content -Path $file -Value ( -join ('azsphere_configure_tools(TOOLS_REVISION "', $tools_version, '")'))
    Add-Content -Path $file -Value ( -join ('azsphere_configure_api(TARGET_API_SET "', $target_api_version, '")')) -NoNewline
}

function get_latest_sysroot {

    if ($IsWindows) {
        # get sysroot folders and sort descending by int name
        $sysroots = get-childitem "C:\Program Files (x86)\Microsoft Azure Sphere SDK\Sysroots"  | Sort-Object -Descending -Property { $_.Name -as [int] }
    }
    else {
        if ($IsLinux) {
            $sysroots = get-childitem "/opt/azurespheresdk/Sysroots"  | Sort-Object -Descending -Property { $_.Name -as [int] }
        }
        else {
            Write-Output "`nERROR: Tool supported on Windows and Linux.`n"
            return $null
        }
    }

    if ($sysroots.Count -gt 1) {
        return  $sysroots[0].Name
    }
    else {
        return $null
    }
}

function get_sdk_version {

    $azure_sphere_sdk_version = azsphere show-version -o tsv

    $version_parts = $azure_sphere_sdk_version.split('.')

    if ($version_parts.count -gt 1) {
        $tools_version = $version_parts[0] + '.' + $version_parts[1]
    }
    else {
        $tools_version = $null
    }

    $rtn = ""

    if ([float]::TryParse($tools_version, [ref]$rtn)) {
        return $tools_version
    }
    else {
        Write-Output "Expected Azure Sphere SDK version to be a floating point number."
        Write-Output (-join("The azsphere show-version -o tsv command returned: ", $azure_sphere_sdk_version))
        return $null
    }
}

$tools_version = get_sdk_version
$target_api_version = get_latest_sysroot

if ([string]::IsNullOrEmpty($tools_version)) {
    Write-Output "`nERROR: Azure Sphere CLI did not return SDK version. Check Azure Sphere CLI installed correctly.`n"
    exit 1
}

if ([string]::IsNullOrEmpty($target_api_version)) {
    Write-Output "`nERROR: Expected sysroots not found. Check Azure Sphere SDK installed correctly.`n"
    exit 1
}

$files = Get-ChildItem -Recurse -Include azsphere_config.cmake -name
foreach ($file in $files) {
    update_config $file $target_api_version $tools_version
}

Write-Output "Azure Sphere CMake Build Config update tool completed.`n`n"
exit $exit_code