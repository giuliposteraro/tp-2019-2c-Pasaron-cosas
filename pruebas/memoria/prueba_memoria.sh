cd

cd tp-2019-2c-Pasaron-cosas/biblioteca/Debug/
make clean
make
sudo cp libbiblioteca.so /usr/lib/
cd

cd tp-2019-2c-Pasaron-cosas/LibMuse/Debug/
make clean
make
sudo cp liblibMuse.so /usr/lib/
cd

cd tp-2019-2c-Pasaron-cosas/muse/Debug/
make clean
make
cd

cp tp-2019-2c-Pasaron-cosas/pruebas/memoria/muse.config tp-2019-2c-Pasaron-cosas/muse/

cd linuse-tests-programs/
make && make entrega
cd
