sc stop PVNWGTunnel$PVN
sc delete PVNWGTunnel$PVN
sc stop AmneziaWGTunnel$AmneziaVPN
sc delete AmneziaWGTunnel$AmneziaVPN
taskkill /IM "PVN-service.exe" /F
taskkill /IM "PVN.exe" /F
taskkill /IM "AmneziaVPN-service.exe" /F
taskkill /IM "AmneziaVPN.exe" /F
exit /b 0
