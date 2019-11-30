/*
 * segmentacionPaginada.c
 *
 *  Created on: 5 oct. 2019
 *      Author: utnso
 */

// Funciones para la segmentacion paginada de MUSE

#include "plugInBaby.h"


t_proceso* crear_proceso(char* id_programa,int socket_creado){
	t_proceso* nuevo = malloc(sizeof(t_proceso));

	nuevo->id_programa = strdup(id_programa);
	nuevo->socket = socket_creado;
	nuevo->tabla_segmentos = list_create();

	return nuevo;
	//list_add(lista_threads,nuevo);
}

uint32_t crear_segmento(uint8_t tipo,t_list* tabla_segmentos,uint32_t tam_solicitado) {
	int cantidad_paginas_solicitadas;
	t_heap_metadata heap_metadata;
	uint8_t bit_presencia = 0;
	uint32_t tam_solicitado_real = sizeof(heap_metadata.isFree) + sizeof(heap_metadata.size) + tam_solicitado;
	int posicion = 0;
	void* buffer;
	bool agregar_metadata_free = false;
	uint32_t direccion_retornada;

	t_segmento* nuevo = malloc(sizeof(t_segmento));
	nuevo->tipo_segmento = tipo;
	nuevo->base = obtener_base(tabla_segmentos,tipo,tam_solicitado);
	nuevo->archivo_mmap = NULL; //lo inicializo para los segmentos q no son mmap

	switch (tipo) {
		case SEGMENTO_HEAP:
			printf("Se crea un nuevo segmento heap\n");

			bit_presencia = 1;

			if((tam_solicitado_real%TAM_PAGINA) != 0){  // si da 0, no necesito agregar la metadata para indicar FREE
				tam_solicitado_real += sizeof(heap_metadata.isFree) + sizeof(heap_metadata.size); //agrego la metadata para indicar FREE
				agregar_metadata_free = true;
			}

			cantidad_paginas_solicitadas = (int)ceil((double)tam_solicitado_real/TAM_PAGINA);

			printf("cantidad paginas solicitadas %d\n",cantidad_paginas_solicitadas);

			nuevo->limite = cantidad_paginas_solicitadas * TAM_PAGINA;

			// comienzo a crear el buffer para luego dividirlo en paginas
			buffer = malloc(nuevo->limite);

			heap_metadata.isFree = false;
			heap_metadata.size = tam_solicitado;

			memcpy(&buffer[posicion],&heap_metadata.isFree,sizeof(heap_metadata.isFree));
			posicion += sizeof(heap_metadata.isFree);

			memcpy(&buffer[posicion],&heap_metadata.size,sizeof(heap_metadata.size));
			posicion += sizeof(heap_metadata.size) + heap_metadata.size;

			if(agregar_metadata_free){
				heap_metadata.isFree = true;
				heap_metadata.size = nuevo->limite - posicion - sizeof(heap_metadata.isFree) - sizeof(heap_metadata.size); // revisar despues si es lo mismo q hacer sizeof(t_heap_metadata)

				memcpy(&buffer[posicion],&heap_metadata.isFree,sizeof(heap_metadata.isFree));
				posicion += sizeof(heap_metadata.isFree);

				memcpy(&buffer[posicion],&heap_metadata.size,sizeof(heap_metadata.size));
			}

			list_add(tabla_segmentos,nuevo);

			nuevo->tabla_paginas = list_create();
			cargar_datos(buffer,nuevo,CREAR_DATOS,cantidad_paginas_solicitadas);
			//cargar_datos(buffer,nuevo->tabla_paginas,CREAR_DATOS,cantidad_paginas_solicitadas);

			free(buffer);

			direccion_retornada = nuevo->base + sizeof(heap_metadata.isFree) + sizeof(heap_metadata.size);
			break;
		case SEGMENTO_MMAP:
		case SEGMENTO_MMAP_EXISTENTE:
			printf("Se crea un nuevo segmento mmap\n");

			bit_presencia = 0;

			cantidad_paginas_solicitadas = (int)ceil((double)tam_solicitado/TAM_PAGINA);
			printf("cantidad paginas solicitadas %d\n",cantidad_paginas_solicitadas);

			nuevo->limite = cantidad_paginas_solicitadas * TAM_PAGINA;
			nuevo->tipo_segmento = SEGMENTO_MMAP;

			list_add(tabla_segmentos,nuevo);

			direccion_retornada = nuevo->base;

			if(tipo == SEGMENTO_MMAP_EXISTENTE)
				break;

			nuevo->tabla_paginas = list_create();
			cargar_datos(NULL,nuevo,CREAR_DATOS,cantidad_paginas_solicitadas);

			break;
	}

	return direccion_retornada;
}

