#define _POSIX_C_SOURCE 200809L //getline
#define _XOPEN_SOURCE           //time
#define TIME_FORMAT "%FT%T%z"
#define AGREGAR "agregar_archivo"
#define VISITANTES "ver_visitantes"
#define VISITADOS "ver_mas_visitados"
#define ERROR "Error en comando %s\n"
#define BUFFERSIZE 100

#include "analog.h"
typedef struct recurso {
	const char* nombre;
	int cant;
}recurso_t;

typedef struct solicitud{
	size_t cant_solicitudes;
	bool dos;
	cola_t* fechas;
}solicitud_t;

/* *****************************************************************
   *                   FUNCIONES AUXILIARES                        *
   ***************************************************************** */
/* 
 * Debe recibir dos punteros del tipo void que son estructuras recursos_t
 * debe devolver:
 *   menor a 0  si  recurso_1->cant > recurso_2->cant
 *       0      si  recurso_1->cant == recurso_2->cant
 *   mayor a 0  si  recurso_1->cant < recurso_2->cant
 */
int cmp_heap(void* dato_1,void* dato_2){
	recurso_t* recurso_1 = dato_1;
	recurso_t* recurso_2 = dato_2;
	if(recurso_1->cant < recurso_2->cant)
		return 1;
	if(recurso_1->cant > recurso_2->cant)
		return -1;
	return 0;
}

/* 
 * Debe recibir dos punteros del tipo char que son cadenas de ip y
 * debe devolver:
 *   menor a 0  si  ip1 < ip2
 *       0      si  ip1 == ip2
 *   mayor a 0  si  ip1 > ip2
 */
int cmp_ip(const char* ip1, const char* ip2){
	char** strv_1 = split(ip1,'.');
	char** strv_2 = split(ip2,'.');

	int i = 0;
	int j = 0;
	while(i==0 && j<4){
		i = strcmp(strv_1[j],strv_2[j]);
		j++;
	}

	free_strv(strv_1);
	free_strv(strv_2);
	return i;

}


/* 
 * Recibe una cadena y devuelve true si los elemenetos de la cadena
 * corresponden a caracters numericos. False en caso contrario. Si el 
 * largo de la cadena es cero devuelve true.
 */
bool es_numero(char* input){
	size_t largo = strlen(input);
	for(size_t i = 0; i < largo; i++){
		if(input[i] == '.') i++;
		if(!isdigit(input[i]))
			return false;
	}
	return true;

}

/* 
 * Devuleve un bool segun si el comando que se ingreso es posible
 * ejecutar correctamente o no. Valida si la cantidad de parametros que
 * ingreso el usuario son los correpondientes, ademas si el comando ingresado
 * corresponde al pasado por parametro
 */
bool verificar_comando(char** strv, size_t strvc,char* comando){
	size_t i = 0;
	while(strv[i])
		i++;
	return strcmp(strv[0],comando)==0 && strvc==i;
}


void destruir_solicitud(solicitud_t* solicitud){
    cola_destruir(solicitud->fechas, NULL);
    free(solicitud);
}
/*
 * Dada una cadena en formato ISO-8601 devuelve una variable de tipo time_t
 * que representa un instante en el tiempo.
 */
time_t iso8601_to_time(const char* iso8601){
	struct tm bktime = { 0 };
	strptime(iso8601, TIME_FORMAT, &bktime);
	return mktime(&bktime);
}

time_t* crear_fecha(const char* fecha){
    time_t* fecha_nueva = malloc(sizeof(time_t));
    *fecha_nueva = iso8601_to_time(fecha);
    return fecha_nueva;
}
/* 
 * Recibe una fecha por parametro y crea un struct de solicitud. Si hubo
 * algun problema en pedir memoria devuelve NULL.
 */
solicitud_t* crear_solicitud(const char* fecha){
	solicitud_t* solicitud =  malloc(sizeof(solicitud_t));
	if(!solicitud) return NULL;
    cola_t* cola = cola_crear();
    if(!cola) return NULL;
	solicitud->dos = false;
	solicitud->fechas = cola;
    time_t* fecha_nueva = crear_fecha(fecha);
    if(!fecha_nueva) return NULL;
    cola_encolar(solicitud->fechas, fecha_nueva);
    solicitud->cant_solicitudes = 1;
	return solicitud;
}

/* 
 * Recibe una fecha por parametro y crea un struct de solicitud. Si hubo
 * algun problema en pedir memoria devuelve NULL.
 */
recurso_t* crear_recurso(hash_t* hash,hash_iter_t* iter){
	recurso_t* recurso =  malloc(sizeof(solicitud_t));
	if(!recurso) return NULL;

	const char* clave = hash_iter_ver_actual(iter);
	recurso->nombre = clave;
	recurso->cant=*(int*)hash_obtener(hash,clave);
	return recurso;
}


/*
 * Recibe un arbol binario de busqueda y un vector de cadenas, y actualiza
 * la informacion de una ip (clave del arbol) segun si esa ip esta 
 * realizando un ataque de denegacion de servicios
 */
