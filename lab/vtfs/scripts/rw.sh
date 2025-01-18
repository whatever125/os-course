echo 'test' > /mnt/vt/test
cat /mnt/vt/test

echo -n $(printf 'A%.0s' {1..1024}) > /mnt/vt/test
cat /mnt/vt/test

echo -n $(printf 'A%.0s' {1..1025}) > /mnt/vt/test
cat /mnt/vt/test
