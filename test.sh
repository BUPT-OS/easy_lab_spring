#!/bin/bash

# 使用方法：
#  sudo bash ./test.sh create
#  sudo bash ./test.sh recover

create_disk() {
    echo "[+] Creating disk image..."
    dd if=/dev/zero of=fat32.disk bs=1M count=6 conv=fsync
    mkfs.fat -F 32 -f 2 -S 1024 -s 1 -R 32 fat32.disk
    mkdir -p ./mnt/
    sudo mount -o umask=0 ./fat32.disk ./mnt/
    touch ./mnt/test.txt
    echo "0000000000000000000000000000000000000000000000
0000000000000000000000000000000000000000000000
0000000000000000000000000000000000000000000000
0000000000000000000000000000000000000000000000
0000000000000000000000000000000000000000000000
0000000000000000000000000000000000000000000000
0000000000000000000000000000000000000000000000
0000000000000000000000000000000000000000000000
0000000000000000000000000000000000000000000000
0000000000000000000000000000000000000000000000
0000000000000000000000000000000000000000000000
0000000000000000000000000000000000000000000000
0000000000000000000000000000000000000000000000
0000000000000000000000000000000000000000000000" > ./mnt/test.txt
    sync
    # hexdump -C ./fat32.disk  > d.txt
    rm ./mnt/test.txt
    sync
    sudo umount ./mnt/
    sudo rm -rf ./mnt
}

recover_disk() {
    echo "[+] Recovering disk..."
    gcc recover.c -lm -g -o recover
    ./recover ./fat32.disk test.txt recovered_data.txt
    # 可以使用以下命令 debug:
    # gdb --args ./recover ./fat32.disk test.txt recovered_data.txt
}

if [ $# -ne 1 ]; then
    echo "Usage: $0 {create|recover}"
    exit 1
fi

case "$1" in
    create)
        create_disk
        ;;
    recover)
        recover_disk
        ;;
    *)
        echo "Invalid argument."
        echo "Usage: $0 {create|recover}"
        exit 1
        ;;
esac