/*
 * funciones.c
 *
 *  Created on: 3 oct. 2019
 *      Author: utnso
 */

// Funciones generales de MUSE

#include "madness.h"

// El socket que se crea en MUSE es diferente al creado en libmuse
// Ej.: para una misma conexion, MUSE tiene el socket 2 y libmuse el 7

void procesar_solicitud(void* socket_cliente){
	t_paquete paquete = recibir_paquete((int)socket_cliente);
	void (*funcion_muse)(t_paquete,int);

	while(paquete.error != 1){
		switch(paquete.header){
			case MUSE_INIT:
				funcion_muse = funcion_init;
				break;
			case MUSE_ALLOC:
				funcion_muse = funcion_alloc;
				break;
			case MUSE_FREE:
				funcion_muse = funcion_free;
				break;
			case MUSE_GET:
				funcion_muse = funcion_get;
				break;
			case MUSE_CPY:
				funcion_muse = funcion_cpy;
				break;
			case MUSE_MAP:
				funcion_muse = funcion_map;
				break;
			case MUSE_SYNC:
				funcion_muse = funcion_sync;
				break;
			case MUSE_UNMAP:
				funcion_muse = funcion_unmap;
				break;
			default:
				printf("Header incorrecto\n");
				return;
		}

		funcion_muse(paquete,(int)socket_cliente);

		paquete = recibir_paquete((int)socket_cliente);
	}

	// MUSE_CLOSE
	funcion_close(paquete,(int)socket_cliente);

	//close((int)socket_cliente);
}

void leer_config(){
	t_config* archivo_config = config_create(PATH_CONFIG);
	PUERTO = config_get_int_value(archivo_config,"LISTEN_PORT");
	TAM_MEMORIA = config_get_int_value(archivo_config,"MEMORY_SIZE");
	TAM_PAGINA = config_get_int_value(archivo_config,"PAGE_SIZE");
	TAM_SWAP = config_get_int_value(archivo_config,"SWAP_SIZE");
	config_destroy(archivo_config);
}

void init_memoria(){
	leer_config();
	lista_procesos = list_create();
	lista_clock = list_create();
	lista_archivos_mmap = list_create();

	archivo_log = log_create(PATH_LOG,"muse",false,LOG_LEVEL_INFO);

	t_heap_metadata heap_metadata;
	SIZE_HEAP_METADATA = sizeof(heap_metadata.isFree) + sizeof(heap_metadata.size);

	/////////////////////// UPCM ///////////////////////
	cantidad_frames = TAM_MEMORIA / TAM_PAGINA;
	int cantidad_frames_bytes = (int)ceil((double)cantidad_frames/8);
	void* frames_bitmap = malloc(cantidad_frames_bytes);
	memset(frames_bitmap,0,cantidad_frames_bytes); // seteo frames_bitmap en 0 por las dudas

	upcm = malloc(TAM_MEMORIA); // memoria principal
	bitmap_upcm = bitarray_create_with_mode(frames_bitmap,cantidad_frames_bytes,MSB_FIRST);
	////////////////////////////////////////////////////

	/////////////////////// SWAP ///////////////////////
	cantidad_frames_swap = TAM_SWAP / TAM_PAGINA;
	int cantidad_frames_swap_bytes = (int)ceil((double)cantidad_frames_swap/8);
	void* frames_swap_bitmap = malloc(cantidad_frames_swap_bytes);
	memset(frames_swap_bitmap,0,cantidad_frames_swap_bytes);

	bitmap_swap = bitarray_create_with_mode(frames_swap_bitmap,cantidad_frames_swap_bytes,MSB_FIRST);

	archivo_swap = fopen(PATH_SWAP,"w");
	void* buffer_archivo_swap = malloc(TAM_SWAP);
	memset(buffer_archivo_swap,'\0',TAM_SWAP);
	//fseek(archivo_swap,0,SEEK_SET);
	fwrite(buffer_archivo_swap,TAM_SWAP,1,archivo_swap);
	free(buffer_archivo_swap);

	fclose(archivo_swap);
	archivo_swap = fopen(PATH_SWAP,"r+");
	////////////////////////////////////////////////////

	algoritmo_clock_frame_recorrido = 0;
}

void init_threads(){
	pthread_t thread_servidor;
	pthread_create(&thread_servidor,NULL,(void*)servidor,NULL);
	pthread_detach(thread_servidor);
}

void servidor(){
	void * conectado;
	int puerto_escucha = escuchar(PUERTO);

	while((conectado=(void*)aceptarConexion(puerto_escucha))!= (void*)1){
		pthread_t thread_solicitud;
		pthread_create(&thread_solicitud,NULL,(void*)procesar_solicitud,conectado);
		pthread_detach(thread_solicitud);
	}
}


void funcion_init(t_paquete paquete,int socket_muse){

	char* id_programa = string_new();
	char* ip_socket = obtener_ip_socket(socket_muse);
	string_append(&id_programa,string_itoa(obtener_valor(paquete.parametros)));
	string_append(&id_programa,"-");
	string_append(&id_programa,ip_socket);

	pthread_mutex_lock(&mutex_lista_procesos);
	list_add(lista_procesos,crear_proceso(id_programa,socket_muse));
	pthread_mutex_unlock(&mutex_lista_procesos);

	//				PRUEBA
	t_proceso* proceso_obtenido;
	for(int i=0; i<list_size(lista_procesos); i++){
		proceso_obtenido = list_get(lista_procesos,i);
		printf("proceso nro %d\t",i);
		printf("id_programa: %s\t",proceso_obtenido->id_programa);
		printf("socket: %d\t\n",proceso_obtenido->socket);
	}
	printf("\n");

	free(ip_socket); // Sacar si falla

	t_paquete paquete_respuesta = {
			.header = MUSE_INIT,
			.parametros = list_create()
	};

	///////////////// Parametros a enviar /////////////////
	agregar_valor(paquete_respuesta.parametros,1); // solo para confirmar que la comunicacion fue exitosa
	enviar_paquete(paquete_respuesta,socket_muse);
	///////////////////////////////////////////////////////
}

void funcion_close(t_paquete paquete,int socket_muse){
	// hay un problema cuando se divide por cero, revisar
	//log_estado_del_sistema();
}