t_pagina* crear_pagina(uint8_t bit_presencia,uint8_t tipo_segmento) {
	int frame_obtenido;

	t_pagina* new = malloc(sizeof(t_pagina));
    new->bit_usado = 0;
    new->bit_modificado = 0;
    new->bit_presencia = bit_presencia;
    new->tipo_segmento = tipo_segmento;
    new->frame = 0;

    if(!bit_presencia)
    	return new;

    frame_obtenido = obtener_frame_libre();
    if(frame_obtenido >= 0){
    	new->frame = frame_obtenido;
    }
    else{
    	// no hay espacio, por lo tanto se tiene que liberar un frame y pasarlo a swap
    	// revisar q pasa si no hay espacio en swap, igualmente
    	// a la hora de crear el segmento
    	t_pagina* pagina_a_reemplazar = ejecutar_algoritmo_clock_modificado();
    	pagina_a_reemplazar->bit_presencia = 0;
		new->frame = obtener_frame_libre();

    	if(pagina_a_reemplazar->tipo_segmento == SEGMENTO_HEAP){
			void* buffer = malloc(TAM_PAGINA);
			int frame_swap_obtenido = obtener_frame_swap_libre();

			memcpy(buffer,&upcm[pagina_a_reemplazar->frame*TAM_PAGINA],TAM_PAGINA);

			archivo_swap = fopen(PATH_SWAP,"r+");

			fseek(archivo_swap,frame_swap_obtenido*TAM_PAGINA,SEEK_SET);
			fwrite(buffer,TAM_PAGINA,1,archivo_swap);

			pagina_a_reemplazar->frame = frame_swap_obtenido;

			free(buffer);
			fclose(archivo_swap);
    	}
    }

    agregar_frame_clock(new);

    return new;
}

t_proceso* buscar_proceso(t_list* lista,int socket_proceso) {
	int igualSocket(t_proceso *p) {
		return p->socket == socket_proceso;
	}

	return list_find(lista, (void*) igualSocket);
}

t_segmento* buscar_segmento(t_list* tabla_segmentos,uint32_t direccion){
	t_segmento* segmento_obtenido;
	for(int i=0;i<list_size(tabla_segmentos);i++){
		segmento_obtenido = list_get(tabla_segmentos,i);
		if((segmento_obtenido->base <= direccion) && (segmento_obtenido->limite >= direccion)){
			return segmento_obtenido;
		}
	}

	return NULL;
}

t_archivo_mmap* buscar_archivo_mmap(int fd_archivo){
	int igualArchivo(t_archivo_mmap* archivo_mmap) {
	    struct stat stat1, stat2;
	    if((fstat(fd_archivo, &stat1) < 0) || (fstat(fileno(archivo_mmap->archivo), &stat2) < 0)) return -1;
	    return (stat1.st_dev == stat2.st_dev) && (stat1.st_ino == stat2.st_ino);
	}

	return list_find(lista_archivos_mmap,(void*) igualArchivo);
}

void agregar_archivo_mmap(FILE* archivo,int socket_proceso,t_list* tabla_paginas){
	t_archivo_mmap* archivo_mmap_nuevo = malloc(sizeof(t_archivo_mmap));
	archivo_mmap_nuevo->archivo = archivo;
	archivo_mmap_nuevo->sockets_procesos = list_create();
	list_add(archivo_mmap_nuevo->sockets_procesos,socket_proceso);
	archivo_mmap_nuevo->tabla_paginas = tabla_paginas;

	list_add(lista_archivos_mmap,archivo_mmap_nuevo);
}

