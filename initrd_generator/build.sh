# TODO: Prepare files for initrd

rm -rf .tmp
mkdir .tmp
cp base_root/* .tmp -v

cd init
nasm -felf64 init.asm -o init.o
ld init.o -o init
cp init ../.tmp/
cd ..

cd .tmp
tar cvf ../initrd.tar *
cd ..
rm -rf .tmp