void funcion_alloc(t_paquete paquete,int socket_muse){
	printf("\nInicio alloc\n");
	uint32_t tam = obtener_valor(paquete.parametros);

	//printf("Tam solicitado: %d\n",tam);

	pthread_mutex_lock(&mutex_acceso_upcm);
	//pthread_mutex_lock(&mutex_lista_procesos);
	t_proceso* proceso_encontrado = buscar_proceso(lista_procesos,socket_muse);
	//pthread_mutex_unlock(&mutex_lista_procesos);

	t_segmento* segmento_obtenido;
	t_segmento* segmento_siguiente;
	int direccion_retornada = 0;
	int posicion_recorrida = 0;
	t_heap_metadata heap_metadata;
	bool ultimo_valor_isFree;
	void* buffer;
	void* buffer_copia;
	uint32_t tam_real = tam + SIZE_HEAP_METADATA;
	uint32_t size_original;
	bool analizar_extension = true;

	bool agregar_metadata_free = false;
	int cantidad_paginas_solicitadas;
	uint32_t segmento_limite_anterior;

	if(proceso_encontrado == NULL){
		printf("No se inicializo libmuse\n");
		return;
	}

	//printf("id_programa: %s\t",proceso_encontrado->id_programa);
	//printf("socket: %d\t\n",proceso_encontrado->socket);

	for(int numero_segmento=0;numero_segmento<list_size(proceso_encontrado->tabla_segmentos);numero_segmento++){
		segmento_obtenido = list_get(proceso_encontrado->tabla_segmentos,numero_segmento);
		if(segmento_obtenido->tipo_segmento == SEGMENTO_MMAP)
			continue;

		buffer = malloc(segmento_obtenido->limite);// si falla, probar declarando la variable aca mismo
		buffer_copia = malloc(segmento_obtenido->limite);
		posicion_recorrida = 0;

		//printf("Se cargan los datos en un buffer\n");
		cargar_datos(buffer,NULL,segmento_obtenido,CARGAR_DATOS,0);

		while(posicion_recorrida < segmento_obtenido->limite){
			memcpy(&heap_metadata.isFree,&buffer[posicion_recorrida],sizeof(heap_metadata.isFree));
			ultimo_valor_isFree = heap_metadata.isFree;
			posicion_recorrida += sizeof(heap_metadata.isFree);
			memcpy(&heap_metadata.size,&buffer[posicion_recorrida],sizeof(heap_metadata.size));

			if((heap_metadata.isFree == true) && ((heap_metadata.size == tam) || (heap_metadata.size >= tam_real))){
				// hay un espacio libre donde se puede asignar el tam solicitado pero tambien se debe indicar el espacio libre restante
				size_original = heap_metadata.size;
				heap_metadata.isFree = false;
				heap_metadata.size = tam;

				// se cambia la metadata
				posicion_recorrida -= sizeof(heap_metadata.isFree);
				memcpy(&buffer[posicion_recorrida],&heap_metadata.isFree,sizeof(heap_metadata.isFree));
				posicion_recorrida += sizeof(heap_metadata.isFree);
				memcpy(&buffer[posicion_recorrida],&heap_metadata.size,sizeof(heap_metadata.size));
				posicion_recorrida += sizeof(heap_metadata.size);

				direccion_retornada = posicion_recorrida + segmento_obtenido->base;

				if(size_original != heap_metadata.size){
					// se agrega la metadata nueva
					posicion_recorrida += heap_metadata.size;

					heap_metadata.isFree = true;
					heap_metadata.size = size_original - tam_real;
					proceso_encontrado->metrica_espacio_disponible = heap_metadata.size;

					memcpy(&buffer[posicion_recorrida],&heap_metadata.isFree,sizeof(heap_metadata.isFree));
					posicion_recorrida += sizeof(heap_metadata.isFree);
					memcpy(&buffer[posicion_recorrida],&heap_metadata.size,sizeof(heap_metadata.size));
					posicion_recorrida += sizeof(heap_metadata.size);
				}

				//posicion_recorrida += heap_metadata.size;
				analizar_extension = false;

				// se vuelven a copiar los datos en los frames correspondientes
				cargar_datos(buffer,buffer_copia,segmento_obtenido,GUARDAR_DATOS,NULL);

				free(buffer);
				free(buffer_copia);
				break;
			}

			posicion_recorrida += sizeof(heap_metadata.size) + heap_metadata.size;
		}

		//se analiza si se puede extender el segmento
		if(!analizar_extension)
			break; // salgo del ciclo for que recorre los segmentos

		// si heap_metadata.isFree es false
		uint32_t tam_auxiliar = SIZE_HEAP_METADATA + tam;
		// tam_auxiliar es el tamano restante que se guardaria en la nueva pagina

		// se revisa cual fue la ultima metadata
		if(heap_metadata.isFree == true){ // podria poner ultimo_valor_isFree
			//printf("La ultima metadata es free\n");
			tam_auxiliar = tam - heap_metadata.size;
		}

		if((tam_auxiliar%TAM_PAGINA) != 0){  // si da 0, no necesito agregar la metadata para indicar FREE
			tam_auxiliar += SIZE_HEAP_METADATA; //agrego la metadata para indicar FREE
			agregar_metadata_free = true;
		}

		cantidad_paginas_solicitadas = (int)ceil((double)tam_auxiliar/TAM_PAGINA);

		// si se solicitan mas paginas de las que hay disponibles, no hay que hacer alloc
		if(cantidad_paginas_solicitadas > obtener_cantidad_frames_disponibles())
			break;

		//printf("tam_auxiliar: %d\n",tam_auxiliar);
		//printf("cantidad paginas solicitadas: %d\n",cantidad_paginas_solicitadas);

		//en teoria, si el ultimo segmento es heap, se extiende ese segmento
		if((numero_segmento + 1) < list_size(proceso_encontrado->tabla_segmentos)){
			segmento_siguiente = list_get(proceso_encontrado->tabla_segmentos,numero_segmento + 1);

			// si no hay espacio entre los segmentos, se corta la iteracion actual para analizar el proximo segmento
			if(!((segmento_siguiente->base - (segmento_obtenido->base + segmento_obtenido->limite)) >= (cantidad_paginas_solicitadas*TAM_PAGINA))){
				continue;
			}
		}

		//se puede agrandar el segmento
		t_pagina* pagina_nueva;
		void* buffer_auxiliar = malloc(cantidad_paginas_solicitadas * TAM_PAGINA);
		void* direccion_datos_auxiliar;

		//printf("Se extiende el segmento\n");

		if(heap_metadata.isFree && (heap_metadata.size >= 0)){
			//modifico la ultima metadata solo si es free
			heap_metadata.isFree = false;
			posicion_recorrida = segmento_obtenido->limite - heap_metadata.size - sizeof(heap_metadata.size) - sizeof(heap_metadata.isFree);
			memcpy(&buffer[posicion_recorrida],&heap_metadata.isFree,sizeof(heap_metadata.isFree));
			posicion_recorrida += sizeof(heap_metadata.isFree);
			heap_metadata.size = tam;
			//printf("modifico la ultima metadata, nuevo size: %d\n",heap_metadata.size);
			memcpy(&buffer[posicion_recorrida],&heap_metadata.size,sizeof(heap_metadata.size));
			posicion_recorrida += sizeof(heap_metadata.size);

			direccion_retornada = posicion_recorrida + segmento_obtenido->base;

			//printf("direccion_retornada: %d\n",direccion_retornada);

			posicion_recorrida += heap_metadata.size;
			posicion_recorrida -= segmento_obtenido->limite;
		}
		else{
			posicion_recorrida = segmento_obtenido->limite + SIZE_HEAP_METADATA;
			direccion_retornada = posicion_recorrida + segmento_obtenido->base;
			//printf("direccion_retornada: %d\n",direccion_retornada);
			posicion_recorrida = 0;
		}

		// se vuelven a copiar los datos en los frames correspondientes
		cargar_datos(buffer,buffer_copia,segmento_obtenido,GUARDAR_DATOS,0);

		segmento_limite_anterior = segmento_obtenido->limite;
		segmento_obtenido->limite += (cantidad_paginas_solicitadas * TAM_PAGINA);

		if(!ultimo_valor_isFree){
			// agrego la nueva metadata para indicar el espacio usado
			heap_metadata.isFree = false;
			heap_metadata.size = tam;

			memcpy(&buffer_auxiliar[posicion_recorrida],&heap_metadata.isFree,sizeof(heap_metadata.isFree));
			posicion_recorrida += sizeof(heap_metadata.isFree);

			memcpy(&buffer_auxiliar[posicion_recorrida],&heap_metadata.size,sizeof(heap_metadata.size));
			posicion_recorrida += sizeof(heap_metadata.size) + heap_metadata.size;
		}

		if(agregar_metadata_free){
			heap_metadata.isFree = true;
			heap_metadata.size = segmento_obtenido->limite - segmento_limite_anterior - posicion_recorrida - sizeof(heap_metadata.isFree) - sizeof(heap_metadata.size);
			proceso_encontrado->metrica_espacio_disponible = heap_metadata.size;

			memcpy(&buffer_auxiliar[posicion_recorrida],&heap_metadata.isFree,sizeof(heap_metadata.isFree));
			posicion_recorrida += sizeof(heap_metadata.isFree);

			memcpy(&buffer_auxiliar[posicion_recorrida],&heap_metadata.size,sizeof(heap_metadata.size));
		}

		// se copian los datos en los nuevos frames
		cargar_datos(buffer_auxiliar,NULL,segmento_obtenido,CREAR_DATOS,cantidad_paginas_solicitadas);

		free(buffer_auxiliar);
		free(buffer);
		free(buffer_copia);
	}// termina for

	//si no hay ningun segmento creado o no hay espacio en uno ya creado, se crea uno nuevo
	if(direccion_retornada == NULL){
		direccion_retornada = crear_segmento(SEGMENTO_HEAP,proceso_encontrado->tabla_segmentos,tam);

		if(direccion_retornada >= 0){
			// obtengo el segmento para las metricas
			segmento_obtenido = buscar_segmento(proceso_encontrado->tabla_segmentos,direccion_retornada);
			proceso_encontrado->metrica_espacio_disponible = segmento_obtenido->limite - tam - (2*SIZE_HEAP_METADATA);
		}
	}

	pthread_mutex_unlock(&mutex_acceso_upcm);

	//print_de_prueba(proceso_encontrado);

	t_paquete paquete_respuesta = {
			.header = MUSE_ALLOC,
			.parametros = list_create()
	};

	///////////////// Parametros a enviar /////////////////
	agregar_valor(paquete_respuesta.parametros,direccion_retornada);
	enviar_paquete(paquete_respuesta,socket_muse);
	///////////////////////////////////////////////////////

	//log_estado_del_sistema();
	printf("Fin alloc\n");
}

