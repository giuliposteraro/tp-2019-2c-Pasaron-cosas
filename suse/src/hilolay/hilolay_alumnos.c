/*
 * hilolay_alumnos.c
 *
 *  Created on: 12 nov. 2019
 *      Author: utnso
 */





// en hilolay_init abro la conexión del socket

 	 void hilolay_init(){
 		 socket_suse = conectarseA(ip,puerto);
 		 	 if(socket_suse == 0){
 		 		 return -1;
 		 	 }
 		 init_internal(hilolay_operations);

	}


	typedef struct hilolay_operations {
		.suse_create= me_create,
		.suse_schedule_next = me_schedule_next,
		.suse_wait = me_wait,
		.suse_signal = me_signal,
		.suse_join= me_join,
		.suse_close = me_close,
	} hilolay_operations;


// le paso el tid y el id del semaforo a suse
	int me_wait(int , char *){

	}

	int me_signal(int, char *){

	}

	int me_join(int){

	}

	int me_create(int tid){

		t_paquete paquete_respuesta = {
				.header = SUSE_CREATE,
				.parametros = list_create()
		};

		agregar_valor(paquete_respuesta.parametros,tid);

		enviar_paquete(paquete_respuesta,socket) //el que guarde al inciiar la conex)

		t_paquete paquete_respuesta = recibir_paquete(socket);

		int retorno = obtener_valor(paquete.parametros) //la de funciones suse, lo que te retorna

		return retorno
	}
	//ponele un 0 si esta ok


		//no nmanda nada, solo necesito que suse me de el proximo hilo a executar. Quizas tengo que evaluar que tenga el programa?? Entienod que lo sabe por el socket
	int me_schedule_next(){

		t_paquete paquete_respuesta = {
				.header = SUSE_SCHEDULE_NEXT,
				.parametros = NULL
				};

		//agregar_valor(paquete_respuesta.parametros,tid);

		enviar_paquete(paquete_respuesta,socket) //el que guarde al inciiar la conex)

		t_paquete paquete_respuesta = recibir_paquete(socket);

		int retorno = obtener_valor(paquete.parametros) //la de funciones suse, lo que te retorna

		return retorno} //ponele un 0 si esta ok



	// ver la parte de "si no había hilo, se liberaban las conexiones"

	int me_close(int tid){

		t_paquete paquete_respuesta = {
				.header = SUSE_CLOSE,
				.parametros = list_create()
				};

		agregar_valor(paquete_respuesta.parametros,tid);

		enviar_paquete(paquete_respuesta,socket) //el que guarde al inciiar la conex)

		t_paquete paquete_respuesta = recibir_paquete(socket);

		int retorno = obtener_valor(paquete.parametros) //la de funciones suse, lo que te retorna

		return retorno} //ponele un 0 si esta ok


