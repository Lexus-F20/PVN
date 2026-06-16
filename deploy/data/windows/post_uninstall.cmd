set PVNPath=%~dp0
echo %PVNPath%

rem Define directories for logs
set "ORG_DIR=%AppData%\PVN"
set "USER_APP_DIR=%ORG_DIR%\PVN"
set "USER_LOG_DIR=%USER_APP_DIR%\log"
set "SYS_APP_DIR=%ProgramData%\PVN"
set "SYS_LOG_DIR=%SYS_APP_DIR%\log"
set "SYS_LOG_FILE=%SYS_LOG_DIR%\PVN-service.log"

timeout /t 1
sc stop PVN-service
sc delete PVN-service
sc stop PVN-service
sc delete PVN-service
sc stop PVNWGTunnel$PVN
sc delete PVNWGTunnel$PVN
sc stop AmneziaWGTunnel$PVN
sc delete AmneziaWGTunnel$PVN
taskkill /IM "PVN-service.exe" /F
taskkill /IM "PVN.exe" /F
taskkill /IM "PVN-service.exe" /F
taskkill /IM "PVN.exe" /F

rem Delete the service log file under ProgramData
if exist "%SYS_LOG_FILE%" del /F /Q "%SYS_LOG_FILE%"
if exist "%SYS_LOG_DIR%" rmdir /S /Q "%SYS_LOG_DIR%"
rem Try to remove application dir if empty
rd "%SYS_APP_DIR%" 2>nul

rem Delete client logs under current user's AppData\Roaming (Organization\Application)
if exist "%USER_LOG_DIR%" rmdir /S /Q "%USER_LOG_DIR%"
rem Try to remove app and org directories if empty
rd "%USER_APP_DIR%" 2>nul
rd "%ORG_DIR%" 2>nul

exit /b 0