void funcion_free(t_paquete paquete,int socket_muse){

	uint32_t direccion_recibida = obtener_valor(paquete.parametros);

	// revisar si la busqueda de proceso se puede hacer directamente en procesar solicitud
	pthread_mutex_lock(&mutex_acceso_upcm);
	//pthread_mutex_lock(&mutex_lista_procesos);
	t_proceso* proceso_encontrado = buscar_proceso(lista_procesos,socket_muse);
	//pthread_mutex_unlock(&mutex_lista_procesos);

	//pthread_mutex_lock(&mutex_acceso_upcm);
	// cuidado con esto, si se elimina proceso_encontrado, pierdo la referencia a _todo???
	t_segmento* segmento_obtenido = buscar_segmento(proceso_encontrado->tabla_segmentos,direccion_recibida);
	// si no se encuentra el segmento, deberia controlar el error
	void* buffer = malloc(segmento_obtenido->limite);
	void* buffer_copia = malloc(segmento_obtenido->limite);
	t_heap_metadata heap_metadata;
	t_heap_metadata heap_metadata_anterior;
	int posicion_recorrida = 0;

	cargar_datos(buffer,NULL,segmento_obtenido,CARGAR_DATOS,NULL);

	posicion_recorrida = direccion_recibida - segmento_obtenido->base - SIZE_HEAP_METADATA;
	memcpy(&heap_metadata.isFree,&buffer[posicion_recorrida],sizeof(heap_metadata.isFree));
	//posicion_recorrida += sizeof(heap_metadata.isFree);

	if(heap_metadata.isFree == false){
		// compruebo si el espacio que voy a liberar fue asignado anteriormente, o sea, hubo un muse_alloc antes

	}

	heap_metadata.isFree = true;
	memcpy(&buffer[posicion_recorrida],&heap_metadata.isFree,sizeof(heap_metadata.isFree));
	posicion_recorrida += sizeof(heap_metadata.isFree);
	memcpy(&heap_metadata.size,&buffer[posicion_recorrida],sizeof(heap_metadata.size));
	posicion_recorrida += sizeof(heap_metadata.size);

	memset(&buffer[posicion_recorrida],NULL,heap_metadata.size);

	// comienzo a revisar _todo el segmento para ver si tengo que unificar espacios libres
	posicion_recorrida = 0;
	heap_metadata_anterior.isFree = false; // con esto indico q aun no se inicializo
	while(posicion_recorrida < segmento_obtenido->limite){
		memcpy(&heap_metadata.isFree,&buffer[posicion_recorrida],sizeof(heap_metadata.isFree));
		posicion_recorrida += sizeof(heap_metadata.isFree);
		memcpy(&heap_metadata.size,&buffer[posicion_recorrida],sizeof(heap_metadata.size));

		if((heap_metadata_anterior.isFree == true) && (heap_metadata.isFree == true)){
			//printf("La metadata actual y la anterior estan libres\n");
			posicion_recorrida -= sizeof(heap_metadata.isFree);
			memset(&buffer[posicion_recorrida],NULL,SIZE_HEAP_METADATA);
			//printf("posicion_recorrida : %d \n",posicion_recorrida);

			//printf("heap_metadata_anterior.size : %d \n",heap_metadata_anterior.size);
			posicion_recorrida = posicion_recorrida - heap_metadata_anterior.size - sizeof(heap_metadata.size);
			//printf("posicion_recorrida : %d \n",posicion_recorrida);
			heap_metadata.size += heap_metadata_anterior.size + SIZE_HEAP_METADATA;
			memcpy(&buffer[posicion_recorrida],&heap_metadata.size,sizeof(heap_metadata.size));

			//printf("Nuevo espacio libre: %d bytes\n",heap_metadata.size);
		}

		//printf("Igualo la metadata a la variable anterior\n");
		heap_metadata_anterior.isFree = heap_metadata.isFree;
		heap_metadata_anterior.size = heap_metadata.size;

		posicion_recorrida += sizeof(heap_metadata.size) + heap_metadata.size;
		//printf("posicion_recorrida : %d \n",posicion_recorrida);
	}

	int cantidad_paginas_libres = (int)floor((double)heap_metadata.size/TAM_PAGINA);
	if((heap_metadata.isFree == true) && (cantidad_paginas_libres>0)){
		// analizo si el ultimo espacio esta libre para eliminar
		// las paginas que no esten siendo usadas

		//printf("Elimino paginas libres\n");

		int cantidad_paginas_actual = list_size(segmento_obtenido->tabla_paginas);

		for(int pagina_recorrida = cantidad_paginas_actual - 1; pagina_recorrida >= (cantidad_paginas_actual - cantidad_paginas_libres); pagina_recorrida--){
			// hay 1 o mas paginas libres sin usar
			// no hace falta hacer memset ya que se hizo al momento de realizar el free
			list_remove_and_destroy_element(segmento_obtenido->tabla_paginas,pagina_recorrida,(void*) eliminar_pagina);
			segmento_obtenido->limite -= TAM_PAGINA;
		}

		//printf("heap_metadata.size: %d\n",heap_metadata.size);

		heap_metadata.size -= cantidad_paginas_libres*TAM_PAGINA;
		posicion_recorrida = segmento_obtenido->limite - heap_metadata.size - sizeof(heap_metadata.size);

		//printf("Nuevo heap_metadata.size: %d\n",heap_metadata.size);
		//printf("posicion_recorrida: %d\n",posicion_recorrida);
		memcpy(&buffer[posicion_recorrida],&heap_metadata.size,sizeof(heap_metadata.size));
	}

	cargar_datos(buffer,buffer_copia,segmento_obtenido,GUARDAR_DATOS,NULL);

	pthread_mutex_unlock(&mutex_acceso_upcm);

	free(buffer);

	//print_de_prueba(proceso_encontrado);

	t_paquete paquete_respuesta = {
			.header = MUSE_FREE,
			.parametros = list_create()
	};

	///////////////// Parametros a enviar /////////////////
	agregar_valor(paquete_respuesta.parametros,1);
	enviar_paquete(paquete_respuesta,socket_muse);
	///////////////////////////////////////////////////////
}

