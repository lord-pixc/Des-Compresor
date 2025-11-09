//bibliotecas estandar usadas para entrada salida, manejo de archivos, estructuras dinamicas y texto
//se evita usar caracteres especiales y se describe cada bloque de forma descriptiva
#include <iostream>
#include <fstream>
#include <vector>
#include <queue>
#include <map>
#include <string>

using namespace std;

bool decompressFile(const string& cpmPath);

//estructura principal del arbol de huffman
struct HuffmanNodo {
    //contador acumulado que indica cuantas veces aparece el simbolo asociado
    unsigned long long frecuencia;
    //identificador del byte almacenado en este nodo, -1 para nodos internos
    int byteGuardado;
    //puntero que apunta al hijo izquierdo, asociado al bit 0 en el recorrido
    HuffmanNodo* izquierda;
    //puntero que apunta al hijo derecho, asociado al bit 1 en el recorrido
    HuffmanNodo* derecha;

    HuffmanNodo(unsigned long long f, int b, HuffmanNodo* izq = NULL, HuffmanNodo* der = NULL)
        : frecuencia(f), byteGuardado(b), izquierda(izq), derecha(der) {
    }
};

//comparador para ordenar la cola segun la frecuencia mas baja primero
struct CompararHuffman {
    bool operator()(HuffmanNodo* a, HuffmanNodo* b) const {
        //retorna true cuando a tiene mayor frecuencia para que la cola priorice el elemento mas chico primero
        return a->frecuencia > b->frecuencia;
    }
};

//obtiene carpeta base de una ruta simple
string getDirectory(const string& path) {
    //busca el ultimo separador de directorio para aislar la carpeta contenedora
    size_t pos = path.find_last_of("/\\");
    if (pos == string::npos)
        return "";

    return path.substr(0, pos + 1);
}

//obtiene nombre y extension
string getFileName(const string& path) {
    //localiza el ultimo separador para extraer la parte final del camino
    size_t pos = path.find_last_of("/\\");
    if (pos == string::npos)
        return path;

    return path.substr(pos + 1);
}

//toma nombre sin extension
string getBaseName(const string& filename) {
    //la extension comienza despues del ultimo punto, se extrae sin caracteres extra
    size_t pos = filename.find_last_of('.');
    if (pos == string::npos)
        return filename;

    return filename.substr(0, pos);
}

//obtiene solo la extension original
string getExtension(const string& filename) {
    //el substring desde el ultimo punto representa la extension original
    size_t pos = filename.find_last_of('.');
    if (pos == string::npos)
        return "";

    return filename.substr(pos);
}

//lee un archivo binario completo
//se reutiliza para la entrada original y para archivos .cpm sin alterar su contenido
bool readFile(const string& path, vector<unsigned char>& buffer) {
    //se abre el archivo en modo binario para conservar bytes sin traducciones
    ifstream in(path.c_str(), ios::binary);

    if (!in) {
        cerr << "No se pudo abrir el archivo de entrada: " << path << "\n";
        return false;
    }

    //se calculan los limites con seekg para conocer la longitud exacta
    in.seekg(0, ios::end);
    streamsize size = in.tellg();
    in.seekg(0, ios::beg);

    if (size <= 0) {
        cerr << "Archivo vacio o error de tamano.\n";
        return false;
    }

    //se ajusta el buffer para contener el archivo completo
    buffer.resize((size_t)size);
    if (!buffer.empty()) {
        in.read((char*)&buffer[0], size);
    }
    return true;
}

//escribe todos los bytes al disco
//recibe cualquier buffer y lo guarda tal cual, por ello se usa para salida comprimida y descomprimida
bool writeFile(const string& path, const vector<unsigned char>& buffer) {
    //modo binario con truncamiento garantiza que el contenido previo se reemplaza
    ofstream out(path.c_str(), ios::binary | ios::trunc);

    if (!out) {
        cerr << "No se pudo abrir el archivo de salida: " << path << "\n";
        return false;
    }

    if (!buffer.empty()) {
        out.write((const char*)&buffer[0], (streamsize)buffer.size());
    }
    return true;
}

