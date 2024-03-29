/*
 * funcionesMuse.h
 *
 *  Created on: 3 oct. 2019
 *      Author: utnso
 */

#ifndef MADNESS_H_
#define MADNESS_H_

#include <math.h>
#include <commons/config.h>
#include <commons/string.h>
#include "muse.h"
#include "plugInBaby.h"

void procesar_solicitud(void* socket_cliente);
void leer_config();
void init_memoria();
void init_threads();
void servidor();

void funcion_init(t_paquete paquete,int socket_muse);
void funcion_close(t_paquete paquete,int socket_muse);
void funcion_alloc(t_paquete paquete,int socket_muse);
void funcion_free(t_paquete paquete,int socket_muse);
void funcion_get(t_paquete paquete,int socket_muse);
void funcion_cpy(t_paquete paquete,int socket_muse);
void funcion_map(t_paquete paquete,int socket_muse);
void funcion_sync(t_paquete paquete,int socket_muse);
void funcion_unmap(t_paquete paquete,int socket_muse);

char* obtener_ip_socket(int s);
void log_estado_del_sistema();

#endif /* MADNESS_H_ */
