echo "Setup ImageServer"
chmod +x ./scripts/install-daemon.sh
chmod +x ./scripts/uninstall-daemon.sh

cd src
make daemon
cd ..

./scripts/uninstall-daemon.sh
./scripts/install-daemon.sh

systemctl start ImageServer.service