//crea el arbol de huffman usando cola prioritaria
HuffmanNodo* buildHuffmanTree(const unsigned long long freqs[256]) {
    //cola de prioridad que entrega siempre el nodo con menor frecuencia
    priority_queue<HuffmanNodo*, vector<HuffmanNodo*>, CompararHuffman> colaHuffman;

    for (int i = 0; i < 256; ++i) {
        //cada simbolo con frecuencia positiva se convierte en un nodo hoja
        if (freqs[i] > 0) {
            colaHuffman.push(new HuffmanNodo(freqs[i], i));
        }
    }

    if (colaHuffman.empty()) return NULL;

    //caso especial con un simbolo
    if (colaHuffman.size() == 1) {
        HuffmanNodo* unico = colaHuffman.top();
        colaHuffman.pop();
        //se crea un nodo padre artificial para conservar la logica de recorridos binarios
        HuffmanNodo* raiz = new HuffmanNodo(unico->frecuencia, -1, unico, NULL);
        return raiz;
    }

    while (colaHuffman.size() > 1) {
        //extrae dos nodos con menor frecuencia para combinarlos en un nuevo padre
        HuffmanNodo* primero = colaHuffman.top();
        colaHuffman.pop();
        HuffmanNodo* segundo = colaHuffman.top();
        colaHuffman.pop();

        //el nuevo padre guarda la suma de frecuencias para mantener la codificacion optima
        HuffmanNodo* padre = new HuffmanNodo(primero->frecuencia + segundo->frecuencia, -1, primero, segundo);
        colaHuffman.push(padre);
    }

    return colaHuffman.top();
}

//recorre el arbol para asignar codigos
void buildCodes(HuffmanNodo* nodo, const string& prefix, vector<string>& codes) {
    if (!nodo) return;

    //se usa recorrido en profundidad acumulando el prefijo de bits hasta llegar a cada hoja

    //hoja del arbol
    if (!nodo->izquierda && !nodo->derecha && nodo->byteGuardado >= 0) {
        codes[nodo->byteGuardado] = prefix.empty() ? "0" : prefix;
        return;
    }

    //el hijo izquierdo representa un cero, por eso se concatena '0' al prefijo acumulado
    buildCodes(nodo->izquierda, prefix + "0", codes);
    //el hijo derecho representa un uno, se concatena '1' manteniendo la propiedad del arbol
    buildCodes(nodo->derecha, prefix + "1", codes);
}

//convierte la cadena de bits en bytes reales usando desplazamientos y or
vector<unsigned char> bitsToBytes(const string& bits, int& paddedBits) {
    //esta rutina empaqueta la cadena de caracteres '0' y '1' en bytes fisicos usando corrimientos y or bit a bit
    //se calcula la cantidad de bits de relleno para ajustar a multiplos de 8
    paddedBits = 0;
    if (bits.empty()) return vector<unsigned char>();

    int totalBits = (int)bits.size();
    paddedBits = (8 - (totalBits % 8)) % 8;

    string bitstream = bits;
    //agrega ceros a la derecha para completar el ultimo byte
    bitstream.append(paddedBits, '0');

    int finalBits = (int)bitstream.size();
    vector<unsigned char> out(finalBits / 8, 0);

    int bitIndex = 0;
    for (size_t i = 0; i < out.size(); ++i) {
        //inicializa el byte actual en cero para componerlo bit a bit
        unsigned char byteActual = 0;

        for (int b = 0; b < 8; ++b) {
            //desplaza a la izquierda para dejar espacio al bit nuevo
            byteActual = (unsigned char)(byteActual << 1);

            if (bitstream[bitIndex++] == '1') {
                //or con 1 prende el bit menos significativo
                byteActual = (unsigned char)(byteActual | 1);
            }
        }
        out[i] = byteActual;
    }

    return out;
}