void funcion_get(t_paquete paquete,int socket_muse){
	printf("\nInicio get\n");
	uint32_t resultado_get = 1;

	uint32_t direccion_recibida = obtener_valor(paquete.parametros);
	uint32_t tam_bloque_datos_a_enviar = obtener_valor(paquete.parametros);
	void* bloque_datos_a_enviar = malloc(tam_bloque_datos_a_enviar);

	pthread_mutex_lock(&mutex_acceso_upcm);

	t_proceso* proceso_obtenido = buscar_proceso(lista_procesos,socket_muse);
	t_segmento* segmento_obtenido = buscar_segmento(proceso_obtenido->tabla_segmentos,direccion_recibida);

	if((segmento_obtenido == NULL) || ((segmento_obtenido->base + segmento_obtenido->limite) < (direccion_recibida + tam_bloque_datos_a_enviar))){
		// no se encontro segmento
		pthread_mutex_unlock(&mutex_acceso_upcm);

		t_paquete paquete_respuesta = {
				.header = MUSE_GET,
				.parametros = list_create()
		};

		resultado_get = 2;
		///////////////// Parametros a enviar /////////////////
		agregar_valor(paquete_respuesta.parametros,resultado_get);
		enviar_paquete(paquete_respuesta,socket_muse);
		///////////////////////////////////////////////////////

		return;
	}
	printf("direccion_recibida %d\n",direccion_recibida);
	int nro_pagina_obtenida = (direccion_recibida - segmento_obtenido->base) / TAM_PAGINA;
	printf("nro_pagina_obtenida %d\n",nro_pagina_obtenida);
	int desplazamiento_obtenido = (direccion_recibida - segmento_obtenido->base) - (nro_pagina_obtenida * TAM_PAGINA);
	printf("desplazamiento_obtenido %d\n",desplazamiento_obtenido);

	// creo que no es necesario este list_get
	t_pagina* pagina_obtenida;

	void* direccion_datos;
	t_heap_metadata heap_metadata;
	int posicion_recorrida = desplazamiento_obtenido;
	void* buffer;

	// calculo para las paginas necesarias
	int cantidad_paginas_necesarias = (int)ceil((double)(desplazamiento_obtenido + tam_bloque_datos_a_enviar)/TAM_PAGINA);

	printf("cantidad_paginas_necesarias %d\n",cantidad_paginas_necesarias);

	buffer = malloc(cantidad_paginas_necesarias*TAM_PAGINA);
	for(int i=0; i<cantidad_paginas_necesarias;i++){
		pagina_obtenida = list_get(segmento_obtenido->tabla_paginas,i + nro_pagina_obtenida);
		if(segmento_obtenido->tipo_segmento == SEGMENTO_HEAP){
			direccion_datos = obtener_datos_frame(pagina_obtenida);
		}
		else if((segmento_obtenido->tipo_segmento == SEGMENTO_MMAP) && pagina_obtenida->puede_pasar_por_swap){
			direccion_datos = obtener_datos_frame(pagina_obtenida);
		}
		else if((segmento_obtenido->tipo_segmento == SEGMENTO_MMAP) && !pagina_obtenida->puede_pasar_por_swap){
			direccion_datos = obtener_datos_frame_mmap(segmento_obtenido,pagina_obtenida,i + nro_pagina_obtenida);
		}

		memcpy(&buffer[TAM_PAGINA*i],direccion_datos,TAM_PAGINA);
	}

	// obtengo los datos solicitados
	memcpy(bloque_datos_a_enviar,&buffer[desplazamiento_obtenido],tam_bloque_datos_a_enviar);

	//printf("bloque_datos antes de ser enviado: %s\n",bloque_datos_a_enviar);

	free(buffer);
	pthread_mutex_unlock(&mutex_acceso_upcm);

	//print_de_prueba(proceso_obtenido);

	t_paquete paquete_respuesta = {
			.header = MUSE_GET,
			.parametros = list_create()
	};

	///////////////// Parametros a enviar /////////////////
	agregar_valor(paquete_respuesta.parametros,resultado_get);
	agregar_bloque_datos(paquete_respuesta.parametros,bloque_datos_a_enviar,tam_bloque_datos_a_enviar);
	enviar_paquete(paquete_respuesta,socket_muse);
	///////////////////////////////////////////////////////

	printf("Fin get\n");
}

