#include "servidor.h"



void inicializarServidor(){

	printf("socketDeEscucha: %i\n", PUERTO);
	pthread_t hiloServidor;

	pthread_create(&hiloServidor,NULL,(void*)servidor, NULL);
	pthread_detach(hiloServidor);
}

void servidor(){
	void * conectado;
	int puerto_escucha = escuchar(PUERTO);

	while((conectado=aceptarConexion(puerto_escucha))!= 1){
		// agrega procesos
		//printf("Se acepto conexion\n");
		pthread_t thread_solicitud;
		pthread_create(&thread_solicitud,NULL,(void*)procesar_solicitud,conectado);
		pthread_detach(thread_solicitud);
	}
}
void procesar_solicitud(void* socket_cliente){
	t_paquete paquete = recibir_paquete(socket_cliente);
	void (*funcion_suse)(t_paquete,int);

	// por cada conexion nueva, agrego al proceso/hilo a la lista de threads

	while(paquete.error != 1){
		switch(paquete.header){
			case SUSE_CREATE:
				funcion_suse = funcion_create;
				break;
			case SUSE_SCHEDULE_NEXT:
				funcion_suse = funcion_schedule_next;
				break;
			case SUSE_WAIT:
				funcion_suse = funcion_wait;
				break;
			case SUSE_SIGNAL:
				funcion_suse = funcion_signal;
				break;
			case SUSE_JOIN:
				funcion_suse = funcion_join;
				break;
			case SUSE_CLOSE:
				funcion_suse = funcion_close;
				break;
		}

		funcion_suse(paquete,socket_cliente);

		paquete = recibir_paquete(socket_cliente);
	}

	close(socket_cliente);
}

void funcion_join(t_paquete paquete,int socket_suse){

	int tid = obtener_valor(paquete.parametros);

	int retorno = join(tid);

	t_paquete paquete_respuesta = {
		.header = SUSE_JOIN,
		.parametros = list_create()
	};

	// agregas valor al paquete de respuesta

	///////////////// Parametros a enviar /////////////////
	agregar_valor(paquete_respuesta.parametros, retorno)//lo que te devuelve la suse create si hay retorno, generalmente int);
	enviar_paquete(paquete_respuesta, socket_suse);
	///////////////////////////////////////////////////////


}

void funcion_close(t_paquete paquete,int socket_suse){

	int tid = obtener_valor(paquete.parametros);

	int retorno = close(tid);

	t_paquete paquete_respuesta = {
	.header = SUSE_CLOSE,
	.parametros = list_create()
	};

	// agregas valor al paquete de respuesta

	///////////////// Parametros a enviar /////////////////
	agregar_valor(paquete_respuesta.parametros, retorno)//lo que te devuelve la suse create si hay retorno, generalmente int);
	enviar_paquete(paquete_respuesta, socket_suse);
	///////////////////////////////////////////////////////

}

void funcion_signal(t_paquete paquete,int socket_suse){

	int tid = obtener_valor(paquete.parametros);
	char* semaforo= obtener_valor(paquete.parametros);

	//aca le das la orden a suse
	int retorno = signal(tid, semaforo);

	//el mensaje que le devuelve a hilolay (que fue el que lo llamo)

	t_paquete paquete_respuesta = {
	.header = SUSE_SIGNAL,
	.parametros = list_create()
	};

	// agregas valor al paquete de respuesta

	///////////////// Parametros a enviar /////////////////
	agregar_valor(paquete_respuesta.parametros, retorno);//lo que te devuelve la suse create si hay retorno, generalmente int);
	enviar_paquete(paquete_respuesta, socket_suse);
	///////////////////////////////////////////////////////



}



