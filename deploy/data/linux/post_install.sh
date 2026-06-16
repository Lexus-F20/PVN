#!/bin/bash

APP_NAME=PVN
LEGACY_NAME=AmneziaVPN
LOG_FOLDER=/var/log/$APP_NAME
LOG_FILE="$LOG_FOLDER/post-install.log"
APP_PATH=/opt/$APP_NAME
# The service/desktop/png files in the installer are still shipped under the
# AmneziaVPN.* name (upstream filenames retained); CPack ships them verbatim.
# Map them to the new APP_NAME at install time so systemd/desktop entries
# are branded as PVN.
LEGACY_SERVICE=$APP_PATH/$LEGACY_NAME.service
LEGACY_DESKTOP=$APP_PATH/$LEGACY_NAME.desktop
LEGACY_ICON=$APP_PATH/$LEGACY_NAME.png

if ! test -f $LOG_FOLDER; then
        sudo mkdir $LOG_FOLDER
        echo "$APP_NAME log dir created at /var/log/"
fi

if ! test -f $LOG_FILE; then
        touch $LOG_FILE
        echo "$APP_NAME log file created at $LOG_FILE"
fi

date > $LOG_FILE
echo "Script started" >> $LOG_FILE
sudo killall -9 $APP_NAME 2>> $LOG_FILE
sudo killall -9 $LEGACY_NAME 2>> $LOG_FILE

if command -v steamos-readonly &> /dev/null; then
        sudo steamos-readonly disable >> $LOG_FILE
        echo "steamos-readonly disabled" >> $LOG_FILE
fi

# Stop legacy AmneziaVPN service if present
if sudo systemctl is-active --quiet $LEGACY_NAME; then
	sudo systemctl stop $LEGACY_NAME >> $LOG_FILE
	sudo systemctl disable $LEGACY_NAME >> $LOG_FILE
	sudo rm -rf /etc/systemd/system/$LEGACY_NAME.service >> $LOG_FILE
fi

if sudo systemctl is-active --quiet $APP_NAME; then
	sudo systemctl stop $APP_NAME >> $LOG_FILE
	sudo systemctl disable $APP_NAME >> $LOG_FILE
	sudo rm -rf /etc/systemd/system/$APP_NAME.service >> $LOG_FILE
fi

sudo chmod -R a-w $APP_PATH/

# Install systemd unit
if [ -f "$LEGACY_SERVICE" ]; then
	sudo cp "$LEGACY_SERVICE" "/etc/systemd/system/$APP_NAME.service" >> $LOG_FILE
elif [ -f "$APP_PATH/$APP_NAME.service" ]; then
	sudo cp "$APP_PATH/$APP_NAME.service" "/etc/systemd/system/$APP_NAME.service" >> $LOG_FILE
fi

sudo systemctl daemon-reload >> $LOG_FILE
sudo systemctl start $APP_NAME >> $LOG_FILE
sudo systemctl enable $APP_NAME >> $LOG_FILE
sudo ln -sf $APP_PATH/bin/$APP_NAME /usr/local/sbin/$APP_NAME >> $LOG_FILE
sudo ln -sf $APP_PATH/bin/$APP_NAME /usr/local/bin/$APP_NAME >> $LOG_FILE

echo "user desktop creation loop started" >> $LOG_FILE
if [ -f "$LEGACY_DESKTOP" ]; then
	sudo cp "$LEGACY_DESKTOP" "/usr/share/applications/$APP_NAME.desktop" >> $LOG_FILE
elif [ -f "$APP_PATH/$APP_NAME.desktop" ]; then
	sudo cp "$APP_PATH/$APP_NAME.desktop" "/usr/share/applications/$APP_NAME.desktop" >> $LOG_FILE
fi
if [ -f "$LEGACY_ICON" ]; then
	sudo cp "$LEGACY_ICON" "/usr/share/pixmaps/$APP_NAME.png" >> $LOG_FILE
elif [ -f "$APP_PATH/$APP_NAME.png" ]; then
	sudo cp "$APP_PATH/$APP_NAME.png" "/usr/share/pixmaps/$APP_NAME.png" >> $LOG_FILE
fi
sudo chmod 555 /usr/share/applications/$APP_NAME.desktop >> $LOG_FILE

# Remove any stale legacy artifacts
sudo rm -f /usr/share/applications/$LEGACY_NAME.desktop 2>> $LOG_FILE
sudo rm -f /usr/share/pixmaps/$LEGACY_NAME.png 2>> $LOG_FILE

echo "user desktop creation loop ended" >> $LOG_FILE

if command -v steamos-readonly &> /dev/null; then
        sudo steamos-readonly enable >> $LOG_FILE
        echo "steamos-readonly enabled" >> $LOG_FILE
fi

date >> $LOG_FILE
echo "Service status:" >> $LOG_FILE
sudo systemctl status $APP_NAME >> $LOG_FILE
date >> $LOG_FILE
echo "Script finished" >> $LOG_FILE
exit 0

