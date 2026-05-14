@echo off
rem Convert orb BMP files to TIG .art format and copy to game data directory.
rem
rem Prerequisites:
rem   pip install Pillow
rem
rem Run this script from the arcanum-ce\ project root:
rem   tools\convert_orb_art.bat  <game_data_dir>
rem
rem Example:
rem   tools\convert_orb_art.bat  "C:\Arcanum\data\art\item"

set ARTS_DIR=%1
if "%ARTS_DIR%"=="" (
    echo Usage: convert_orb_art.bat ^<game_data_art_item_dir^>
    echo Example: convert_orb_art.bat "C:\Arcanum\data\art\item"
    exit /b 1
)

set BMPS=src\game\Assets_main
set TOOL=tools\bmp_to_art.py

if not exist "%ARTS_DIR%" mkdir "%ARTS_DIR%"

python %TOOL% %BMPS%\reforging.bmp        "%ARTS_DIR%\orb1_inven.art"
python %TOOL% %BMPS%\reforging_ground.bmp "%ARTS_DIR%\orb1_ground.art"
python %TOOL% %BMPS%\ascension.bmp        "%ARTS_DIR%\orb2_inven.art"
python %TOOL% %BMPS%\ascension_ground.bmp "%ARTS_DIR%\orb2_ground.art"
python %TOOL% %BMPS%\cleansing.bmp        "%ARTS_DIR%\orb3_inven.art"
python %TOOL% %BMPS%\cleansing_ground.bmp "%ARTS_DIR%\orb3_ground.art"
python %TOOL% %BMPS%\annulment.bmp        "%ARTS_DIR%\orb4_inven.art"
python %TOOL% %BMPS%\annulment_ground.bmp "%ARTS_DIR%\orb4_ground.art"
python %TOOL% %BMPS%\awakening.bmp        "%ARTS_DIR%\orb5_inven.art"
python %TOOL% %BMPS%\awakening_ground.bmp "%ARTS_DIR%\orb5_ground.art"

echo Done. 10 art files written to %ARTS_DIR%
