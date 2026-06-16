sc stop PVNWGTunnel$PVN
sc delete PVNWGTunnel$PVN
sc stop AmneziaWGTunnel$PVN
sc delete AmneziaWGTunnel$PVN
taskkill /IM "PVN-service.exe" /F
taskkill /IM "PVN.exe" /F
taskkill /IM "PVN-service.exe" /F
taskkill /IM "PVN.exe" /F
exit /b 0

