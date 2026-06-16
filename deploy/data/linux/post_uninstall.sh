#!/bin/bash

APP_NAME=PVN
LEGACY_NAME=AmneziaVPN
ORG_NAME=PVN
LEGACY_ORG_NAME=AmneziaVPN.ORG
LOG_FOLDER=/var/log/$APP_NAME
LOG_FILE="$LOG_FOLDER/post-uninstall.log"
APP_PATH=/opt/$APP_NAME
LEGACY_APP_PATH=/opt/$LEGACY_NAME

if ! test -f $LOG_FILE; then
	mkdir -p "$LOG_FOLDER" 2>/dev/null
	touch $LOG_FILE
fi

date >> $LOG_FILE
echo "Uninstall Script started" >> $LOG_FILE
sudo killall -9 $APP_NAME 2>> $LOG_FILE
sudo killall -9 $LEGACY_NAME 2>> $LOG_FILE

if command -v steamos-readonly &> /dev/null; then
	sudo steamos-readonly disable >> $LOG_FILE
	echo "steamos-readonly disabled" >> $LOG_FILE
fi

# Stop & disable systemd units (both names — old upgrades may have left it)
for name in $APP_NAME $LEGACY_NAME; do
	if sudo systemctl is-active --quiet $name; then
		sudo systemctl stop $name >> $LOG_FILE
	fi
	if sudo systemctl is-enabled --quiet $name; then
		sudo systemctl disable $name >> $LOG_FILE
	fi
	if test -f /etc/systemd/system/$name.service; then
		sudo rm -rf /etc/systemd/system/$name.service >> $LOG_FILE
	fi
done

# Unlink lib symlinks
for path in $APP_PATH $LEGACY_APP_PATH; do
	if [ -d "$path/client/lib" ]; then
		ls "$path/client/lib/"* 2>/dev/null | while IFS=: read -r dir; do
			sudo unlink "$dir" >> $LOG_FILE 2>&1 || true
		done
	fi
done

# Remove install dirs
for path in $APP_PATH $LEGACY_APP_PATH; do
	if test -d $path; then
		sudo rm -rf $path >> $LOG_FILE
	fi
done

# Remove binaries & desktop integration (both names)
for name in $APP_NAME $LEGACY_NAME; do
	for prefix in /usr/sbin /usr/bin /usr/local/bin /usr/local/sbin; do
		if test -f $prefix/$name; then
			sudo rm -f $prefix/$name >> $LOG_FILE
		fi
	done
	if test -f /usr/share/applications/$name.desktop; then
		sudo rm -f /usr/share/applications/$name.desktop >> $LOG_FILE
	fi
	if test -f /usr/share/pixmaps/$name.png; then
		sudo rm -f /usr/share/pixmaps/$name.png >> $LOG_FILE
	fi
done

### Remove the service log file (keep post-uninstall.log)
if test -f "$LOG_FOLDER/$APP_NAME-service.log"; then
    sudo rm -f "$LOG_FOLDER/$APP_NAME-service.log" >> $LOG_FILE 2>&1
fi
if test -f "/var/log/$LEGACY_NAME/$LEGACY_NAME-service.log"; then
    sudo rm -f "/var/log/$LEGACY_NAME/$LEGACY_NAME-service.log" >> $LOG_FILE 2>&1
fi

### Remove user logs for current user only
TARGET_HOME="$HOME"
if [ -n "$SUDO_USER" ] && [ "$SUDO_USER" != "root" ]; then
    TARGET_HOME=$(getent passwd "$SUDO_USER" | cut -d: -f6)
fi
for combo in "$ORG_NAME/$APP_NAME" "$LEGACY_ORG_NAME/$LEGACY_NAME"; do
    if test -d "$TARGET_HOME/.local/share/$combo/log"; then
        rm -rf "$TARGET_HOME/.local/share/$combo/log" >> $LOG_FILE 2>&1
    fi
    rmdir "$TARGET_HOME/.local/share/$combo" 2>/dev/null || true
done
rmdir "$TARGET_HOME/.local/share/$ORG_NAME" 2>/dev/null || true
rmdir "$TARGET_HOME/.local/share/$LEGACY_ORG_NAME" 2>/dev/null || true

if command -v steamos-readonly &> /dev/null; then
	sudo steamos-readonly enable >> $LOG_FILE
	echo "steamos-readonly enabled" >> $LOG_FILE
fi

date >> $LOG_FILE
echo "Script finished" >> $LOG_FILE

