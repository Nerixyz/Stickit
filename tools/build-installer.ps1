if (-not (Test-Path -PathType Container bin)) {
    Write-Error "Couldn't find a folder called 'bin' in the current directory.";
    exit 1
}

# Build the installer
ISCC /DWORKING_DIR="$($pwd.Path)\" /O. "$PSScriptRoot\installer.iss";