void cargar_datos(void* buffer,t_segmento* segmento,uint32_t flag_operacion,int cantidad_paginas_solicitadas){
	t_pagina* pagina;
	void* direccion_frame;
	int paginas_a_recorrer = cantidad_paginas_solicitadas;
	uint8_t bit_presencia = 1;

	if(segmento->tipo_segmento == SEGMENTO_MMAP)
		bit_presencia = 0;

	if(paginas_a_recorrer == NULL)
		paginas_a_recorrer = list_size(segmento->tabla_paginas);

	for(int numero_pagina=0;numero_pagina<paginas_a_recorrer;numero_pagina++){
		if(flag_operacion == CREAR_DATOS){
			pagina = crear_pagina(bit_presencia,segmento->tipo_segmento);
			list_add(segmento->tabla_paginas,pagina);
			printf("Se actualizo la tabla de paginas\n");
			if(!bit_presencia)
				continue;
		}
		else{
			pagina = list_get(segmento->tabla_paginas,numero_pagina);
		}
		direccion_frame = obtener_datos_frame(pagina);

		switch(flag_operacion){
			case CARGAR_DATOS:
				memcpy(&buffer[TAM_PAGINA*numero_pagina],direccion_frame,TAM_PAGINA);
				break;
			case GUARDAR_DATOS:
			case CREAR_DATOS:
				memcpy(direccion_frame,&buffer[TAM_PAGINA*numero_pagina],TAM_PAGINA);
		    	//printf("Se copiaron los datos nuevos\n");
				break;
		}
	}
}

void* obtener_datos_frame(t_pagina* pagina){
	if(!pagina->bit_presencia && !espacio_en_upcm()){
    	t_pagina* pagina_upcm = ejecutar_algoritmo_clock_modificado();

    	void* buffer_pagina_upcm = malloc(TAM_PAGINA);
    	void* buffer_pagina_swap = malloc(TAM_PAGINA);

    	memcpy(buffer_pagina_upcm,&upcm[pagina_upcm->frame*TAM_PAGINA],TAM_PAGINA);

    	archivo_swap = fopen(PATH_SWAP,"r+");

    	fseek(archivo_swap,pagina->frame*TAM_PAGINA,SEEK_SET);
    	fread(buffer_pagina_swap,TAM_PAGINA,1,archivo_swap);
    	fseek(archivo_swap,pagina->frame*TAM_PAGINA,SEEK_SET);
    	fwrite(buffer_pagina_upcm,TAM_PAGINA,1,archivo_swap);

    	memcpy(&upcm[pagina_upcm->frame*TAM_PAGINA],buffer_pagina_swap,TAM_PAGINA);

    	pagina_upcm->bit_presencia = 0;
    	pagina_upcm->frame = pagina->frame;

    	pagina->frame = obtener_frame_libre();
    	pagina->bit_presencia = 1;
    	pagina->bit_usado = 1;

    	agregar_frame_clock(pagina);

    	free(buffer_pagina_upcm);
    	free(buffer_pagina_swap);
    	fclose(archivo_swap);
	}
	else if(!pagina->bit_presencia && espacio_en_upcm()){
    	void* buffer_pagina_swap = malloc(TAM_PAGINA);

    	archivo_swap = fopen(PATH_SWAP,"r+");

    	fseek(archivo_swap,pagina->frame*TAM_PAGINA,SEEK_SET);
    	fread(buffer_pagina_swap,TAM_PAGINA,1,archivo_swap);

    	pagina->frame = obtener_frame_libre();
    	pagina->bit_presencia = 1;
    	pagina->bit_usado = 1;

    	memcpy(&upcm[pagina->frame*TAM_PAGINA],buffer_pagina_swap,TAM_PAGINA);

    	agregar_frame_clock(pagina);

    	free(buffer_pagina_swap);
    	fclose(archivo_swap);
	}

	return (char*) upcm + (pagina->frame * TAM_PAGINA);
}


