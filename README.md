# Camera_2019


### Préparation RPi3.

On commence par récupérer le docker :

````
$ docker pull pblottiere/embsys-rpi3-buildroot
$ docker run -it pblottiere/embsys-rpi3-buildroot-video /bin/bash
# cd /root
# tar zxvf buildroot-precompiled-2017.08.tar.gz
````

On récupère ensuite le fichier sdcard.img :

```` shell
$ docker cp <container_id>:/root/buildroot-precompiled-2017.08/output/images/sdcard.img .
````

 et les fichiers start_x.elf et fixup_x.dat:

```` shell
$ docker cp <container_id>:/root/buildroot-precompiled-2017.08/output/build/rpi-firmware-685b3ceb0a6d6d6da7b028ee409850e83fb7ede7/boot/start_x.elf .
$ docker cp <container_id>:/root/buildroot-precompiled-2017.08/output/build/rpi-firmware-685b3ceb0a6d6d6da7b028ee409850e83fb7ede7/boot/fixup_x.dat .
````

On flash la carte SD avec sdcard.img : 

```` shell
$ sudo dd if=sdcard.img of=/dev/mmXXX
````

avec mmXXX qui est l'emplacement de la carte SD (peut etre retrouvé grace à la commande dmesg)

On copie les fichier start_x.elf et fixup_x.dat sur la partition 1 de la carte SD (la plus petite) via du drag and drop ou le terminal.

On ajoute les deux lignes au fichier config.txt

start_x=1
gpu_mem=128

Ces deux lignes vont permettre à notre rasp de boot avec start_x.elf et fixup_x.dat.



Il nous faut ensuite connaitre l'adresse IP de notre rasp afin de pouvoir s'y connecter en SSH (beaucoup plus simple et pratique). En s'y connectant via un connecteur serie (tx -> rx, rx -> tx et gnd -> gnd) et avec un terminal serie (gtkterm pour nous avec port USB0 et 115200 baud) on peut se connecter en tant que user/user1* .
On se met ensuite en root (su -> root1* ) pour faire la commande ifconfig afin d'avoir notre adresse IP (172.20.21.162).

En se connectant sur le même réseau que la rasp on peut maitenant 'faire du ssh'.


```` shell
$ ssh user@172.20.21.162
# user@172.20.21.162's password: user1*
 ````
 
### Cross-compilation
 #### Cross-compilation de v4l2grab
 On compile v4l2grab avec arm-linux-gcc sur le docker. On obtient un exécutable compatible avec la rpi3.
 
 Installation des librairies 
```` shell
$ apt-get update
$ apt-get install libjpeg-dev libv4l-dev autoconf automake libtool
 ````
 
 Clone du repository
```` shell
$ git clone https://github.com/twam/v4l2grab.git
$ cd v4l2grab
 ````
 
 Création fichiers autotools
```` shell
$ ./autogen.sh
 ````
 
 Ajout du path vers le compilateur

```` shell
$ export PATH=$PATH:/root/buildroot-precompiled-2017.08/output/host/usr/bin
 ````
 Configuration
```` shell
$ ./configure --host=arm-linux
 ````
 Commenter malloc dans le fichier configure.ac

#AC_FUNC_MALLOC

Make
```` shell
$ make
$ make install
```` 
 #### Cross-compilation de client et serveur
Dans le dossier src:
 ```` shell
$ docker cp clientserver_docker/ <container_id>:/root/buildroot-precompiled-2017.08
```` 
Dans le docker <container_id>:
 ```` shell
$ cd /root/buildroot-precompiled-2017.08/clientserver_docker
$ make
```` 
Dans le dossier src:
 ```` shell
$ docker cp <container_id>:/root/buildroot-precompiled-2017.08/clientserver_docker .
```` 



### Install Camera

lien du [tuto](https://www.dexterindustries.com/howto/installing-the-raspberry-pi-camera/)

On doit faire cet commande sur la raspberry pi
```` shell
$ modprobe bcm2835-v4l2
````
Verifier qu'elle soit bien installée:
```` shell
$ ls /dev/video0
/dev/video0
````

### Client Server

lancer le server (depuis la raspberry pi):
```` shell
$ cd /home/user/clientserver_docker
$ ./server <port> <pathToTheImageFolder>
````

lancer le client (depuis le pc):
```` shell
$ cd <pathToTheFolderclientserver_docker>
$ ./client <server_IP> <port> <"image_extension">
````
