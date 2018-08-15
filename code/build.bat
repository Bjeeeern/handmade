@echo off

SET "source=%~dp0"
SET "builddir=%source%..\build"
SET "platformdir=%source%\platform"

SET "filename=handmade"

CALL "%platformdir%\build.bat"

if not exist "%builddir%" mkdir "%builddir%"
pushd "%builddir%"

del *.pdb >nul 2>nul

REM 64-bit build

SET "gamesource=%source%\%filename%.cpp"

cl %defines% %errors% %flags% -I%platformdir% -Fm%filename%.map "%gamesource%" -LD -link %linkflags% -EXPORT:GameGetSoundSamples -EXPORT:GameUpdateAndRender -PDB:%filename%_%random%.pdb -OUT:%filename%.dll

@xcopy ..\temp . /Y >nul 2>&1

popd