void funcion_cpy(t_paquete paquete,int socket_muse){
	printf("\nInicio cpy\n");
	uint32_t resultado_cpy = 1;

	uint32_t direccion_recibida = obtener_valor(paquete.parametros);
	uint32_t tam_bloque_datos_recibido = obtener_valor(paquete.parametros);
	void* bloque_datos_recibido = obtener_bloque_datos(paquete.parametros);

	//printf("bloque_datos_recibido %s\n",bloque_datos_recibido);

	pthread_mutex_lock(&mutex_acceso_upcm);

	t_proceso* proceso_obtenido = buscar_proceso(lista_procesos,socket_muse);
	t_segmento* segmento_obtenido = buscar_segmento(proceso_obtenido->tabla_segmentos,direccion_recibida);

	if(segmento_obtenido == NULL){
		// no se encontro segmento
		t_paquete paquete_respuesta = {
				.header = MUSE_CPY,
				.parametros = list_create()
		};

		resultado_cpy = 2;

		///////////////// Parametros a enviar /////////////////
		agregar_valor(paquete_respuesta.parametros,resultado_cpy); // indico que debe producirse segmentation fault
		enviar_paquete(paquete_respuesta,socket_muse);
		///////////////////////////////////////////////////////

		pthread_mutex_unlock(&mutex_acceso_upcm);

		return;
	}

	int nro_pagina_obtenida = (direccion_recibida - segmento_obtenido->base) / TAM_PAGINA;
	//printf("nro_pagina_obtenida %d\n",nro_pagina_obtenida);
	int desplazamiento_obtenido = (direccion_recibida - segmento_obtenido->base) - (nro_pagina_obtenida * TAM_PAGINA);
	//printf("desplazamiento_obtenido %d\n",desplazamiento_obtenido);

	t_pagina* pagina_obtenida;
	void* direccion_datos;
	t_heap_metadata heap_metadata;
	void* buffer;
	int posicion_recorrida = desplazamiento_obtenido - SIZE_HEAP_METADATA;

	// calculo para las paginas necesarias
	int cantidad_paginas_necesarias = (int)ceil((double)(desplazamiento_obtenido + tam_bloque_datos_recibido)/TAM_PAGINA);
	//int cantidad_paginas_necesarias;

	if((segmento_obtenido->tipo_segmento == SEGMENTO_HEAP) && (posicion_recorrida < 0)){ // revisar esto para un mmap
		// tengo que obtener la pagina anterior para poder manejar la metadata
		int cantidad_paginas_necesarias_auxiliares = (int)ceil((double)abs(posicion_recorrida)/TAM_PAGINA);
		cantidad_paginas_necesarias += cantidad_paginas_necesarias_auxiliares;
		nro_pagina_obtenida -= cantidad_paginas_necesarias_auxiliares;
		posicion_recorrida = (cantidad_paginas_necesarias_auxiliares*TAM_PAGINA) + desplazamiento_obtenido - SIZE_HEAP_METADATA;
	}

	// controlo que la cant de pagina necesarias no supere a la cantidad de paginas reales
	cantidad_paginas_necesarias = (int)fminf((float)cantidad_paginas_necesarias,(float)list_size(segmento_obtenido->tabla_paginas));
	//printf("cantidad_paginas_necesarias %d\n",cantidad_paginas_necesarias);

	buffer = malloc(cantidad_paginas_necesarias*TAM_PAGINA);
	for(int i=0; i<cantidad_paginas_necesarias;i++){
		pagina_obtenida = list_get(segmento_obtenido->tabla_paginas,i + nro_pagina_obtenida);
		if(segmento_obtenido->tipo_segmento == SEGMENTO_HEAP){
			direccion_datos = obtener_datos_frame(pagina_obtenida);
		}
		else{
			direccion_datos = obtener_datos_frame_mmap(segmento_obtenido,pagina_obtenida,i + nro_pagina_obtenida);
		}
		memcpy(&buffer[TAM_PAGINA*i],direccion_datos,TAM_PAGINA);
	}

	// creo una copia del buffer para luego comparar los frames modificados
	void* buffer_copia = malloc(cantidad_paginas_necesarias*TAM_PAGINA);
	memcpy(buffer_copia,buffer,cantidad_paginas_necesarias*TAM_PAGINA);

	switch(segmento_obtenido->tipo_segmento){
		case SEGMENTO_HEAP:
			// obtengo la metadata
			memcpy(&heap_metadata.isFree,&buffer[posicion_recorrida],sizeof(heap_metadata.isFree));
			posicion_recorrida += sizeof(heap_metadata.isFree);
			memcpy(&heap_metadata.size,&buffer[posicion_recorrida],sizeof(heap_metadata.size));
			posicion_recorrida += sizeof(heap_metadata.size);

			if(!heap_metadata.isFree && (heap_metadata.size >= tam_bloque_datos_recibido)){
				memcpy(&buffer[posicion_recorrida],bloque_datos_recibido,tam_bloque_datos_recibido);

				// vuelvo a cargar los datos al upcm
				for(int x=0; x<cantidad_paginas_necesarias;x++){
					pagina_obtenida = list_get(segmento_obtenido->tabla_paginas,x + nro_pagina_obtenida);
					direccion_datos = obtener_datos_frame(pagina_obtenida);
					memcpy(direccion_datos,&buffer[TAM_PAGINA*x],TAM_PAGINA);

					if(memcmp(&buffer[TAM_PAGINA*x],&buffer_copia[TAM_PAGINA*x],TAM_PAGINA) != 0){
						pagina_obtenida->bit_modificado = 1;
					}
				}
			}
			else{
				// no puedo almacenar los datos pq ingreso a una posicion invalida
				resultado_cpy = 2;
			}
			break;
		case SEGMENTO_MMAP:
			if((desplazamiento_obtenido + (cantidad_paginas_necesarias*TAM_PAGINA)) >= tam_bloque_datos_recibido){
				memcpy(&buffer[desplazamiento_obtenido],bloque_datos_recibido,tam_bloque_datos_recibido);

				// vuelvo a cargar los datos al upcm
				for(int x=0; x<cantidad_paginas_necesarias;x++){
					pagina_obtenida = list_get(segmento_obtenido->tabla_paginas,x + nro_pagina_obtenida);
					direccion_datos = obtener_datos_frame_mmap(segmento_obtenido,pagina_obtenida,x + nro_pagina_obtenida);
					memcpy(direccion_datos,&buffer[TAM_PAGINA*x],TAM_PAGINA);

					if(memcmp(&buffer[TAM_PAGINA*x],&buffer_copia[TAM_PAGINA*x],TAM_PAGINA) != 0){
						pagina_obtenida->bit_modificado = 1;
					}
				}
			}
			else{
				// no puedo almacenar los datos pq ingreso a una posicion invalida
				//printf("no puedo almacenar los datos pq ingreso a una posicion invalida\n");
				resultado_cpy = 2;
			}
			break;
	}

	free(buffer);
	free(buffer_copia);
	pthread_mutex_unlock(&mutex_acceso_upcm);

	//print_de_prueba(proceso_obtenido);

	t_paquete paquete_respuesta = {
			.header = MUSE_CPY,
			.parametros = list_create()
	};

	///////////////// Parametros a enviar /////////////////
	agregar_valor(paquete_respuesta.parametros,resultado_cpy);
	enviar_paquete(paquete_respuesta,socket_muse);
	///////////////////////////////////////////////////////

	printf("Fin cpy\n");
}

