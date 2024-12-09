$baseDir = "$(Split-Path -Parent $MyInvocation.MyCommand.Path)"
$precompiledDir = "$baseDir\precompiled"

$glewInclude = "$baseDir\lib\glew\include"
$glfwInclude = "$baseDir\lib\glfw\include"
$imguiInclude = "$baseDir\lib\imgui"  
$imguiSource = "$baseDir\lib\imgui" 

& clang++ -std=c++17 -c "$imguiSource\imgui.cpp" -I"$imguiInclude" -I"$glfwInclude" -o "$precompiledDir\imgui.o"
& clang++ -std=c++17 -c "$imguiSource\imgui_draw.cpp" -I"$imguiInclude" -I"$glfwInclude" -o "$precompiledDir\imgui_draw.o"
& clang++ -std=c++17 -c "$imguiSource\imgui_widgets.cpp" -I"$imguiInclude" -I"$glfwInclude" -o "$precompiledDir\imgui_widgets.o"
& clang++ -std=c++17 -c "$imguiSource\imgui_tables.cpp" -I"$imguiInclude" -I"$glfwInclude" -o "$precompiledDir\imgui_tables.o"
& clang++ -std=c++17 -c "$imguiSource\backends\imgui_impl_glfw.cpp" -I"$imguiInclude" -I"$glfwInclude" -o "$precompiledDir\imgui_impl_glfw.o"
& clang++ -std=c++17 -c "$imguiSource\backends\imgui_impl_opengl3.cpp" -I"$imguiInclude" -I"$glfwInclude" -o "$precompiledDir\imgui_impl_opengl3.o"