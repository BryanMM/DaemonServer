echo " Installing ImageServer Daemon"
sudo cp ImageServer.service /etc/systemd/system/
sudo systemctl daemon-reload
echo " Completed"
echo "Start daemon: sudo systemctl start ImageServer.service"
echo "Status daemon: sudo systemctl status ImageServer.service"
echo "Stop daemon: sudo systemctl stop ImageServer.service"