void funcion_map(t_paquete paquete,int socket_muse){
	printf("\nInicio map\n");
	char* path_recibido = obtener_string(paquete.parametros);
	uint32_t length_recibido = obtener_valor(paquete.parametros);
	uint8_t flag_recibido = obtener_valor(paquete.parametros);

	pthread_mutex_lock(&mutex_acceso_upcm);

	t_proceso* proceso_encontrado = buscar_proceso(lista_procesos,socket_muse);

	t_segmento* segmento_nuevo;
	int direccion_retornada = NULL;

	if(proceso_encontrado == NULL){
		printf("No se inicializo libmuse\n");
		return;
	}

	printf("id_programa: %s\t",proceso_encontrado->id_programa);
	printf("socket: %d\t\n",proceso_encontrado->socket);

	FILE* archivo_solicitado = fopen(path_recibido,"r+");

	// si falla, se tiene que crear el archivo con el length solicitado
	if(archivo_solicitado == NULL){
		printf("No existe el archivo, se procede a crearlo\n");
		archivo_solicitado = fopen(path_recibido,"w+");
		printf("path_recibido %s\n",path_recibido);
		if(archivo_solicitado == NULL){
			printf("Error: no se pudo crear el archivo\n");
			pthread_mutex_lock(&mutex_acceso_upcm);
		}
		void* buffer_archivo_vacio = malloc(length_recibido);
		memset(buffer_archivo_vacio,'\0',length_recibido);
		fwrite(buffer_archivo_vacio,length_recibido,1,archivo_solicitado);
		fclose(archivo_solicitado);
		free(buffer_archivo_vacio);

		archivo_solicitado = fopen(path_recibido,"r+");
	}


	int fd_archivo_solicitado = fileno(archivo_solicitado);
	t_archivo_mmap* archivo_mmap_encontrado = buscar_archivo_mmap(fd_archivo_solicitado);

	switch(flag_recibido){
		case MAP_SHARED:
			if(archivo_mmap_encontrado == NULL){
				// aun no se mapeo el archivo
				direccion_retornada = crear_segmento(SEGMENTO_MMAP,proceso_encontrado->tabla_segmentos,length_recibido);

				if(direccion_retornada >= 0){
					segmento_nuevo = buscar_segmento(proceso_encontrado->tabla_segmentos,direccion_retornada);
					segmento_nuevo->archivo_mmap = archivo_solicitado;
					segmento_nuevo->tam_archivo_mmap = obtener_tam_archivo(fd_archivo_solicitado);
					agregar_archivo_mmap(archivo_solicitado,socket_muse,segmento_nuevo->tabla_paginas);
				}
			}
			else{
				// ya se mapeo el archivo anteriormente
				direccion_retornada = crear_segmento(SEGMENTO_MMAP_EXISTENTE,proceso_encontrado->tabla_segmentos,length_recibido);

				if(direccion_retornada >= 0){
					segmento_nuevo = buscar_segmento(proceso_encontrado->tabla_segmentos,direccion_retornada);
					segmento_nuevo->tabla_paginas = archivo_mmap_encontrado->tabla_paginas;
					segmento_nuevo->archivo_mmap = archivo_mmap_encontrado->archivo;
					segmento_nuevo->tam_archivo_mmap = obtener_tam_archivo(fd_archivo_solicitado);
					list_add(archivo_mmap_encontrado->sockets_procesos,socket_muse);
					fclose(archivo_solicitado);
				}
			}
			segmento_nuevo->tipo_map = MAP_SHARED;
			break;
		case MAP_PRIVATE:
			printf("Map private\n");
			direccion_retornada = crear_segmento(SEGMENTO_MMAP,proceso_encontrado->tabla_segmentos,length_recibido);
			//printf("direccion_retornada %d\n",direccion_retornada);
			if(direccion_retornada >= 0){
				segmento_nuevo = buscar_segmento(proceso_encontrado->tabla_segmentos,direccion_retornada);
				segmento_nuevo->tam_archivo_mmap = obtener_tam_archivo(fd_archivo_solicitado);
				if(archivo_mmap_encontrado == NULL){
					// aun no se mapeo el archivo
					segmento_nuevo->archivo_mmap = archivo_solicitado;
					agregar_archivo_mmap(archivo_solicitado,socket_muse,NULL); // no mando la tabla de paginas ya que no se comparte
				}
				else{
					// ya se mapeo el archivo anteriormente
					segmento_nuevo->archivo_mmap = archivo_mmap_encontrado->archivo;
					list_add(archivo_mmap_encontrado->sockets_procesos,socket_muse);
					fclose(archivo_solicitado);
				}
				segmento_nuevo->tipo_map = MAP_PRIVATE;
			}
			break;
	}

	pthread_mutex_unlock(&mutex_acceso_upcm);

	//print_de_prueba(proceso_encontrado);

	t_paquete paquete_respuesta = {
			.header = MUSE_MAP,
			.parametros = list_create()
	};

	///////////////// Parametros a enviar /////////////////
	agregar_valor(paquete_respuesta.parametros,direccion_retornada);
	enviar_paquete(paquete_respuesta,socket_muse);
	///////////////////////////////////////////////////////

	printf("Fin map\n");
}

