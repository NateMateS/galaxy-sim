# Script to setup dependencies in libs/ folder

$ErrorActionPreference = "Stop"

$libsDir = Join-Path $PSScriptRoot "libs"

if (-not (Test-Path $libsDir)) {
    New-Item -ItemType Directory -Path $libsDir | Out-Null
    Write-Host "Created libs directory."
}

# --- GLFW ---
$glfwUrl = "https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.bin.WIN64.zip"
$glfwZip = Join-Path $libsDir "glfw.zip"
$glfwExtractDir = Join-Path $libsDir "glfw_temp"
$glfwFinalDir = Join-Path $libsDir "glfw"

if (-not (Test-Path $glfwFinalDir)) {
    Write-Host "Downloading GLFW..."
    Invoke-WebRequest -Uri $glfwUrl -OutFile $glfwZip

    Write-Host "Extracting GLFW..."
    Expand-Archive -Path $glfwZip -DestinationPath $libsDir

    # The zip contains a folder named "glfw-3.4.bin.WIN64"
    $extractedFolder = Join-Path $libsDir "glfw-3.4.bin.WIN64"
    if (Test-Path $extractedFolder) {
        Rename-Item -Path $extractedFolder -NewName "glfw"
    }

    Remove-Item $glfwZip
    Write-Host "GLFW setup complete."
} else {
    Write-Host "GLFW already exists. Skipping."
}

# --- GLAD ---
$gladDir = Join-Path $libsDir "glad"
$gladIncDir = Join-Path $gladDir "include"
$gladSrcDir = Join-Path $gladDir "src"
$gladHeaderDir = Join-Path $gladIncDir "glad"
$khrHeaderDir = Join-Path $gladIncDir "KHR"

if (-not (Test-Path $gladDir)) {
    New-Item -ItemType Directory -Path $gladDir | Out-Null
    New-Item -ItemType Directory -Path $gladIncDir | Out-Null
    New-Item -ItemType Directory -Path $gladSrcDir | Out-Null
    New-Item -ItemType Directory -Path $gladHeaderDir | Out-Null
    New-Item -ItemType Directory -Path $khrHeaderDir | Out-Null

    Write-Host "Downloading GLAD files..."

    # glad.h
    Invoke-WebRequest -Uri "https://raw.githubusercontent.com/JoeyDeVries/LearnOpenGL/master/includes/glad/glad.h" -OutFile (Join-Path $gladHeaderDir "glad.h")

    # khrplatform.h
    Invoke-WebRequest -Uri "https://raw.githubusercontent.com/JoeyDeVries/LearnOpenGL/master/includes/KHR/khrplatform.h" -OutFile (Join-Path $khrHeaderDir "khrplatform.h")

    # glad.c
    Invoke-WebRequest -Uri "https://raw.githubusercontent.com/JoeyDeVries/LearnOpenGL/master/src/glad.c" -OutFile (Join-Path $gladSrcDir "glad.c")

    Write-Host "GLAD setup complete."
} else {
    Write-Host "GLAD already exists. Skipping."
}

Write-Host "Dependencies setup successfully."