//pasa cada byte a texto de bits usando mascaras
string bytesToBits(const vector<unsigned char>& data) {
    //convierte cada byte comprimido a su representacion textual de bits para lectura secuencial
    string bits;
    bits.reserve(data.size() * 8);

    for (size_t i = 0; i < data.size(); ++i) {
        unsigned char byte = data[i];
        for (int b = 7; b >= 0; --b) {
            //mascara con desplazamiento para revisar cada posicion
            unsigned char mask = (unsigned char)(1 << b);
            //se usa and bit a bit para aislar el bit objetivo antes de convertirlo a caracter
            bits.push_back((byte & mask) ? '1' : '0');
        }
    }

    return bits;
}

//lee un entero de 4 bytes desde el buffer
unsigned int readUInt(const vector<unsigned char>& data, size_t& offset) {
    if (offset + 4 > data.size()) return 0;
    //se reconstruye el entero en formato little endian aplicando corrimientos de 8 bits
    unsigned int value = 0;
    for (int i = 0; i < 4; ++i) {
        //cada byte se desplaza segun su peso y se combina con or para reconstruir el entero
        value |= ((unsigned int)data[offset + i]) << (8 * i);
    }
    offset += 4;
    return value;
}

//lee un entero de 8 bytes usando desplazamientos
unsigned long long readULL(const vector<unsigned char>& data, size_t& offset) {
    if (offset + 8 > data.size()) return 0;
    //la logica replica el proceso anterior pero ampliando a 64 bits para manejar valores grandes
    unsigned long long value = 0;
    for (int i = 0; i < 8; ++i) {
        //se aprovechan corrimientos de 8 bits para ubicar cada byte en su posicion correcta
        value |= ((unsigned long long)data[offset + i]) << (8 * i);
    }
    offset += 8;
    return value;
}

//escribe el header completo con informacion adicional
void writeHeader(ofstream& out,
    const vector<string>& codes,
    int paddedBits,
    unsigned long long originalSize,
    const string& originalName) {
    //guarda primero los bits de relleno para reconstruir el ultimo byte
    int pad = paddedBits;
    //el valor de relleno permite saber cuantos bits invalidos descartar durante la lectura
    out.write((const char*)&pad, sizeof(int));

    unsigned int numCodes = 0;
    for (size_t i = 0; i < codes.size(); ++i) {
        if (!codes[i].empty()) numCodes++;
    }
    out.write((const char*)&numCodes, sizeof(unsigned int));

    for (int i = 0; i < 256; ++i) {
        if (codes[i].empty()) continue;

        unsigned char symbol = (unsigned char)i;
        unsigned int len = (unsigned int)codes[i].size();

        //se serializa el simbolo seguido de la longitud y la cadena de bits sin separadores
        out.write((const char*)&symbol, sizeof(unsigned char));
        out.write((const char*)&len, sizeof(unsigned int));
        out.write(codes[i].data(), (streamsize)len);
    }

    out.write((const char*)&originalSize, sizeof(unsigned long long));

    unsigned int nameLen = (unsigned int)originalName.size();
    out.write((const char*)&nameLen, sizeof(unsigned int));
    if (nameLen > 0) {
        out.write(originalName.data(), (streamsize)nameLen);
    }
}

//lee el header y arma las tablas de codigos
bool readHeader(const vector<unsigned char>& fileData,
    size_t& offset,
    vector<string>& codes,
    int& paddedBits,
    unsigned long long& originalSize,
    string& originalName) {
    if (fileData.size() < 8) return false;

    if (offset + sizeof(int) > fileData.size()) return false;
    //el entero inicial define cuantos bits finales deben ignorarse al reconstruir el flujo
    paddedBits = (int)readUInt(fileData, offset);

    if (offset + sizeof(unsigned int) > fileData.size()) return false;
    unsigned int numCodes = readUInt(fileData, offset);

    for (unsigned int i = 0; i < numCodes; ++i) {
        //recupera cada simbolo y longitud para reconstruir la tabla de codigos
        if (offset + 1 > fileData.size()) return false;
        unsigned char symbol = fileData[offset];
        offset += 1;

        if (offset + sizeof(unsigned int) > fileData.size()) return false;
        unsigned int len = readUInt(fileData, offset);

        if (offset + len > fileData.size()) return false;
        //se reconstruye el codigo literal concatenando exactamente len caracteres
        string code((const char*)&fileData[offset], len);
        offset += len;

        codes[symbol] = code;
    }

    if (offset + sizeof(unsigned long long) > fileData.size()) return false;
    //se obtiene el valor original de size para saber hasta donde leer los bytes reconstruidos
    originalSize = readULL(fileData, offset);

    if (offset + sizeof(unsigned int) > fileData.size()) return false;
    unsigned int nameLen = readUInt(fileData, offset);

    if (offset + nameLen > fileData.size()) return false;
    originalName.assign((const char*)&fileData[offset], nameLen);
    offset += nameLen;

    return true;
}