void funcion_sync(t_paquete paquete,int socket_muse){

	uint32_t resultado_sync = 1;
	printf("\nInicio muse_sync\n");

	uint32_t direccion_recibida = obtener_valor(paquete.parametros);
	uint32_t length_recibido = obtener_valor(paquete.parametros);

	pthread_mutex_lock(&mutex_acceso_upcm);

	t_proceso* proceso_obtenido = buscar_proceso(lista_procesos,socket_muse);
	t_segmento* segmento_obtenido = buscar_segmento(proceso_obtenido->tabla_segmentos,direccion_recibida);

	// calculo para las paginas necesarias
	int cantidad_paginas_necesarias = (int)ceil((double)length_recibido/TAM_PAGINA);
	//printf("cantidad_paginas_necesarias %d\n",cantidad_paginas_necesarias);

	if(segmento_obtenido == NULL){
		resultado_sync = 2; // indico que debe producirse segmentation fault
	}
	else if(cantidad_paginas_necesarias > list_size(segmento_obtenido->tabla_paginas)){
		resultado_sync = 2; // indico que debe producirse segmentation fault
	}
	else if((segmento_obtenido->tipo_segmento != SEGMENTO_MMAP) || (direccion_recibida%TAM_PAGINA != 0)){
		resultado_sync = 3; // indico que debe retornar -1
	}

	if(resultado_sync > 1){
		// no se encontro segmento
		t_paquete paquete_respuesta = {
				.header = MUSE_SYNC,
				.parametros = list_create()
		};

		///////////////// Parametros a enviar /////////////////
		agregar_valor(paquete_respuesta.parametros,resultado_sync);
		enviar_paquete(paquete_respuesta,socket_muse);
		///////////////////////////////////////////////////////

		pthread_mutex_unlock(&mutex_acceso_upcm);

		return;
	}

	int nro_pagina_obtenida = (direccion_recibida - segmento_obtenido->base) / TAM_PAGINA;

	t_pagina* pagina_obtenida;
	void* direccion_datos;
	int posicion_recorrida;
	void* buffer;

	buffer = malloc(cantidad_paginas_necesarias*TAM_PAGINA);
	for(int i=0; i<cantidad_paginas_necesarias;i++){
		pagina_obtenida = list_get(segmento_obtenido->tabla_paginas,i + nro_pagina_obtenida);
		if(pagina_obtenida->puede_pasar_por_swap)
			direccion_datos = obtener_datos_frame(pagina_obtenida);
		else
			direccion_datos = obtener_datos_frame_mmap(segmento_obtenido,pagina_obtenida,i + nro_pagina_obtenida);
		memcpy(&buffer[TAM_PAGINA*i],direccion_datos,TAM_PAGINA);
	}

	//printf("Buffer antes de sync %s\n",buffer);
	//pthread_mutex_lock(&mutex_acceso_upcm);

	if((nro_pagina_obtenida*TAM_PAGINA) <= segmento_obtenido->tam_archivo_mmap){
		fseek(segmento_obtenido->archivo_mmap,nro_pagina_obtenida*TAM_PAGINA,SEEK_SET);
		int bytes_a_escribir = (int)fmin(length_recibido,segmento_obtenido->tam_archivo_mmap);
		fwrite(buffer,bytes_a_escribir,1,segmento_obtenido->archivo_mmap);
	}
	else{
		// el primer byte a escribir supera el tamano del archivo
		resultado_sync = 3;
	}

	free(buffer);
	pthread_mutex_unlock(&mutex_acceso_upcm);

	//print_de_prueba(proceso_obtenido);

	t_paquete paquete_respuesta = {
			.header = MUSE_SYNC,
			.parametros = list_create()
	};

	///////////////// Parametros a enviar /////////////////
	agregar_valor(paquete_respuesta.parametros,resultado_sync);
	enviar_paquete(paquete_respuesta,socket_muse);
	///////////////////////////////////////////////////////

	printf("Fin muse_sync\n");
}

