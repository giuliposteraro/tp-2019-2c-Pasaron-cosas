#ifndef SERVIDOR_H_
#define SERVIDOR_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <commons/config.h>
#include "../../biblioteca/biblioteca.h"
#include "../../biblioteca/biblioteca_sockets.h"
#include "funcionesSuse.h"


void inicializarServidor();
void servidor();
void procesar_solicitud(void *socket_cliente);


/////////  FUNCIONES SUSE  /////////

void funcion_create(t_paquete paquete,int socket_suse);
void funcion_schedule_next(t_paquete paquete,int socket_suse);
void funcion_wait(t_paquete paquete,int socket_suse);
void funcion_signal(t_paquete paquete,int socket_suse);
void funcion_join(t_paquete paquete,int socket_suse);
void funcion_close(t_paquete paquete,int socket_suse);

#endif /* SERVIDOR_H_ */