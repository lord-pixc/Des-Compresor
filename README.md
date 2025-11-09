# Descompresor Huffman

## Requisitos
- Sistema Windows con Microsoft Visual Studio (probado en versiones recientes).
- Proyecto ya configurado con los archivos del repositorio.

## Como compilar
1. Abrir la solucion `Huffman Des-Compresor.sln` en Visual Studio.
2. Confirmar que el proyecto "Huffman Des-Compresor" esta establecido como proyecto de inicio.
3. Compilar con `Ctrl + Shift + B` o con el menu **Build > Build Solution**.

## Como usar el programa
1. Ejecutar la aplicacion desde Visual Studio o abriendo el ejecutable generado.
2. Se mostrara un menu con tres opciones:
   - `1` para comprimir un archivo regular (por ejemplo `foto.png`).
   - `2` para descomprimir un archivo `.cpm` creado por el programa.
   - `0` para salir.
3. Para comprimir:
   - Seleccione la opcion `1`.
   - Escriba la ruta completa o relativa del archivo a comprimir.
   - Se genera un archivo con la misma ruta y nombre base, pero con extension `.cpm`.
4. Para descomprimir:
   - Seleccione la opcion `2`.
   - Escriba la ruta del archivo `.cpm`.
   - Se genera un archivo nuevo con sufijo `-descomprimido` y la extension original.
5. El programa muestra mensajes informativos para confirmar el exito de cada paso o indicar un fallo basico.

## Flujo interno del programa
1. **Conteo de frecuencias:**
   - Se recorre el archivo de entrada byte por byte para saber cuantas veces aparece cada simbolo (0-255).
   - Esta informacion llena un arreglo de 256 posiciones.
2. **Construccion del arbol Huffman:**
   - Se crea una cola de prioridad que siempre entrega los nodos menos frecuentes.
   - Cada combinacion forma un arbol binario donde los nodos hoja representan bytes reales.
3. **Asignacion de codigos:**
   - Se recorre el arbol. Ir a la izquierda agrega `0` y a la derecha agrega `1` al codigo.
   - Cada byte queda asociado a una cadena de bits.
4. **Conversion a bits y bytes:**
   - Se arma una cadena gigante con todos los codigos.
   - Se completa con ceros para que la cantidad de bits sea multiplo de ocho.
   - Se transforman bloques de ocho bits en un byte real mediante desplazamientos y operaciones OR.
5. **Escritura del encabezado:**
   - Se guarda cuanta informacion de relleno se agrego.
   - Se almacena el numero de codigos, sus longitudes, los bits que definen a cada byte, el tamano original y el nombre del archivo fuente.
6. **Escritura de datos comprimidos:**
   - Los bytes generados se escriben despues del encabezado.
7. **Proceso inverso para descomprimir:**
   - Se lee el encabezado y se reconstruyen los codigos.
   - Se convierte la secuencia de bytes nuevamente a bits.
   - Se van leyendo bits hasta empatar con un codigo conocido y se recupera el byte original.
   - Se detiene cuando se alcanza el tamano esperado o se agotan los datos.

## Notas importantes
- El programa asume archivos binarios genericos y no valida rutas con espacios u otros caracteres especiales.
- El formato `.cpm` es propio del ejercicio: incluye encabezado y datos en binario.
- No se libera memoria del arbol porque el sistema operativo la recupera al cerrar el programa. Para proyectos grandes podria agregarse una funcion de limpieza.
