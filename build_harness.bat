@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
set INC=D:\Documents\GitHub\OpenGLProject\OpenGLProject\dependencies
cl /nologo /O2 /MT /EHsc /std:c++20 /I "D:\Documents\GitHub\OpenGLProject\sim" /I "D:\Documents\GitHub\OpenGLProject\OpenGLProject" /I "%INC%" sim\harness_strafe.cpp sim\Animator.cpp /Fe:harness_strafe.exe /link "%INC%\lib\assimp-vc143-mt.lib"
