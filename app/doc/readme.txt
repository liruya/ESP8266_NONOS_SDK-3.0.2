项目在linux下开发, 需要搭建python2.7虚拟环境, 本机已搭建

终端进入项目文件夹/app目录下
cd app
进入python2.7 虚拟环境
workon env27

编译生成bin文件, compile.sh为编译脚本,传入参数生成相应bin文件
-p 必须, 参数传入产品类型 [led|socket|monsoon]
-v 可选, 不传默认version=1, -v 5 表示编译生成固件版本为5
编译生成的文件位置 app/bin/${product}/${product}_v${version}.bin
./compile.sh -p [led|socket|monsoon] [optional -v version]

擦除flash
./erase_flash.sh

下载全部bin文件, 包括boot, user_bin, esp_init_data_default, blank, ca_cert(阿里云物联网平台证书)
./write_all.sh -p [led|socket|monsoon] [optional -v version]

下载app bin文件
./write_app.sh -p [led|socket|monsoon] [optional -v version]


备注:

本项目需要编译器 xtensa-lx106-elf, 已安装在/opt/xtensa-lx106-elf, 若要在其他电脑开发, 复制该文件夹, 并配置环境变量
修改用户文件夹下 .bashrc文件
cd ~
nano ~/.bashrc
添加环境变量
export PATH=/opt/xtensa-lx106-elf/bin:$PATH
生效
source .bashrc


搭建python2.7虚拟环境, 需要先安装python及pip
cd ~

sudo pip install virtualenvwrapper

mkdir .virtualenvs

sudo nano .bashrc

/* 添加到.bashrc文件 */
	export WORKON_HOME=~/.virtualenvs
	export VIRTUALENVWRAPPER_PYTHON=/usr/bin/python3.7
	export VIRTUALENVWRAPPER_VIRTUALENV=/usr/bin/virtualenv
	source /usr/bin/virtualenvwrapper.sh
/*******************/

source .bashrc

mkvirtualenv -p /usr/bin/python2.7 env27


linux下载工具 esptool,  windows下可使用官方下载软件


为便于测试OTA升级功能, 在NAS部署了设备固件版本接口
http://ledinpro.synology.me:8086/exoterra/firmwares.json
编译新固件后, 上传至/docker/nginx/files/exoterra/${product} 文件夹下, 并按格式修改firmwares.json文件
