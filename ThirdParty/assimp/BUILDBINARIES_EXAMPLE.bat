:: This is an example file to generate binaries using Windows Operating System
:: This script is configured to be executed from the source directory

:: Compiled binaries will be placed in BINARIES_DIR\code\CONFIG

:: NOTE
:: The build process will generate a config.h file that is placed in BINARIES_DIR\include
:: This file must be merged with SOURCE_DIR\include
:: You should write yourself a script that copies the files where you want them.
:: Also see: https://github.com/assimp/assimp/pull/2646

SET SOURCE_DIR=.
SET GENERATOR=Visual Studio 17 2022

::SET BINARIES_DIR="./build/Win32"
::cmake . -G "%GENERATOR%" -A Win32 -S %SOURCE_DIR% -B %BINARIES_DIR%
::cmake --build %BINARIES_DIR% --config debug
::cmake --build %BINARIES_DIR% --config release

SET BINARIES_DIR="./build/x64"
cmake . -G "%GENERATOR%" -A x64 -S %SOURCE_DIR% -B %BINARIES_DIR%
cmake --build %BINARIES_DIR% --config debug
cmake --build %BINARIES_DIR% --config release

:: Copy built binaries to ThirdParty\BuildBin\<Config>
:: (BuildBin is a sibling folder of this assimp folder.)
set "_SCRIPT_DIR=%~dp0"
set "_TP_DIR=%_SCRIPT_DIR%.."
set "_DEST_ROOT=%_TP_DIR%\BuildBin"
set "_DEST_DEBUG=%_DEST_ROOT%\Debug"
set "_DEST_RELEASE=%_DEST_ROOT%\Release"

if not exist "%_DEST_DEBUG%" mkdir "%_DEST_DEBUG%" >NUL
if not exist "%_DEST_RELEASE%" mkdir "%_DEST_RELEASE%" >NUL

:: Copy libs
if exist %BINARIES_DIR%\lib\Debug\assimp*.lib copy /Y %BINARIES_DIR%\lib\Debug\assimp*.lib "%_DEST_DEBUG%\" >NUL
if exist %BINARIES_DIR%\lib\Release\assimp*.lib copy /Y %BINARIES_DIR%\lib\Release\assimp*.lib "%_DEST_RELEASE%\" >NUL

:: Copy DLLs (if produced)
if exist %BINARIES_DIR%\bin\Debug\assimp*.dll copy /Y %BINARIES_DIR%\bin\Debug\assimp*.dll "%_DEST_DEBUG%\" >NUL
if exist %BINARIES_DIR%\bin\Release\assimp*.dll copy /Y %BINARIES_DIR%\bin\Release\assimp*.dll "%_DEST_RELEASE%\" >NUL

:: Copy PDBs (optional)
if exist %BINARIES_DIR%\bin\Debug\assimp*.pdb copy /Y %BINARIES_DIR%\bin\Debug\assimp*.pdb "%_DEST_DEBUG%\" >NUL
if exist %BINARIES_DIR%\bin\Release\assimp*.pdb copy /Y %BINARIES_DIR%\bin\Release\assimp*.pdb "%_DEST_RELEASE%\" >NUL

PAUSE