//comprime el archivo original
//flujo: calcula frecuencias, genera arbol, obtiene codigos, empaqueta bits y escribe header mas payload
bool compressFile(const string& inputPath) {
    //lee todo el archivo original para medir frecuencias byte a byte
    vector<unsigned char> input;
    if (!readFile(inputPath, input)) {
        return false;
    }

    unsigned long long freqs[256];
    //se inicializa la tabla con ceros antes de sumar apariciones
    for (int i = 0; i < 256; ++i) freqs[i] = 0;
    for (size_t i = 0; i < input.size(); ++i) {
        //cuenta apariciones de cada byte para armar la tabla de frecuencias
        unsigned char b = input[i];
        freqs[b]++;
    }

    HuffmanNodo* root = buildHuffmanTree(freqs);
    if (!root) {
        cerr << "No se pudo construir el arbol de Huffman.\n";
        return false;
    }

    //inicializa la tabla de codigos antes de recorrer el arbol
    vector<string> codes(256);
    //cada posicion del vector representa el codigo binario asociado a su indice como byte
    buildCodes(root, "", codes);

    string bitString;
    //se reserva mas espacio para reducir realocaciones mientras se agregan cadenas de bits
    bitString.reserve(input.size() * 2);
    for (size_t i = 0; i < input.size(); ++i) {
        //concatena la representacion en bits para cada byte original
        bitString += codes[input[i]];
    }

    int paddedBits = 0;
    //convierte la cadena de bits a bytes compactos y devuelve cuantos bits finales fueron de relleno
    vector<unsigned char> compressedData = bitsToBytes(bitString, paddedBits);

    //se preparan componentes del nombre para dejar el .cpm en la misma carpeta que la fuente
    string dir = getDirectory(inputPath);
    string fileName = getFileName(inputPath);
    string base = getBaseName(fileName);
    string compressedPath = dir + base + ".cpm";

    ofstream out(compressedPath.c_str(), ios::binary | ios::trunc);
    if (!out) {
        cerr << "No se pudo abrir el archivo de salida: " << compressedPath << "\n";
        return false;
    }

    unsigned long long originalSize = (unsigned long long)input.size();
    string originalName = fileName;

    writeHeader(out, codes, paddedBits, originalSize, originalName);
    if (!compressedData.empty()) {
        out.write((const char*)&compressedData[0], (streamsize)compressedData.size());
    }

    cout << "Archivo comprimido correctamente.\n";
    cout << "Archivo original : " << inputPath << "\n";
    cout << "Archivo .cpm     : " << compressedPath << "\n";
    return true;
}