void* obtener_datos_frame_mmap(t_segmento* segmento,t_pagina* pagina,int nro_pagina){
	if(!pagina->bit_presencia && !espacio_en_upcm()){
    	t_pagina* pagina_upcm = ejecutar_algoritmo_clock_modificado();

    	void* buffer_pagina_upcm = malloc(TAM_PAGINA);
    	void* buffer_pagina_mmap = malloc(TAM_PAGINA);
    	int frame_swap_obtenido = obtener_frame_swap_libre();

    	memcpy(buffer_pagina_upcm,&upcm[pagina_upcm->frame*TAM_PAGINA],TAM_PAGINA);

		archivo_swap = fopen(PATH_SWAP,"r+");

		fseek(archivo_swap,frame_swap_obtenido*TAM_PAGINA,SEEK_SET);
		fwrite(buffer_pagina_upcm,TAM_PAGINA,1,archivo_swap);

		pagina_upcm->frame = frame_swap_obtenido;
    	pagina_upcm->bit_presencia = 0;

      	pagina->frame = obtener_frame_libre();
    	pagina->bit_presencia = 1;
    	pagina->bit_usado = 1;

    	// si este metodo no sirve, agregar un campo length_mmap en t_segmento
    	if((nro_pagina*TAM_PAGINA) <= segmento->tam_archivo_mmap){
    		fseek(segmento->archivo_mmap,nro_pagina*TAM_PAGINA,SEEK_SET);

    		int bytes_a_leer = (int)fmin(TAM_PAGINA,((nro_pagina*TAM_PAGINA) + TAM_PAGINA) - segmento->tam_archivo_mmap);

    		fread(buffer_pagina_mmap,bytes_a_leer,1,segmento->archivo_mmap);
    		memcpy(&upcm[pagina->frame*TAM_PAGINA],buffer_pagina_mmap,bytes_a_leer);

    		if(TAM_PAGINA > bytes_a_leer)
    			memset(&upcm[pagina->frame*bytes_a_leer],'\0',TAM_PAGINA - bytes_a_leer);
    	}
    	else{
    		// el primer byte a leer supera el tamano del archivo
    		memset(&upcm[pagina->frame*TAM_PAGINA],'\0',TAM_PAGINA);
    	}

    	agregar_frame_clock(pagina);

    	free(buffer_pagina_upcm);
    	free(buffer_pagina_mmap);
    	fclose(archivo_swap);
	}
	else if(!pagina->bit_presencia && espacio_en_upcm()){
    	void* buffer_pagina_mmap = malloc(TAM_PAGINA);

    	pagina->frame = obtener_frame_libre();
    	pagina->bit_presencia = 1;
    	pagina->bit_usado = 1;

		fseek(segmento->archivo_mmap,nro_pagina*TAM_PAGINA,SEEK_SET);
		fread(buffer_pagina_mmap,TAM_PAGINA,1,segmento->archivo_mmap);

    	memcpy(&upcm[pagina->frame*TAM_PAGINA],buffer_pagina_mmap,TAM_PAGINA);

    	agregar_frame_clock(pagina);

    	free(buffer_pagina_mmap);
	}

	return (char*) upcm + (pagina->frame * TAM_PAGINA);
}


int obtener_frame_libre(){
	for(int i=0;i<cantidad_frames;i++){
		if(!bitarray_test_bit(bitmap_upcm,i)){ // retorna el primer bit q encuentre en 0
			bitarray_set_bit(bitmap_upcm,i);
			return i;
		}
	}
	return -1; // no hay frames libres
}

int obtener_frame_swap_libre(){
	for(int i=0;i<cantidad_frames_swap;i++){
		if(!bitarray_test_bit(bitmap_swap,i)){ // retorna el primer bit q encuentre en 0
			bitarray_set_bit(bitmap_swap,i);
			return i;
		}
	}
	return -1; // no hay frames libres
}

uint32_t obtener_base(t_list* tabla_segmentos,uint8_t tipo_segmento,uint32_t tam_solicitado){
	t_segmento* segmento_obtenido;

	// si es MMAP, reviso si hay espacio entre los segmentos ya creados para meter el map nuevo
	if(tipo_segmento == SEGMENTO_MMAP){
		uint32_t direccion_obtenida = 0;
		for(int nro_segmento=0;nro_segmento<list_size(tabla_segmentos);nro_segmento++){
			segmento_obtenido = list_get(tabla_segmentos,nro_segmento);
			if(!direccion_obtenida && ((direccion_obtenida + tam_solicitado) < segmento_obtenido->base))
				return direccion_obtenida;
			direccion_obtenida = segmento_obtenido->base + segmento_obtenido->limite;
		}
	}

	// obtengo el ultimo segmento
	if(list_size(tabla_segmentos)){
		segmento_obtenido = list_get(tabla_segmentos,list_size(tabla_segmentos) - 1);
		return segmento_obtenido->base + segmento_obtenido->limite;
	}

	return 0;
}

int obtener_tam_archivo(int fd_archivo){
	struct stat st;
	fstat(fd_archivo,&st);
	return st.st_size;
}

bool espacio_en_upcm(){
	for(int i=0;i<cantidad_frames;i++){
		if(!bitarray_test_bit(bitmap_upcm,i)){ // retorna el primer bit q encuentre en 0
			return true;
		}
	}
	return false;
}

int filtrarHeap(t_segmento* p){
	return p->tipo_segmento == SEGMENTO_HEAP;
}

void liberar_frame(int numero_frame){
	bitarray_clean_bit(bitmap_upcm,numero_frame);
}

void liberar_frame_swap(int numero_frame_swap){
	bitarray_clean_bit(bitmap_swap,numero_frame_swap);
}

