@echo off

SET "source=%~dp0"
SET "builddir=%source%..\build"
SET "platformdir=%source%\platform"

SET "filename=handmade"

CALL "%platformdir%\build.bat"

if not exist "%builddir%" mkdir "%builddir%"
pushd "%builddir%"

REM 64-bit build

SET "gamesource=%source%\%filename%.cpp"

cl %defines% %errors% %flags% -I%platformdir% -Fm "%gamesource%" -LD -link %linkflags% handmade_intrinsics.obj -EXPORT:GameGetSoundSamples -EXPORT:GameUpdateAndRender -PDB:game_%random%.pdb -OUT:game.dll

@xcopy ..\temp . /Y >nul 2>&1

popd
