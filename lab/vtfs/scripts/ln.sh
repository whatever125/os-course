echo "test" > /mnt/vt/t
ls -lai /mnt/vt
ln /mnt/vt/t /mnt/vt/t1
ls -lai /mnt/vt
cat /mnt/vt/t
cat /mnt/vt/t1
rm /mnt/vt/t1
ls -lai /mnt/vt
cat /mnt/vt/t