void eliminar_pagina(t_pagina* pagina){
	printf("Eliminando pagina\n");
	if(pagina->bit_presencia){
		//memset(obtener_datos_frame(pagina),NULL,TAM_PAGINA);
		int igualFrameClock(t_pagina* p) {
			return p->frame == pagina->frame;
		}

		list_remove_by_condition(lista_clock,(void*) igualFrameClock);
		liberar_frame(pagina->frame);
	}
	else if(!pagina->bit_presencia && (pagina->tipo_segmento == SEGMENTO_HEAP)){
		// liberar del swap
		// podria hacer memset de la pagina en archivo_swap, pero creo q no hace falta
		liberar_frame_swap(pagina->frame);
	}

	free(pagina);
	printf("Se elimino la pagina\n");
}

void eliminar_segmento(t_segmento* segmento){
	if((segmento->tipo_segmento == SEGMENTO_HEAP) || ((segmento->tipo_segmento == SEGMENTO_MMAP) && (segmento->tipo_map == MAP_PRIVATE)))
		list_destroy_and_destroy_elements(segmento->tabla_paginas,(void*) eliminar_pagina);
	free(segmento);
}

void eliminar_archivo_mmap(t_archivo_mmap* archivo_mmap){
	if(archivo_mmap->tabla_paginas != NULL)
		list_destroy_and_destroy_elements(archivo_mmap->tabla_paginas,(void*) eliminar_pagina);
	list_destroy(archivo_mmap->sockets_procesos);
	fclose(archivo_mmap->archivo);
	free(archivo_mmap);
}

// me va a retornar los datos de la pagina que se libera de la memoria principal
// ahora la nueva version va a retornar la pagina que sera reemplazada
t_pagina* ejecutar_algoritmo_clock_modificado(){
	//printf("Se ejecuta el algoritmo clock modificado\n");
	t_proceso* proceso_obtenido;
	t_segmento* segmento_obtenido;
	t_pagina* pagina_obtenida;
	int primer_frame_recorrido = algoritmo_clock_frame_recorrido;
	int nro_vuelta = 1;

	while(true){

		pagina_obtenida = list_get(lista_clock,algoritmo_clock_frame_recorrido);

		switch(nro_vuelta){
			case 1:
				if(!pagina_obtenida->bit_usado && !pagina_obtenida->bit_modificado){
					//datos_pagina = malloc(TAM_PAGINA);
					//memcpy(datos_pagina,&upcm[algoritmo_clock_frame_recorrido*TAM_PAGINA],TAM_PAGINA);
					liberar_frame(algoritmo_clock_frame_recorrido);
					list_remove(lista_clock,algoritmo_clock_frame_recorrido);
					//pagina_obtenida->bit_presencia = 0;
					algoritmo_clock_frame_recorrido++;
					if(algoritmo_clock_frame_recorrido == cantidad_frames){
						algoritmo_clock_frame_recorrido = 0;
					}
					//return datos_pagina;
					return pagina_obtenida;
				}
				break;
			case 2:
				if(!pagina_obtenida->bit_usado && pagina_obtenida->bit_modificado){
					//datos_pagina = malloc(TAM_PAGINA);
					//memcpy(datos_pagina,&upcm[algoritmo_clock_frame_recorrido*TAM_PAGINA],TAM_PAGINA);
					liberar_frame(algoritmo_clock_frame_recorrido);
					list_remove(lista_clock,algoritmo_clock_frame_recorrido);
					//pagina_obtenida->bit_presencia = 0;
					algoritmo_clock_frame_recorrido++;
					if(algoritmo_clock_frame_recorrido == cantidad_frames){
						algoritmo_clock_frame_recorrido = 0;
					}
					//return datos_pagina;
					return pagina_obtenida;
				}
				else{
					pagina_obtenida->bit_usado = 0;
				}
				break;
		}

		algoritmo_clock_frame_recorrido++;
		if(algoritmo_clock_frame_recorrido == cantidad_frames){
			algoritmo_clock_frame_recorrido = 0;
		}
		if(algoritmo_clock_frame_recorrido == primer_frame_recorrido){
			nro_vuelta++;
			if(nro_vuelta>2){
				nro_vuelta = 1;
			}
			printf("vuelta nro %d\n",nro_vuelta);
		}
	}
}

void agregar_frame_clock(t_pagina* pagina){
	list_add_in_index(lista_clock,pagina->frame,pagina);
}