void funcion_unmap(t_paquete paquete,int socket_muse){
	uint32_t resultado_unmap = 1;
	printf("Inicio unmap\n");
	uint32_t direccion_recibida = obtener_valor(paquete.parametros);

	pthread_mutex_lock(&mutex_acceso_upcm);

	t_proceso* proceso_encontrado = buscar_proceso(lista_procesos,socket_muse);
	t_segmento* segmento_obtenido = buscar_segmento(proceso_encontrado->tabla_segmentos,direccion_recibida);

	if(segmento_obtenido == NULL){
		resultado_unmap = 2; // indico que debe producirse segmentation fault
	}
	else if((segmento_obtenido->tipo_segmento != SEGMENTO_MMAP) || (direccion_recibida != segmento_obtenido->base)){
		resultado_unmap = 3; // indico que debe retornar -1
	}

	if(resultado_unmap > 1){
		t_paquete paquete_respuesta = {
				.header = MUSE_UNMAP,
				.parametros = list_create()
		};

		///////////////// Parametros a enviar /////////////////
		agregar_valor(paquete_respuesta.parametros,resultado_unmap);
		enviar_paquete(paquete_respuesta,socket_muse);
		///////////////////////////////////////////////////////

		pthread_mutex_unlock(&mutex_acceso_upcm);

		return;
	}

	int fd_archivo_segmento_mmap = fileno(segmento_obtenido->archivo_mmap);
	t_archivo_mmap* archivo_mmap_encontrado = buscar_archivo_mmap(fd_archivo_segmento_mmap);

	// funcion auxiliar
	int igualSocket(int p) {
		return p == proceso_encontrado->socket;
	}
	list_remove_by_condition(archivo_mmap_encontrado->sockets_procesos,(void*) igualSocket);

	if(list_size(archivo_mmap_encontrado->sockets_procesos) == 0){
		//printf("Se elimina el archivo mmap\n");
		// funcion auxiliar
		int igualArchivo(t_archivo_mmap* archivo_mmap) {
		    struct stat stat1, stat2;
		    if((fstat(fd_archivo_segmento_mmap, &stat1) < 0) || (fstat(fileno(archivo_mmap->archivo), &stat2) < 0)) return -1;
		    return (stat1.st_dev == stat2.st_dev) && (stat1.st_ino == stat2.st_ino);
		}
		list_remove_and_destroy_by_condition(lista_archivos_mmap,(void*) igualArchivo,(void*) eliminar_archivo_mmap);
	}

	// funcion auxiliar
	int igualBaseSegmento(t_segmento* p){
		return p->base == segmento_obtenido->base;
	}
	list_remove_and_destroy_by_condition(proceso_encontrado->tabla_segmentos,(void*) igualBaseSegmento,(void*) eliminar_segmento);

	pthread_mutex_unlock(&mutex_acceso_upcm);

	//print_de_prueba(proceso_encontrado);

	t_paquete paquete_respuesta = {
			.header = MUSE_UNMAP,
			.parametros = list_create()
	};

	///////////////// Parametros a enviar /////////////////
	agregar_valor(paquete_respuesta.parametros,resultado_unmap);
	enviar_paquete(paquete_respuesta,socket_muse);
	///////////////////////////////////////////////////////
}


char* obtener_ip_socket(int s){
	socklen_t len;
	struct sockaddr_storage addr;
	char ipstr[INET6_ADDRSTRLEN];
	char* ipstr_reservado = malloc(sizeof(ipstr));

	len = sizeof addr;
	getpeername(s, (struct sockaddr*)&addr, &len);

	// deal with both IPv4 and IPv6:
	if(addr.ss_family == AF_INET){
	    struct sockaddr_in *s = (struct sockaddr_in *)&addr;
	    inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);
	}
	else{ // AF_INET6
		struct sockaddr_in6 *s = (struct sockaddr_in6 *)&addr;
	    inet_ntop(AF_INET6, &s->sin6_addr, ipstr, sizeof ipstr);
	}

	strcpy(ipstr_reservado,ipstr);

	return ipstr_reservado;
}

void log_estado_del_sistema(){
	pthread_mutex_lock(&mutex_acceso_upcm);

	// Log de los sockets
	t_proceso* proceso_obtenido;
	int cantidad_total_segmentos = 0;
	int porcentaje_asignacion_memoria;
	for(int i=0; i<list_size(lista_procesos); i++){
		proceso_obtenido = list_get(lista_procesos,i);
		cantidad_total_segmentos += list_size(proceso_obtenido->tabla_segmentos);
	}
	log_info(archivo_log,"\t\tSockets");
	log_info(archivo_log,"Socket\t\tPorcentaje asignacion\tEspacio disponible");
	for(int i=0; i<list_size(lista_procesos); i++){
		proceso_obtenido = list_get(lista_procesos,i);
		porcentaje_asignacion_memoria = (list_size(proceso_obtenido->tabla_segmentos)/cantidad_total_segmentos)*100;
		// el nro 23 es solo de prueba
		log_info(archivo_log,"%d\t\t%d\t\t\t%d",proceso_obtenido->socket,porcentaje_asignacion_memoria,proceso_obtenido->metrica_espacio_disponible);
	}

	log_info(archivo_log,"\n");

	pthread_mutex_unlock(&mutex_acceso_upcm);
}
