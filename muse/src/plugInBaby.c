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
			//printf("Se crea un nuevo segmento heap\n");

			bit_presencia = 1;

			if((tam_solicitado_real%TAM_PAGINA) != 0){  // si da 0, no necesito agregar la metadata para indicar FREE
				tam_solicitado_real += sizeof(heap_metadata.isFree) + sizeof(heap_metadata.size); //agrego la metadata para indicar FREE
				agregar_metadata_free = true;
			}

			cantidad_paginas_solicitadas = (int)ceil((double)tam_solicitado_real/TAM_PAGINA);

			// si se solicitan mas paginas de las que hay disponibles, no hay que hacer alloc
			if(cantidad_paginas_solicitadas > obtener_cantidad_frames_disponibles()){
				free(nuevo);
				return -1;
			}

			//printf("cantidad paginas solicitadas %d\n",cantidad_paginas_solicitadas);

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
			cargar_datos(buffer,NULL,nuevo,CREAR_DATOS,cantidad_paginas_solicitadas);

			free(buffer);

			direccion_retornada = nuevo->base + sizeof(heap_metadata.isFree) + sizeof(heap_metadata.size);
			break;
		case SEGMENTO_MMAP:
		case SEGMENTO_MMAP_EXISTENTE:
			//printf("Se crea un nuevo segmento mmap\n");

			bit_presencia = 0;

			cantidad_paginas_solicitadas = (int)ceil((double)tam_solicitado/TAM_PAGINA);
			//printf("cantidad paginas solicitadas %d\n",cantidad_paginas_solicitadas);

			if(cantidad_paginas_solicitadas > obtener_cantidad_frames_disponibles()){
				free(nuevo);
				return -1;
			}

			nuevo->limite = cantidad_paginas_solicitadas * TAM_PAGINA;
			nuevo->tipo_segmento = SEGMENTO_MMAP;

			list_add(tabla_segmentos,nuevo);

			direccion_retornada = nuevo->base;

			if(tipo == SEGMENTO_MMAP_EXISTENTE)
				break;

			nuevo->tabla_paginas = list_create();
			cargar_datos(NULL,NULL,nuevo,CREAR_DATOS,cantidad_paginas_solicitadas);

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
    new->puede_pasar_por_swap = true;

	//printf("Estoy por crear una pagina\n");

    if(!bit_presencia){
    	// si el segmento es MMAP, aun no paso por swap. Podra pasar una vez que este en UPCM
    	new->puede_pasar_por_swap = false;
    	return new;
    }

    new->bit_usado = 1;

    frame_obtenido = obtener_frame_libre();
    if(frame_obtenido >= 0){
    	//printf("Hay frame libre\n");
    	new->frame = frame_obtenido;
    }
    else{
    	//printf("NO hay frame libre\n");
    	// no hay espacio, por lo tanto se tiene que liberar un frame y pasarlo a swap
    	// revisar q pasa si no hay espacio en swap, igualmente
    	// a la hora de crear el segmento
    	t_pagina* pagina_a_reemplazar = ejecutar_algoritmo_clock_modificado();
    	//printf("Se encontro la pagina a reemplazar\n");
    	pagina_a_reemplazar->bit_presencia = 0;
		new->frame = obtener_frame_libre();
		//new->bit_usado = 1;

		void* buffer = malloc(TAM_PAGINA);
		int frame_swap_obtenido = obtener_frame_swap_libre();

		memcpy(buffer,&upcm[pagina_a_reemplazar->frame*TAM_PAGINA],TAM_PAGINA);

		//archivo_swap = fopen(PATH_SWAP,"r+");

		fseek(archivo_swap,frame_swap_obtenido*TAM_PAGINA,SEEK_SET);
		fwrite(buffer,TAM_PAGINA,1,archivo_swap);

		pagina_a_reemplazar->frame = frame_swap_obtenido;
		pagina_a_reemplazar->puede_pasar_por_swap = true;

		free(buffer);
		//fclose(archivo_swap);
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

void cargar_datos(void* buffer,void* buffer_copia,t_segmento* segmento,uint32_t flag_operacion,int cantidad_paginas_solicitadas){
	t_pagina* pagina;
	void* direccion_frame;
	int paginas_a_recorrer = cantidad_paginas_solicitadas;
	uint8_t bit_presencia = 1;

	if(segmento->tipo_segmento == SEGMENTO_MMAP)
		bit_presencia = 0;

	if(paginas_a_recorrer == 0)
		paginas_a_recorrer = list_size(segmento->tabla_paginas);

	for(int numero_pagina=0;numero_pagina<paginas_a_recorrer;numero_pagina++){
		if(flag_operacion == CREAR_DATOS){
			pagina = crear_pagina(bit_presencia,segmento->tipo_segmento);
			list_add(segmento->tabla_paginas,pagina);

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
				memcpy(direccion_frame,&buffer[TAM_PAGINA*numero_pagina],TAM_PAGINA);
				if(buffer_copia == NULL)
					break;
				if(memcmp(&buffer[TAM_PAGINA*numero_pagina],&buffer_copia[TAM_PAGINA*numero_pagina],TAM_PAGINA) != 0){
					pagina->bit_modificado = 1;
				}
				break;
			case CREAR_DATOS:
				memcpy(direccion_frame,&buffer[TAM_PAGINA*numero_pagina],TAM_PAGINA);
		    	//printf("Se copiaron los datos nuevos\n");
				//printf("Pagina que guarde: nro %d\tbp %d\tframe %d\t\n",numero_pagina,pagina->bit_presencia,pagina->frame);
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

    	//archivo_swap = fopen(PATH_SWAP,"r+");

    	fseek(archivo_swap,pagina->frame*TAM_PAGINA,SEEK_SET);
    	fread(buffer_pagina_swap,TAM_PAGINA,1,archivo_swap);
    	fseek(archivo_swap,pagina->frame*TAM_PAGINA,SEEK_SET);
    	fwrite(buffer_pagina_upcm,TAM_PAGINA,1,archivo_swap);

    	memcpy(&upcm[pagina_upcm->frame*TAM_PAGINA],buffer_pagina_swap,TAM_PAGINA);

    	pagina_upcm->bit_presencia = 0;
    	pagina_upcm->bit_modificado = 0;
    	// no pongo en 0 el bit usado pq debio hacerlo el algoritmo clock
    	pagina_upcm->frame = pagina->frame;

    	pagina->frame = obtener_frame_libre();
    	pagina->bit_presencia = 1;
    	pagina->bit_usado = 1;

    	agregar_frame_clock(pagina);

    	free(buffer_pagina_upcm);
    	free(buffer_pagina_swap);
    	//fclose(archivo_swap);
	}
	else if(!pagina->bit_presencia && espacio_en_upcm()){
    	void* buffer_pagina_swap = malloc(TAM_PAGINA);

    	//archivo_swap = fopen(PATH_SWAP,"r+");

    	fseek(archivo_swap,pagina->frame*TAM_PAGINA,SEEK_SET);
    	fread(buffer_pagina_swap,TAM_PAGINA,1,archivo_swap);

    	pagina->frame = obtener_frame_libre();
    	pagina->bit_presencia = 1;
    	pagina->bit_usado = 1;

    	memcpy(&upcm[pagina->frame*TAM_PAGINA],buffer_pagina_swap,TAM_PAGINA);

    	agregar_frame_clock(pagina);

    	free(buffer_pagina_swap);
    	//fclose(archivo_swap);
	}

	return (char*) upcm + (pagina->frame * TAM_PAGINA);
}

void* obtener_datos_frame_mmap(t_segmento* segmento,t_pagina* pagina,int nro_pagina){
	if(!pagina->bit_presencia && !espacio_en_upcm()){
		//printf("El frame no esta en memoria y no hay espacio\n");
    	t_pagina* pagina_upcm = ejecutar_algoritmo_clock_modificado();

    	void* buffer_pagina_upcm = malloc(TAM_PAGINA);
    	void* buffer_pagina_mmap = malloc(TAM_PAGINA);
    	int frame_swap_obtenido = obtener_frame_swap_libre();

    	memcpy(buffer_pagina_upcm,&upcm[pagina_upcm->frame*TAM_PAGINA],TAM_PAGINA);

		//archivo_swap = fopen(PATH_SWAP,"r+");

		fseek(archivo_swap,frame_swap_obtenido*TAM_PAGINA,SEEK_SET);
		fwrite(buffer_pagina_upcm,TAM_PAGINA,1,archivo_swap);

		pagina_upcm->frame = frame_swap_obtenido;
    	pagina_upcm->bit_presencia = 0;
    	pagina_upcm->bit_modificado = 0;

      	pagina->frame = obtener_frame_libre();
    	pagina->bit_presencia = 1;
    	pagina->bit_usado = 1;
    	pagina->puede_pasar_por_swap = true;

    	// si este metodo no sirve, agregar un campo length_mmap en t_segmento
    	if((nro_pagina*TAM_PAGINA) <= segmento->tam_archivo_mmap){
    		//printf("nro_pagina*TAM_PAGINA %d\n",nro_pagina*TAM_PAGINA);
    		//printf("segmento->tam_archivo_mmap %d\n",segmento->tam_archivo_mmap);
    		fseek(segmento->archivo_mmap,nro_pagina*TAM_PAGINA,SEEK_SET);

    		int bytes_a_leer = (int)fmin(TAM_PAGINA,segmento->tam_archivo_mmap - (nro_pagina*TAM_PAGINA));

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
    	//fclose(archivo_swap);
	}
	else if(!pagina->bit_presencia && espacio_en_upcm()){
		//printf("El frame no esta en memoria y hay espacio\n");
		void* buffer_pagina_mmap = malloc(TAM_PAGINA);

    	pagina->frame = obtener_frame_libre();
    	pagina->bit_presencia = 1;
    	pagina->bit_usado = 1;
    	pagina->puede_pasar_por_swap = true;

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

// frames libres de upcm y swap
int obtener_cantidad_frames_disponibles(){
	int cantidad_frames_disponibles = 0;
	for(int i=0;i<cantidad_frames;i++){
		if(!bitarray_test_bit(bitmap_upcm,i)){ // retorna el primer bit q encuentre en 0
			cantidad_frames_disponibles++;
		}
	}
	for(int x=0;x<cantidad_frames_swap;x++){
		if(!bitarray_test_bit(bitmap_swap,x)){ // retorna el primer bit q encuentre en 0
			cantidad_frames_disponibles++;
		}
	}
	return cantidad_frames_disponibles;
}

bool espacio_en_upcm(){
	for(int i=0;i<cantidad_frames;i++){
		if(!bitarray_test_bit(bitmap_upcm,i)){ // retorna el primer bit q encuentre en 0
			return true;
		}
	}
	return false;
}

void liberar_frame(int numero_frame){
	bitarray_clean_bit(bitmap_upcm,numero_frame);
}

void liberar_frame_swap(int numero_frame_swap){
	bitarray_clean_bit(bitmap_swap,numero_frame_swap);
}

void eliminar_pagina(t_pagina* pagina){
	//printf("Eliminando pagina\n");
	if(pagina->bit_presencia){
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
	//printf("Se elimino la pagina\n");
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
	t_pagina* pagina_obtenida;
	int primer_frame_recorrido = algoritmo_clock_frame_recorrido;
	int nro_vuelta = 1;

	while(true){

		pagina_obtenida = list_get(lista_clock,algoritmo_clock_frame_recorrido);
		//printf("Se obtuvo pagina en el algoritmo clock modificado bu %d\tbm %d\tfr%d\n",pagina_obtenida->bit_usado,pagina_obtenida->bit_modificado,pagina_obtenida->frame);
		//printf("nro_vuelta %d\n",nro_vuelta);
		switch(nro_vuelta){
			case 1:
				if(!pagina_obtenida->bit_usado && !pagina_obtenida->bit_modificado){

					liberar_frame(algoritmo_clock_frame_recorrido);
					//list_remove(lista_clock,algoritmo_clock_frame_recorrido);

					algoritmo_clock_frame_recorrido++;
					if(algoritmo_clock_frame_recorrido == cantidad_frames){
						algoritmo_clock_frame_recorrido = 0;
					}
					return pagina_obtenida;
				}
				break;
			case 2:
				if(!pagina_obtenida->bit_usado && pagina_obtenida->bit_modificado){

					liberar_frame(algoritmo_clock_frame_recorrido);
					//list_remove(lista_clock,algoritmo_clock_frame_recorrido); no se pq no estaba comentado igual q el del case 1

					algoritmo_clock_frame_recorrido++;
					if(algoritmo_clock_frame_recorrido == cantidad_frames){
						algoritmo_clock_frame_recorrido = 0;
					}
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
		}
	}
}

void agregar_frame_clock(t_pagina* pagina){
	if(list_size(lista_clock) == cantidad_frames){
		list_replace(lista_clock,pagina->frame,pagina);
	}
	else{
		list_add_in_index(lista_clock,pagina->frame,pagina);
	}
}

void print_de_prueba(t_proceso* proceso_obtenido){
	/////////////// PRUEBA ///////////////
	t_segmento* segmento_mostrado;
	t_pagina* pagina_mostrada;
	for(int s=0; s<list_size(proceso_obtenido->tabla_segmentos); s++){
		segmento_mostrado = list_get(proceso_obtenido->tabla_segmentos,s);
		printf("segmento nro %d\t",s);
		printf("tipo: %d\t",segmento_mostrado->tipo_segmento);
		printf("base: %d\t",segmento_mostrado->base);
		printf("limite: %d\t\n",segmento_mostrado->limite);

		printf("tabla de paginas: \n");
		for(int p=0; p<list_size(segmento_mostrado->tabla_paginas); p++){
			pagina_mostrada = list_get(segmento_mostrado->tabla_paginas,p);
			printf("pagina nro %d\t",p);
			printf("bp: %d\t",pagina_mostrada->bit_presencia);
			printf("bu: %d\t",pagina_mostrada->bit_usado);
			printf("bm: %d\t",pagina_mostrada->bit_modificado);
			if(!pagina_mostrada->bit_presencia && !pagina_mostrada->puede_pasar_por_swap)
				printf("frame: -\t\n");
			else
				printf("frame: %d\t\n",pagina_mostrada->frame);
		}
	}
	printf("\n");
	//////////////////////////////////////
}
