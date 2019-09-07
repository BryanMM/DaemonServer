# DaemonServer
Tarea 1 para el curso de sistemas operativos

Instalación de bibliotecas
    $ sudo apt install python-pip
    $ pip install Wand

Iniciar 
    $ chmod +x run.sh
    $ ./run.sh

Instalar del demonio:
    $ ./scripts/install-daemon.sh

Desinstalar demonio:
    $ ./scripts/uninstall-daemon.sh

Controlar ejecución del demonio
    $ systemctl start ImageServer.service
    $ systemctl status ImageServer.service
    $ systemctl stop ImageServer.service
    $ systemctl reload ImageServer.service
    