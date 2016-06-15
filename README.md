# libscapi
libscapi is the C++ high performance version of scapi (secure multi party computation - API). We are in beta,  release of first version is expected at 1 August 2016


##Installing libscapi on Ubuntu 14.04

#### Prerequisites
- sudo apt-get update
- sudo apt-get install git
- sudo apt-get install -y build-essential

#### Installing boost
- wget -O boost_1_60_0.tar.bz2 http://sourceforge.net/projects/boost/files/boost/1.60.0/boost_1_60_0.tar.bz2/download
- tar --bzip2 -xf boost_1_60_0.tar.bz2
-  ./bootstrap.sh
-  ./b2 
- sudo ldconfig ~/boost_1_60_0/stage/lib/

#### Installing OpenSSL
- sudo apt-get install libssl-ocaml-dev libssl-dev

#### Clone andd build libscapi
- git clone https://github.com/cryptobiu/libscapi.git
- cd libscapi
- make

#### Build and Run tests
- cd ~/libscapi/test
- make
- ./tests.exe
 
# License

#
