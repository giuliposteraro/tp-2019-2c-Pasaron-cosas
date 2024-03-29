/*
 * segmentacionPaginada.h
 *
 *  Created on: 5 oct. 2019
 *      Author: utnso
 */

#ifndef PLUGINBABY_H_
#define PLUGINBABY_H_

#include "muse.h"

#define SEGMENTO_HEAP 0
#define SEGMENTO_MMAP 1
#define SEGMENTO_MMAP_EXISTENTE 2

#define CARGAR_DATOS 0
#define GUARDAR_DATOS 1
#define CREAR_DATOS 2

// usar estos dos si no esta agregada mman.h
#define MAP_PRIVATE 1
#define MAP_SHARED 2


typedef struct{
	char* id_programa;
	t_list* tabla_segmentos;
	int socket;
	uint32_t metrica_espacio_disponible; // espacio disponible en el ultimo segmento pedido
}t_proceso;

typedef struct{
	uint32_t base;
	uint32_t limite;
	t_list* tabla_paginas;
	uint8_t tipo_segmento;
	uint8_t tipo_map;
	FILE* archivo_mmap;
	int tam_archivo_mmap;
}t_segmento;

typedef struct{
	uint16_t frame;
	uint8_t bit_presencia;
	uint8_t bit_modificado;
	uint8_t bit_usado;
	uint8_t tipo_segmento;
	bool puede_pasar_por_swap;
}t_pagina;

typedef struct{
	FILE* archivo;
	t_list* tabla_paginas;
	t_list* sockets_procesos;
}t_archivo_mmap;

typedef struct{
	uint32_t size;
	bool isFree;
}t_heap_metadata;


t_proceso* crear_proceso(char* id_programa,int socket_creado);
uint32_t crear_segmento(uint8_t tipo,t_list* tabla_segmentos,uint32_t tam_solicitado);
t_pagina* crear_pagina(uint8_t bit_presencia,uint8_t tipo_segmento);

t_proceso* buscar_proceso(t_list* lista,int socket_proceso);
t_segmento* buscar_segmento(t_list* tabla_segmentos,uint32_t direccion);
t_segmento* obtener_segmento_disponible(t_list lista,uint32_t tam_solicitado);
uint32_t obtener_base(t_list* tabla_segmentos,uint8_t tipo_segmento,uint32_t tam_solicitado);
void* obtener_datos_frame(t_pagina* pagina);
void* obtener_datos_frame_mmap(t_segmento* segmento,t_pagina* pagina,int nro_pagina);
int obtener_frame_libre();
int obtener_frame_swap_libre();
int obtener_tam_archivo(int fd_archivo);
void cargar_datos(void* buffer,void* buffer_copia,t_segmento* segmento,uint32_t flag_operacion,int cantidad_paginas_solicitadas);
bool espacio_en_upcm();
int obtener_cantidad_frames_disponibles();

void liberar_frame(int numero_frame);
void liberar_frame_swap(int numero_frame_swap);
void eliminar_pagina(t_pagina* pagina);
void eliminar_segmento(t_segmento* segmento);
void eliminar_archivo_mmap(t_archivo_mmap* archivo_mmap);

t_pagina* ejecutar_algoritmo_clock_modificado();
void agregar_frame_clock(t_pagina* pagina);
void print_de_prueba(t_proceso* proceso_obtenido);

#endif /* PLUGINBABY_H_ */
