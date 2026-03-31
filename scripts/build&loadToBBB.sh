##build file o
arm-linux-gnueabihf-gcc -o led_mmap led_mmap.c 

##load into bbb
scp led_mmap debian@10.42.0.206:/home/debian && sync