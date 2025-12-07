# Script to setup dependencies in libs/ folder

$ErrorActionPreference = "Stop"

$libsDir = Join-Path $PSScriptRoot "libs"

if (-not (Test-Path $libsDir)) {
    New-Item -ItemType Directory -Path $libsDir | Out-Null
    Write-Host "Created libs directory."
}

function Check-File($path) {
    if (-not (Test-Path $path)) {
        throw "File not found: $path"
    }
    $item = Get-Item $path
    if ($item.Length -eq 0) {
        throw "File is empty (download failed?): $path"
    }
}

# --- GLFW ---
$glfwUrl = "https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.bin.WIN64.zip"
$glfwZip = Join-Path $libsDir "glfw.zip"
$glfwExtractDir = Join-Path $libsDir "glfw_temp"
$glfwFinalDir = Join-Path $libsDir "glfw"

if (-not (Test-Path $glfwFinalDir)) {
    Write-Host "Downloading GLFW..."
    Invoke-WebRequest -Uri $glfwUrl -OutFile $glfwZip
    Check-File $glfwZip

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
    $gladH = Join-Path $gladHeaderDir "glad.h"
    Invoke-WebRequest -Uri "https://raw.githubusercontent.com/JoeyDeVries/LearnOpenGL/master/includes/glad/glad.h" -OutFile $gladH
    Check-File $gladH

    # khrplatform.h
    $khrH = Join-Path $khrHeaderDir "khrplatform.h"
    Invoke-WebRequest -Uri "https://raw.githubusercontent.com/JoeyDeVries/LearnOpenGL/master/includes/KHR/khrplatform.h" -OutFile $khrH
    Check-File $khrH

    # glad.c
    $gladC = Join-Path $gladSrcDir "glad.c"
    Invoke-WebRequest -Uri "https://raw.githubusercontent.com/JoeyDeVries/LearnOpenGL/master/src/glad.c" -OutFile $gladC
    Check-File $gladC

    Write-Host "GLAD setup complete."
} else {
    Write-Host "GLAD already exists. Skipping."
}

# --- GLM ---
$glmDir = Join-Path $libsDir "glm"
$glmZip = Join-Path $libsDir "glm.zip"

if (-not (Test-Path $glmDir)) {
    Write-Host "Downloading GLM..."
    # Using 0.9.9.8 as a stable version
    Invoke-WebRequest -Uri "https://github.com/g-truc/glm/releases/download/0.9.9.8/glm-0.9.9.8.zip" -OutFile $glmZip
    Check-File $glmZip

    Write-Host "Extracting GLM..."
    # Extract to a temp folder first to handle the nested structure
    $glmTemp = Join-Path $libsDir "glm_temp"
    Expand-Archive -Path $glmZip -DestinationPath $glmTemp

    # The zip contains a 'glm' folder which contains the 'glm' source folder.
    # We want libs/glm/glm to exist so we can include <glm/glm.hpp>

    # Move the inner 'glm' folder to libs/
    # Structure in zip: glm/glm/...
    $innerGlm = Join-Path $glmTemp "glm"

    if (Test-Path $innerGlm) {
        Move-Item -Path $innerGlm -Destination $libsDir
    } else {
        # Fallback if structure is different, just rename temp
        Rename-Item -Path $glmTemp -NewName "glm"
    }

    # Cleanup
    if (Test-Path $glmTemp) { Remove-Item -Recurse -Force $glmTemp }
    Remove-Item $glmZip

    Write-Host "GLM setup complete."
} else {
    Write-Host "GLM already exists. Skipping."
}

# --- STB ---
$stbDir = Join-Path $libsDir "stb"

if (-not (Test-Path $stbDir)) {
    New-Item -ItemType Directory -Path $stbDir | Out-Null
    Write-Host "Downloading STB..."

    $stbTT = Join-Path $stbDir "stb_truetype.h"
    Invoke-WebRequest -Uri "https://raw.githubusercontent.com/nothings/stb/master/stb_truetype.h" -OutFile $stbTT
    Check-File $stbTT

    $stbImg = Join-Path $stbDir "stb_image.h"
    Invoke-WebRequest -Uri "https://raw.githubusercontent.com/nothings/stb/master/stb_image.h" -OutFile $stbImg
    Check-File $stbImg

    Write-Host "STB setup complete."
} else {
    Write-Host "STB already exists. Skipping."
}

Write-Host "Dependencies setup successfully."