bool analizar_dos (abb_t* abb_ip,char** strv){
	const char* ip = strv[0];	const char* fecha= strv[1];
	solicitud_t* solicitud = abb_obtener(abb_ip,ip);
	//si hay una ip en el arbol la analizamos
    if(solicitud){
        if(solicitud->dos)    return true;

        time_t* fecha_actual = crear_fecha(fecha);
        double dif_tiempo = difftime(*(time_t*)cola_ver_primero(solicitud->fechas),*fecha_actual);

        if(dif_tiempo<2) {
            cola_encolar(solicitud->fechas, &fecha_actual);
            solicitud->cant_solicitudes++;
        }else{
            while(difftime(*(time_t*)cola_ver_primero(solicitud->fechas), *fecha_actual) >= 2){
                cola_desencolar(solicitud->fechas);
                solicitud->cant_solicitudes--;
            }
        }
        if(solicitud->cant_solicitudes > 4)
            solicitud->dos = true;
        return true;
    }


	//todo esto pasa si la ip no esta en el arbol
	solicitud = crear_solicitud(fecha);
	if(!solicitud) return false;
	return abb_guardar(abb_ip,ip,solicitud);

}
/*
 * Actualiza en un hash los contadores correspondientes a los recursos.
 */
bool analizar_resources(hash_t* hash_resources,char** strv){
	char* resource = strv[3];
    int* cont = hash_obtener(hash_resources,resource);
	if(cont){
		(*cont)++;
		return hash_guardar(hash_resources,resource,cont);
	}
	cont = malloc(sizeof(int*));
	*cont = 1;
	return hash_guardar(hash_resources,resource,cont);

}
/* 
 * Itera de manera acotada sobre un arbol binario de busqueda e imprime
 * aqeullas ip cuyo valor es una estrucura con el miembro dos == true
 */
void ver_dos(abb_t* abb_ip){
	abb_iter_t* iter = abb_iter_in_crear(abb_ip);
	while(!abb_iter_in_al_final(iter)){
		const char* ip = abb_iter_in_ver_actual(iter);
		solicitud_t* solicitud = abb_obtener(abb_ip,ip);
		if(solicitud->dos)
			printf("DoS: %s\n",ip);
		abb_iter_in_avanzar(iter);
	}
}

bool imprimir(const char* dato,void* extra,void*extra_1){
	printf("\t%s\n",dato);
	return true;
}

void ver_visitantes(abb_t* abb_ip,char* inicio,char* final){
	abb_in_order(abb_ip,imprimir,NULL,inicio,final);
	printf("OK\n");
}

void ver_mas_visitados(hash_t* hash_resources,int n){
	
	hash_iter_t* iter = hash_iter_crear(hash_resources);
	heap_t* heap = heap_crear((cmp_func_t) cmp_heap);
	int i=0;

	while(!hash_iter_al_final(iter) && i<n){
		recurso_t* recurso = crear_recurso(hash_resources,iter);
		heap_encolar(heap,recurso);
		hash_iter_avanzar(iter);
		i++;
	}
	while(!hash_iter_al_final(iter)){

		recurso_t* recurso = crear_recurso(hash_resources,iter);
		recurso_t* aux = heap_ver_max(heap);
		if(recurso->cant > aux->cant){ //es ver_min
			aux = heap_desencolar(heap);
			free(aux);
			heap_encolar(heap,recurso);
		}else
			free(recurso);

		hash_iter_avanzar(iter);
	}

	printf("Sitios más visitados:\n");
	for(i=0;i<n;i++){
		recurso_t* aux = heap_desencolar(heap);
		printf("\t%s - %d\n",aux->nombre,aux->cant);
	}

	
	heap_destruir(heap,free); //es un simple free puesto que no quiero liberar la cadena
	printf("OK\n");
}

/* 
 * Recibe un archivo, verifica si lo puede abrir y devuelve una lista
 * de usuarios en memoria dinamica. Si hubo un problema con el arhcivo
 * devuelve NULL. Si el archivo existe y esta vacio devuelve una lista
 * vacia.
 */
bool agregar_archivo(abb_t* abb_ip, hash_t* hash_resources, char* file){
	FILE* archivo = fopen(file,"r");
	if (archivo == NULL)
		return false;

	char* linea = NULL; size_t capacidad = 0; //combo getline
	char** strv;
	bool ok; bool ok2;

	while(getline(&linea,&capacidad, archivo) > 0){
		strv = split(linea,'\t');
		ok = analizar_dos(abb_ip,strv);
		ok2= analizar_resources(hash_resources,strv);
        free_strv(strv);
	}
	free(linea);
	fclose(archivo);

	if(ok && ok2){
		ver_dos(abb_ip);
		printf("OK\n");
        return true;
	}
    return false;
}

/* *****************************************************************
 *               	     FUNCION PRINCIPAL
 * *****************************************************************/
int main(int argc, char* argv[]){
    size_t buffersize = BUFFERSIZE;
    char* buffer  = malloc(sizeof(char) * buffersize);
    if(!buffer)
        return 0;

	hash_t* hash_resources = hash_crear(free);
	abb_t* abb_ip = abb_crear(cmp_ip,(abb_destruir_dato_t) free);

    while(getline(&buffer, &buffersize, stdin) > 0){
		char** strv = split(buffer,' ');
		if (verificar_comando(strv, 2, AGREGAR)) {
            if (!agregar_archivo(abb_ip, hash_resources, strv[1])) {
                free(buffer);
                continue;
            }
        }
        if (verificar_comando(strv, 3, VISITANTES)){
			ver_visitantes(abb_ip, strv[1], strv[2]);
			free(buffer);
            continue;
        }

		if(verificar_comando(strv,2, VISITADOS) && es_numero(strv[1])) {
			int n = (int) strtol(strv[1], NULL, 10);
			ver_mas_visitados(hash_resources, n);
			free(buffer);
            continue;
		}
		free(buffer);
        fprintf(stderr, ERROR, strv[0]);
		free_strv(strv);
	}
	hash_destruir(hash_resources);
	abb_destruir(abb_ip);
	return 0;
}