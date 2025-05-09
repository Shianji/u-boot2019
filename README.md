这是黑芝麻bst a1000b上面的u-boot2019代码。
其中a1000_uboot2019_analysis在自己学习u-boot启动流程过程中加了一些注释，目前已经无法编译通过。
a1000_uboot2019是可以编译通过的完整代码。
编译之前需要获取黑芝麻的工具链bstos-gcc-glibc-x86_64-core-image-bstos-sdk-aarch64-a1000-toolchain-linux-23.rar，加压安装到linux，
然后执行命令source /opt/bstos/linux-23/environment-setup-aarch64-bst-linux使工具链生效就可以执行./make-a1000b.sh编译了。
