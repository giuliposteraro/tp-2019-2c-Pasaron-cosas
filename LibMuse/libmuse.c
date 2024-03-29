/*
 * libmuse.c
 *
 *  Created on: 22 sep. 2019
 *      Author: utnso
 */

#include "libmuse.h"


int muse_init(int id, char* ip, int puerto){

	socket_muse = conectarseA(ip,puerto);
	if(socket_muse == 0){ return -1;}

	t_paquete paquete = {
			.header = MUSE_INIT,
			.parametros = list_create()
	};

	///////////////// Parametros a enviar /////////////////
	agregar_valor(paquete.parametros,id);
	enviar_paquete(paquete,socket_muse);
	///////////////////////////////////////////////////////

	///////////////// Parametros a recibir ////////////////
	t_paquete paquete_recibido = recibir_paquete(socket_muse);
	uint32_t valor_recibido = obtener_valor(paquete_recibido.parametros);
	//printf("Conexion exitosa?: %d\n",valor_recibido);
	///////////////////////////////////////////////////////

	return 0;
}

void muse_close(){

	/*
	 * Cuando hago un close, podria saber cuanta memoria
	 * (del programa que hizo close) quedo sin liberar
	 *
	 * */

	close(socket_muse);
}

uint32_t muse_alloc(uint32_t tam){
	if(tam == 0)
		return NULL;

	t_paquete paquete = {
			.header = MUSE_ALLOC,
			.parametros = list_create()
	};

	///////////////// Parametros a enviar /////////////////
	agregar_valor(paquete.parametros,tam);
	enviar_paquete(paquete,socket_muse);
	///////////////////////////////////////////////////////

	///////////////// Parametros a recibir ////////////////
	t_paquete paquete_recibido = recibir_paquete(socket_muse);
	int direccion_recibida = obtener_valor(paquete_recibido.parametros);
	//printf("Direccion recibida: %d\n",direccion_recibida);
	///////////////////////////////////////////////////////

	if(direccion_recibida < 0)
		return NULL;

	//printf("Fin muse_alloc\n");
	return (uint32_t)direccion_recibida;
}

void muse_free(uint32_t dir){
	if(dir == NULL)
		return;

	t_paquete paquete = {
			.header = MUSE_FREE,
			.parametros = list_create()
	};

	///////////////// Parametros a enviar /////////////////
	agregar_valor(paquete.parametros, dir);
	enviar_paquete(paquete,socket_muse);
	///////////////////////////////////////////////////////

	///////////////// Parametros a recibir ////////////////
	t_paquete paquete_recibido = recibir_paquete(socket_muse);
	uint32_t valor_recibido = obtener_valor(paquete_recibido.parametros);
	//printf("Free exitoso?: %d\n",valor_recibido);
	///////////////////////////////////////////////////////
}

int muse_get(void* dst, uint32_t src, size_t n){

	t_paquete paquete = {
			.header = MUSE_GET,
			.parametros = list_create()
	};

	///////////////// Parametros a enviar /////////////////
	agregar_valor(paquete.parametros,src);
	agregar_valor(paquete.parametros,n);
	enviar_paquete(paquete,socket_muse);
	///////////////////////////////////////////////////////

	///////////////// Parametros a recibir ////////////////
	t_paquete paquete_recibido = recibir_paquete(socket_muse);
	uint32_t valor_recibido = obtener_valor(paquete_recibido.parametros);
	///////////////////////////////////////////////////////

	if(valor_recibido == 2){
		raise(SIGSEGV);
		return -1;
	}

	void* bloque_datos_recibido = obtener_bloque_datos(paquete_recibido.parametros);
	memcpy(dst,bloque_datos_recibido,n);

	//printf("Get exitoso\n");

	return 0;
}

int muse_cpy(uint32_t dst, void* src, int n){

	t_paquete paquete = {
			.header = MUSE_CPY,
			.parametros = list_create()
	};

	///////////////// Parametros a enviar /////////////////
	agregar_valor(paquete.parametros,dst);
	agregar_valor(paquete.parametros,n);
	agregar_bloque_datos(paquete.parametros,src,n);
	enviar_paquete(paquete,socket_muse);
	///////////////////////////////////////////////////////

	///////////////// Parametros a recibir ////////////////
	t_paquete paquete_recibido = recibir_paquete(socket_muse);
	uint32_t valor_recibido = obtener_valor(paquete_recibido.parametros);
	///////////////////////////////////////////////////////

	if(valor_recibido == 2){
		raise(SIGSEGV);
		return -1;
	}

	//printf("Cpy exitoso\n");
	return 0;
}

uint32_t muse_map(char *path, size_t length, int flags){

	t_paquete paquete = {
			.header = MUSE_MAP,
			.parametros = list_create()
	};

	///////////////// Parametros a enviar /////////////////
	agregar_string(paquete.parametros,path);
	agregar_valor(paquete.parametros,length);
	agregar_valor(paquete.parametros,flags);
	enviar_paquete(paquete,socket_muse);
	///////////////////////////////////////////////////////

	///////////////// Parametros a recibir ////////////////
	t_paquete paquete_recibido = recibir_paquete(socket_muse);
	uint32_t direccion_recibida = obtener_valor(paquete_recibido.parametros);
	//printf("Direccion recibida: %d\n",direccion_recibida);
	///////////////////////////////////////////////////////

	//printf("Fin muse_map\n");
	return direccion_recibida;
}

int muse_sync(uint32_t addr, size_t len){

	t_paquete paquete = {
			.header = MUSE_SYNC,
			.parametros = list_create()
	};

	///////////////// Parametros a enviar /////////////////
	agregar_valor(paquete.parametros,addr);
	agregar_valor(paquete.parametros,len);
	enviar_paquete(paquete,socket_muse);
	///////////////////////////////////////////////////////

	///////////////// Parametros a recibir ////////////////
	t_paquete paquete_recibido = recibir_paquete(socket_muse);
	uint32_t valor_recibido = obtener_valor(paquete_recibido.parametros);
	///////////////////////////////////////////////////////

	if(valor_recibido == 2){
		raise(SIGSEGV);
		return -1; // creo que no es necesario este return
	}
	else if (valor_recibido == 3){
		//raise(SIG_ERR);
		printf("Fallo en muse_sync\n");
		return -1;
	}

	//printf("Sync exitoso\n");

	return 0;
}

int muse_unmap(uint32_t dir){

	t_paquete paquete = {
			.header = MUSE_UNMAP,
			.parametros = list_create()
	};

	///////////////// Parametros a enviar /////////////////
	agregar_valor(paquete.parametros, dir);
	enviar_paquete(paquete,socket_muse);
	///////////////////////////////////////////////////////

	///////////////// Parametros a recibir ////////////////
	t_paquete paquete_recibido = recibir_paquete(socket_muse);
	uint32_t valor_recibido = obtener_valor(paquete_recibido.parametros);
	///////////////////////////////////////////////////////

	if(valor_recibido == 2){
		raise(SIGSEGV);
		return -1; // creo que no es necesario este return
	}
	else if (valor_recibido == 3){
		printf("Fallo en muse_unmap\n");
		return -1;
	}

	//printf("unmap exitoso\n");
	return 0;
}
