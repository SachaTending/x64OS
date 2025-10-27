# TODO: Prepare files for initrd

mkdir .tmp
cp base_root/* .tmp -v
cd .tmp
tar cvf ../initrd.tar *
cd ..
rm -rf tmp