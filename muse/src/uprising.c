/*
 * segmentacionPaginada.c
 *
 *  Created on: 5 oct. 2019
 *      Author: utnso
 */

// Funciones para la segmentacion paginada de MUSE

#include "uprising.h"


t_thread* crear_thread(char* id_programa,int socket_creado){
	t_thread* nuevo = malloc(sizeof(t_thread));

	nuevo->id_programa = strdup(id_programa);
	nuevo->socket = socket_creado;
	nuevo->tabla_segmentos = list_create();

	return nuevo;
	//list_add(lista_threads,nuevo);
}

t_segmento* crear_segmento(uint8_t tipo) {
	 t_segmento* nuevo = malloc(sizeof(t_segmento));
	 nuevo->tipo_segmento = tipo;
	 nuevo->tabla_paginas = list_create();
	 return nuevo;
}

t_pagina* crear_pagina(uint8_t bit_modificado, void* datos) {
    t_pagina* new = malloc(sizeof(t_pagina));
    //new->numeroPagina = numeroPagina;
    new->bit_modificado = bit_modificado;
    //new->marco = guardarRegistro(registro);
    return new;
}

t_thread* buscar_thread(t_list* lista,int socket_thread) {
	int igualSocket(t_thread *p) {
		return p->socket == socket_thread;
	}

	return list_find(lista, (void*) igualSocket);
}

void reservar_espacio(t_thread* thread_solicitante,uint32_t tam,uint8_t tipo_segmento){
	t_list* tabla_segmentos_filtrada;
	t_segmento* segmento_obtenido;
	t_pagina* pagina_obtenida;
	void* direccion_datos;

	switch(tipo_segmento){
		case SEGMENTO_HEAP:

			tabla_segmentos_filtrada = list_filter(thread_solicitante->tabla_segmentos,(void*) filtrarHeap);

			for(int i=0; i<list_size(tabla_segmentos_filtrada); i++){
				segmento_obtenido = list_get(tabla_segmentos_filtrada,i);
				buscar_bloque_libre(segmento_obtenido->tabla_paginas,tam);


			}

			break;
		case SEGMENTO_MMAP:
			break;
	}

	/*
	if(!list_size(lista))
		return NULL;

	t_segmento* segmento_obtenido;
	t_list* tabla_segmentos_heap = list_filter(lista,(void*) filtrarHeap);

	for(int i=0; i<list_size(tabla_segmentos_heap); i++){
		segmento_obtenido = list_get(tabla_segmentos_heap,i);

		obtener_bloque_disponible(segmento_obtenido->tabla_paginas);

	}
	*/
}

t_desplazamiento buscar_bloque_libre(t_list* tabla_paginas,uint32_t tam){
	t_pagina* pagina_obtenida;
	void* direccion_datos;
	int posicion = 0;
	int bytes_recorridos;
	//t_desplazamiento desplazamiento;
	t_heap_metadata heap_metadata;

	for(int x=0; x<list_size(tabla_paginas); x++){
		pagina_obtenida = list_get(tabla_paginas,x);
		direccion_datos = obtener_datos_frame(pagina_obtenida);

		while(posicion <= TAM_PAGINA){
			memcpy(&heap_metadata.isFree,&direccion_datos[posicion],sizeof(heap_metadata.isFree));
			posicion += sizeof(heap_metadata.isFree);
			memcpy(&heap_metadata.size,&direccion_datos[posicion],sizeof(heap_metadata.size));
			posicion += sizeof(heap_metadata.size);

			if(heap_metadata.isFree && (tam<=heap_metadata.size)){
				t_desplazamiento desplazamiento = {
						.numero_pagina = x,
						.posicion = posicion - sizeof(heap_metadata.isFree) - sizeof(heap_metadata.size)
				};
				return desplazamiento;
			}

			posicion += heap_metadata.size;

		}
	}

	return 0;
}

void* obtener_datos_frame(t_pagina* pagina){
	// Tambien deberia chequear si el frame se encuentra en memoria o no
	int posicion_inicio_frame = pagina->frame * TAM_PAGINA;
	return (char*) upcm + posicion_inicio_frame;
}

int filtrarHeap(t_segmento* p){
	return p->tipo_segmento == SEGMENTO_HEAP;
}