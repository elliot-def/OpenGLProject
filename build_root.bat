@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
set INC=D:\Documents\GitHub\OpenGLProject\OpenGLProject\dependencies
cl /nologo /O2 /MT /EHsc /std:c++20 /I "%INC%" sim\dump_root.cpp /Fe:dump_root.exe /link "%INC%\lib\assimp-vc143-mt.lib"
