#!/bin/bash

INSTALL_DIR=
WORKSPACE_DIR=

U_ID=`id -u`
G_ID=`id -g`
HOSTNAME=`hostname`


read -p "please input the install directory:" INSTALL_DIR

mkdir -p $INSTALL_DIR

cp -arf rootfs_sample $INSTALL_DIR/rootfs

cp -arf docker-compose.sample.yaml $INSTALL_DIR/docker-compose.yaml

sed -i -e "s?%UID%?${U_ID}?g" $INSTALL_DIR/docker-compose.yaml
sed -i -e "s?%GID%?${G_ID}?g" $INSTALL_DIR/docker-compose.yaml

read -p "input the path which will be mounted at /workspace in container:" WORKSPACE

sed -i -e "s?%WORKSPACE%?${WORKSPACE}?g" $INSTALL_DIR/docker-compose.yaml
sed -i -e "s?%HOSTNAME%?${HOSTNAME}?g" $INSTALL_DIR/docker-compose.yaml
sed -i -e "s?%USER%?${USER}?g" $INSTALL_DIR/docker-compose.yaml
grep "$USER:" /etc/passwd >> $INSTALL_DIR/rootfs/etc/passwd
grep "$USER:" /etc/group >> $INSTALL_DIR/rootfs/etc/group

mkdir -p  $INSTALL_DIR/rootfs/home/$USER

cp -arf /etc/localtime $INSTALL_DIR/rootfs/etc/

echo "docker has been installed at: $INSTALL_DIR"

echo "Now please enter directory: $INSTALL_DIR"
echo "and type \"docker-compose run ubuntu_18.04 /bin/bash\" to start the container"