void funcion_wait(t_paquete paquete,int socket_suse){

	int tid = obtener_valor(paquete.parametros);
	char* semaforo= obtener_valor(paquete.parametros);

	//aca le das la orden a suse
	int retorno = wait(tid, semaforo);

	//el mensaje que le devuelve a hilolay (que fue el que lo llamo)

	t_paquete paquete_respuesta = {
	.header = SUSE_WAIT,
	.parametros = list_create()
	};

	// agregas valor al paquete de respuesta

	///////////////// Parametros a enviar /////////////////
	agregar_valor(paquete_respuesta.parametros, retorno)//lo que te devuelve la suse create si hay retorno, generalmente int);
	enviar_paquete(paquete_respuesta, socket_suse);
	///////////////////////////////////////////////////////


}

void funcion_create(t_paquete paquete,int socket_suse){

	//los parametros vienen en una lista, tengo que respetar ese orden al poner obtenervalor/obtenerint

	//
	int tid = obtener_valor(paquete.parametros);


	//aca le das la orden a suse

	int retorno = planificate(tid);//funcion suse)(tid); //podria tener una respuesta

	//en caso que tenga retorno int retorno = crearArchivo( path );


	//el mensaje que le devuelve a hilolay (que fue el que lo llamo)

	t_paquete paquete_respuesta = {
	.header = SUSE_CREATE,
	.parametros = list_create()
	};

	// agregas valor al paquete de respuesta

	///////////////// Parametros a enviar /////////////////
	agregar_valor(paquete_respuesta.parametros, retorno)//lo que te devuelve la suse create si hay retorno, generalmente int);
	enviar_paquete(paquete_respuesta, socket_suse);
	///////////////////////////////////////////////////////

/*	char* id_programa = string_new();
	char* ip_socket = obtener_ip_socket(socket_muse);
	string_append(&id_programa,string_itoa(obtener_valor(paquete.parametros)));
	string_append(&id_programa,"-");
	string_append(&id_programa,ip_socket);

	list_add(lista_procesos,crear_proceso(id_programa,socket_muse));

	//				PRUEBA
	/*
	t_thread* thread_obtenido;
	for(int i=0; i<list_size(lista_threads); i++){
		thread_obtenido = list_get(lista_threads,i);
		printf("hilo nro %d\t",i);
		printf("id_programa: %s\t",thread_obtenido->id_programa);
		printf("socket: %d\t\n",thread_obtenido->socket);
	}
	printf("\n");


	free(ip_socket); // Sacar si falla
	*/

}


void funcion_schedule_next(t_paquete paquete,int socket_suse){

	//los parametros vienen en una lista, tengo que respetar ese orden al poner obtenervalor/obtenerint

	//


	//aca le das la orden a suse
	//tengo que mandarle un id de programa o algo?
	int retorno= dame_prox_hilo();//funcion suse)(tid); //podria tener una respuesta

	//en caso que tenga retorno int retorno = crearArchivo( path );


	//el mensaje que le devuelve a hilolay (que fue el que lo llamo)

	t_paquete paquete_respuesta = {
	.header = SUSE_SCHEDULE_NEXT,
	.parametros = list_create()
	};

	// agregas valor al paquete de respuesta

	///////////////// Parametros a enviar /////////////////
	agregar_valor(paquete_respuesta.parametros, retorno);//lo que te devuelve la suse create si hay retorno, generalmente int);
	enviar_paquete(paquete_respuesta, socket_suse);
	///////////////////////////////////////////////////////
}


char* obtener_ip_socket(int s){
	socklen_t len;
	struct sockaddr_storage addr;
	char ipstr[INET6_ADDRSTRLEN];
	char* ipstr_reservado = malloc(sizeof(ipstr));
	//int port;

	len = sizeof addr;
	getpeername(s, (struct sockaddr*)&addr, &len);

	// deal with both IPv4 and IPv6:
	if (addr.ss_family == AF_INET) {
	    struct sockaddr_in *s = (struct sockaddr_in *)&addr;
	    //port = ntohs(s->sin_port);
	    inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);
	} else { // AF_INET6
	    struct sockaddr_in6 *s = (struct sockaddr_in6 *)&addr;
	    //port = ntohs(s->sin6_port);
	    inet_ntop(AF_INET6, &s->sin6_addr, ipstr, sizeof ipstr);
	}

	strcpy(ipstr_reservado,ipstr);

	return ipstr_reservado;
}



