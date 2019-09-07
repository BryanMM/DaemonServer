echo " Uninstalling ImageServer Daemon"
sudo systemctl stop ImageServer.service
sudo rm -f /etc/systemd/system/ImageServer.service
sudo rm -f /usr/bin/daemon
sudo rm -rf /etc/ImageServer/ 
sudo systemctl daemon-reload
echo " Completed"