//descomprime el archivo creado
//flujo: lee header, reconstruye tabla, expande bits y remueve relleno para obtener los bytes originales
bool decompressFile(const string& cpmPath) {
    //carga el archivo comprimido completo para analizar su header y datos binarios
    vector<unsigned char> fileData;
    if (!readFile(cpmPath, fileData)) {
        return false;
    }

    size_t offset = 0;
    vector<string> codes(256);
    //al reutilizar la misma estructura que en compresion se facilita la traduccion inversa
    int paddedBits = 0;
    unsigned long long originalSize = 0;
    string originalName;

    if (!readHeader(fileData, offset, codes, paddedBits, originalSize, originalName)) {
        cerr << "Header de Huffman invalido.\n";
        return false;
    }

    //extrae la seccion comprimida restante despues del header
    vector<unsigned char> compressedBytes;
    if (offset < fileData.size()) {
        //copia solo la porcion restante que contiene la data comprimida real
        compressedBytes.assign(fileData.begin() + offset, fileData.end());
    }
    //se expande el arreglo comprimido a una cadena de bits para iterar facilmente sobre cada simbolo
    string bitString = bytesToBits(compressedBytes);

    if (paddedBits > 0 && paddedBits <= (int)bitString.size()) {
        //elimina los bits de relleno agregados durante la compresion
        bitString.resize(bitString.size() - paddedBits);
    }

    //diccionario inverso que traduce secuencias de bits a su byte asociado
    map<string, unsigned char> reverseCodes;
    for (int i = 0; i < 256; ++i) {
        if (!codes[i].empty()) {
            //se crea un mapa inverso para traducir secuencias de bits a bytes originales
            reverseCodes[codes[i]] = (unsigned char)i;
        }
    }

    vector<unsigned char> output;
    if (originalSize > 0) {
        //reserva un size aproximado para evitar realocaciones
        output.reserve((size_t)originalSize);
    }

    //buffer temporal con los bits acumulados hasta formar un codigo valido
    string current;
    for (size_t i = 0; i < bitString.size(); ++i) {
        current.push_back(bitString[i]);
        map<string, unsigned char>::iterator it = reverseCodes.find(current);
        if (it != reverseCodes.end()) {
            //cuando se encuentra un codigo completo se agrega el byte correspondiente
            output.push_back(it->second);
            current.clear();
            if (originalSize > 0 && output.size() >= (size_t)originalSize) {
                break;
            }
        }
    }

    if (originalSize > 0 && output.size() > (size_t)originalSize) {
        //si la reconstruccion genero bytes extra por padding, se recorta usando el size almacenado
        output.resize((size_t)originalSize);
    }

    //se recrea el nombre de salida usando la carpeta original mas un sufijo descriptivo
    string dir = getDirectory(cpmPath);
    string baseO = getBaseName(originalName);
    string extO = getExtension(originalName);
    string outputName = dir + baseO + "-descomprimido" + extO;

    if (!writeFile(outputName, output)) {
        return false;
    }

    cout << "Archivo descomprimido correctamente.\n";
    cout << "Archivo .cpm             : " << cpmPath << "\n";
    cout << "Nombre original (header) : " << originalName << "\n";
    cout << "Archivo descomprimido    : " << outputName << "\n";
    return true;
}

//punto de entrada con menu interactivo basico para elegir operacion
int main() {
    cout << "============================================\n";
    cout << "  COMPRESOR / DESCOMPRESOR HUFFMAN (.cpm)\n";
    cout << "============================================\n\n";

    int opcion;
    string entrada;

    do {
        //muestra las opciones disponibles y lee la accion del usuario
        cout << "\nMENU:\n";
        cout << "1. Comprimir archivo (ArchivoX.ext -> ArchivoX.cpm)\n";
        cout << "2. Descomprimir archivo (ArchivoX.cpm -> ArchivoX-descomprimido.ext)\n";
        cout << "0. Salir\n";
        cout << "Seleccione una opcion: ";

        if (!(cin >> opcion)) {
            cin.clear();
            cin.ignore(10000, '\n');
            cout << "Entrada invalida. Intente de nuevo.\n";
            continue;
        }

        switch (opcion) {
        case 1:
            cout << "\n--- COMPRESION ---\n";
            cout << "Ingrese ruta del archivo a comprimir (ArchivoX.ext): ";
            cin >> entrada;
            if (!compressFile(entrada)) {
                cout << "Ocurrio un error al comprimir.\n";
            }
            break;

        case 2:
            cout << "\n--- DESCOMPRESION ---\n";
            cout << "Ingrese ruta del archivo comprimido (.cpm): ";
            cin >> entrada;
            if (!decompressFile(entrada)) {
                cout << "Ocurrio un error al descomprimir.\n";
            }
            break;

        case 0:
            cout << "Saliendo del programa...\n";
            break;

        default:
            cout << "Opcion no valida. Intente nuevamente.\n";
            break;
        }

    } while (opcion != 0);

    return 0;
}
