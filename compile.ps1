$baseDir = "$(Split-Path -Parent $MyInvocation.MyCommand.Path)"
$precompiledDir = "$baseDir\precompiled"
$exePath = ".\build\app.exe"

# LIBRARY FOLDERS - GLEW GLFW & IMGUI
$glewInclude = "$baseDir\lib\glew\include"
$glewLib = "$baseDir\lib\glew\lib\x64"
$glfwInclude = "$baseDir\lib\glfw\include"
$glfwLib = "$baseDir\lib\glfw\lib"
$imguiInclude = "$baseDir\lib\imgui"  
$imguiSource = "$baseDir\lib\imgui" 

# COMPILE SOURCE FILE 
& clang++ -std=c++17 -fopenmp -c "src\main.cpp" -I"$glewInclude" -I"$glfwInclude" -I"$imguiInclude" -o "main.o"

# GET PRECOMPILED OBJECT FILES
$imguiObjectFiles = Get-ChildItem -Path $precompiledDir -Filter "*.o" | ForEach-Object { $_.FullName }

# LINK AND COMPILE TO AN EXECUTIBLE
& clang++ -o $exePath "main.o" $imguiObjectFiles -L"$glewLib" -L"$glfwLib" -L"$baseDir\lib\imgui" `
-lglew32 -lglfw3 -lopengl32 -lgdi32 -luser32 -lshell32 -fopenmp 




# COPY SHADERS TO BUILD 
Remove-Item "$baseDir\build\shaders" -Recurse -Force -ErrorAction SilentlyContinue
Copy-Item "$baseDir\shaders" -Destination "$baseDir\build\shaders" -Recurse

# REMOVE OBJECT FILE
Remove-Item "*.o" -Force

Write-Output "Compiled Successfully!"
