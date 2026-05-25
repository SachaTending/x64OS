# TODO: Prepare files for initrd

rm -rf .tmp
mkdir .tmp
cp base_root/* .tmp -v -r

cd init
#nasm -felf64 init.asm -o init.o
#ld init.o -o init
gcc -o crt0.o -c crt0.S
gcc crt0.o main.c -o init libc.a -nostdlib -ffreestanding -static
if [ ! -f "../.tmp/init" ]; then
    cp init ../.tmp/
fi
cp ld.so ../.tmp/
cd ..

cd .tmp
tar cvf ../initrd.tar *
cd ..
rm -rf .tmp