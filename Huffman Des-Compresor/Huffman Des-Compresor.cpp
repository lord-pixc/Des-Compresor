#include "Descomprimir.cpp"

#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <queue>
#include <map>
#include <string>
#include <cstdint>

using namespace std;

// -----------------------------------------------------------------------------
// NODO DEL ÁRBOL DE HUFFMAN
// -----------------------------------------------------------------------------
struct Node {
    uint64_t freq;
    int byte;         
    Node* left;
    Node* right;

    Node(uint64_t f, int b, Node* l = nullptr, Node* r = nullptr)
        : freq(f), byte(b), left(l), right(r) {
    }
};

struct NodeCompare {
    bool operator()(Node* a, Node* b) const {
        return a->freq > b->freq;  
    }
};


// -----------------------------------------------------------------------------
// RUTAS Y NOMBRES DE ARCHIVO
// -----------------------------------------------------------------------------
string getDirectory(const string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos == string::npos) 
        return "";

    return path.substr(0, pos + 1);
}

string getFileName(const string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos == string::npos) 
        return path;

    return path.substr(pos + 1);
}

string getBaseName(const string& filename) {
    size_t pos = filename.find_last_of('.');
    if (pos == string::npos) 
        return filename;

    return filename.substr(0, pos);
}

string getExtension(const string& filename) {
    size_t pos = filename.find_last_of('.');
    if (pos == string::npos) 
        return "";

    return filename.substr(pos);  // incluye el '.'
}

// -----------------------------------------------------------------------------
// LECTURA / ESCRITURA DE ARCHIVOS BINARIOS
// -----------------------------------------------------------------------------
bool readFile(const string& path, vector<uint8_t>& buffer) {
    ifstream in(path, ios::binary);

    if (!in) {
        cerr << "No se pudo abrir el archivo de entrada: " << path << "\n";
        return false;
    }

    in.seekg(0, ios::end);
    streamsize size = in.tellg();
    in.seekg(0, ios::beg);

    if (size <= 0) {
        cerr << "Archivo vacio o error de tamaño.\n";
        return false;
    }

    buffer.resize(static_cast<size_t>(size));
    in.read(reinterpret_cast<char*>(buffer.data()), size);
    return true;
}

bool writeFile(const string& path, const vector<uint8_t>& buffer) {
    ofstream out(path, ios::binary | ios::trunc);

    if (!out) {
        cerr << "No se pudo abrir el archivo de salida: " << path << "\n";
        return false;
    }

    out.write(reinterpret_cast<const char*>(buffer.data()),
        static_cast<streamsize>(buffer.size()));
    return true;
}

// -----------------------------------------------------------------------------
// CONSTRUCCIÓN DEL ÁRBOL Y CÓDIGOS HUFFMAN
// -----------------------------------------------------------------------------
Node* buildHuffmanTree(const array<uint64_t, 256>& freqs) {
    priority_queue<Node*, vector<Node*>, NodeCompare> pq;

    for (int i = 0; i < 256; ++i) {
        if (freqs[i] > 0) {
            pq.push(new Node(freqs[i], i));
        }
    }

    if (pq.empty()) return nullptr;

    // Caso especial: solo un símbolo
    if (pq.size() == 1) {
        Node* only = pq.top();
        pq.pop();
        Node* root = new Node(only->freq,
            -1,
            only,
            nullptr
        );

        return root;
    }

    while (pq.size() > 1) {

        Node* a = pq.top(); 
        pq.pop();
        Node* b = pq.top(); 
        pq.pop();

        Node* parent = new Node(
            a->freq + b->freq,
            -1,
            a,
            b
        );

        pq.push(parent);
    }

    return pq.top();
}

void buildCodes(Node* node, const string& prefix, vector<string>& codes) {
    if (!node) return;

    // Hoja
    if (!node->left && !node->right && node->byte >= 0) {
        // Si solo había un símbolo, puede que prefix esté vacío -> ponemos "0"
        codes[node->byte] = prefix.empty() ? "0" : prefix;
        return;
    }

    // Izquierda = 0, Derecha = 1
    buildCodes(node->left, prefix + "0", codes);
    buildCodes(node->right, prefix + "1", codes);
}

// -----------------------------------------------------------------------------
// CONVERSIÓN bits <-> bytes (usa &, |, <<, etc. en el algoritmo real)
// -----------------------------------------------------------------------------
vector<uint8_t> bitsToBytes(const string& bits, int& paddedBits) {
    paddedBits = 0;
    if (bits.empty()) return {};

    int totalBits = static_cast<int>(bits.size());
    paddedBits = (8 - (totalBits % 8)) % 8;  // cuántos 0 agregamos al final

    string bitstream = bits;
    bitstream.append(paddedBits, '0');

    int finalBits = static_cast<int>(bitstream.size());
    vector<uint8_t> out(finalBits / 8);

    int bitIndex = 0;
    for (size_t i = 0; i < out.size(); ++i) {
        uint8_t byte = 0;

        for (int b = 0; b < 8; ++b) {
            byte <<= 1; // desplazamiento de bits

            if (bitstream[bitIndex++] == '1') {
                byte |= 1;  // OR con 1 -> pone el bit menos significativo en 1
            }
        }
        out[i] = byte;
    }

    return out;
}

string bytesToBits(const vector<uint8_t>& data) {
    string bits;
    bits.reserve(data.size() * 8);

    for (uint8_t byte : data) {
        for (int b = 7; b >= 0; --b) {
            uint8_t mask = static_cast<uint8_t>(1u << b);
            bits.push_back((byte & mask) ? '1' : '0');  // AND con máscara
        }
    }

    return bits;
}


/* HEADER DEL ARCHIVO COMPRIMIDO.cpm

 Formato del archivo .cpm:
  [int32  paddedBits]
  [uint32 numCodes]
    por cada código:
       [uint8  symbol]
       [uint32 lenCode]
       [lenCode chars '0'/'1']
  [uint64 originalSize]      
  [uint32 nameLen]
  [nameLen chars nombreOriginal]   

 Después de esto vienen los datos comprimidos (RawData).

 Estos CAMBIOS de header son para cumplir:
  "Header/Size/nombreOriginal y su RawData" de la práctica.
 -----------------------------------------------------------------------------*/
void writeHeader(ofstream& out,
    const vector<string>& codes,
    int paddedBits,
    uint64_t originalSize,
    const string& originalName) {
    int32_t pad = paddedBits;
    out.write(reinterpret_cast<const char*>(&pad), sizeof(int32_t));

    uint32_t numCodes = 0;
    for (const auto& c : codes) {
        if (!c.empty()) ++numCodes;
    }
    out.write(reinterpret_cast<const char*>(&numCodes), sizeof(uint32_t));

    for (int i = 0; i < 256; ++i) {
        if (codes[i].empty()) continue;

        uint8_t symbol = static_cast<uint8_t>(i);
        uint32_t len = static_cast<uint32_t>(codes[i].size());

        out.write(reinterpret_cast<const char*>(&symbol), sizeof(uint8_t));
        out.write(reinterpret_cast<const char*>(&len), sizeof(uint32_t));
        out.write(codes[i].data(), static_cast<streamsize>(len));
    }

    // CAMBIO: guardamos tamaño original y nombre original
    out.write(reinterpret_cast<const char*>(&originalSize), sizeof(uint64_t));

    uint32_t nameLen = static_cast<uint32_t>(originalName.size());
    out.write(reinterpret_cast<const char*>(&nameLen), sizeof(uint32_t));
    out.write(originalName.data(), static_cast<streamsize>(nameLen));
}

bool readHeader(const vector<uint8_t>& fileData,
    size_t& offset,
    vector<string>& codes,
    int& paddedBits,
    uint64_t& originalSize,
    string& originalName) {
    if (fileData.size() < 8) return false;

    if (offset + sizeof(int32_t) > fileData.size()) return false;
    paddedBits = *reinterpret_cast<const int32_t*>(&fileData[offset]);
    offset += sizeof(int32_t);

    if (offset + sizeof(uint32_t) > fileData.size()) return false;
    uint32_t numCodes = *reinterpret_cast<const uint32_t*>(&fileData[offset]);
    offset += sizeof(uint32_t);

    for (uint32_t i = 0; i < numCodes; ++i) {
        if (offset + 1 > fileData.size()) return false;
        uint8_t symbol = fileData[offset];
        offset += 1;

        if (offset + sizeof(uint32_t) > fileData.size()) return false;
        uint32_t len = *reinterpret_cast<const uint32_t*>(&fileData[offset]);
        offset += sizeof(uint32_t);

        if (offset + len > fileData.size()) return false;
        string code(reinterpret_cast<const char*>(&fileData[offset]), len);
        offset += len;

        codes[symbol] = code;
    }

    //leer tamaño original y nombre original
    if (offset + sizeof(uint64_t) > fileData.size()) return false;
    originalSize = *reinterpret_cast<const uint64_t*>(&fileData[offset]);
    offset += sizeof(uint64_t);

    if (offset + sizeof(uint32_t) > fileData.size()) return false;
    uint32_t nameLen = *reinterpret_cast<const uint32_t*>(&fileData[offset]);
    offset += sizeof(uint32_t);

    if (offset + nameLen > fileData.size()) return false;
    originalName.assign(reinterpret_cast<const char*>(&fileData[offset]),
        nameLen);
    offset += nameLen;

    return true;
}

// -----------------------------------------------------------------------------
// COMPRESIÓN
//
// CAMBIO: compressFile SOLO pide el nombre del archivo original y
//         INTERNAMENTE decide que el comprimido se llame "ArchivoX.cpm"
//         en la misma carpeta, como dice la tabla de la práctica.
// -----------------------------------------------------------------------------
bool compressFile(const string& inputPath) {
    vector<uint8_t> input;
    if (!readFile(inputPath, input)) {
        return false;
    }

    // Frecuencia de cada byte
    array<uint64_t, 256> freqs{};
    for (uint8_t b : input) {
        freqs[b]++;
    }

    Node* root = buildHuffmanTree(freqs);
    if (!root) {
        cerr << "No se pudo construir el arbol de Huffman.\n";
        return false;
    }

    vector<string> codes(256);
    buildCodes(root, "", codes);

    // Codificar el archivo a una cadena de bits
    string bitString;
    bitString.reserve(input.size() * 2); // aproximación
    for (uint8_t b : input) {
        bitString += codes[b];
    }

    int paddedBits = 0;
    vector<uint8_t> compressedData = bitsToBytes(bitString, paddedBits);

    // CAMBIO: generamos nombre "ArchivoX.cpm"
    string dir = getDirectory(inputPath);
    string fileName = getFileName(inputPath);     // nombre original (con ext)
    string base = getBaseName(fileName);
    string compressedPath = dir + base + ".cpm";  // extensión .cpm

    ofstream out(compressedPath, ios::binary | ios::trunc);
    if (!out) {
        cerr << "No se pudo abrir el archivo de salida: " << compressedPath << "\n";
        return false;
    }

    uint64_t originalSize = static_cast<uint64_t>(input.size());
    string originalName = fileName; // sin ruta

    // Header + datos comprimidos
    writeHeader(out, codes, paddedBits, originalSize, originalName);
    out.write(reinterpret_cast<const char*>(compressedData.data()),
        static_cast<streamsize>(compressedData.size()));

    cout << "Archivo comprimido correctamente.\n";
    cout << "Archivo original : " << inputPath << "\n";
    cout << "Archivo .cpm     : " << compressedPath << "\n";
    return true;
}



// -----------------------------------------------------------------------------
// MENÚ PRINCIPAL
// -----------------------------------------------------------------------------
int main() {
    cout << "============================================\n";
    cout << "  COMPRESOR / DESCOMPRESOR HUFFMAN (.cpm)\n";
    cout << "============================================\n\n";


    int opcion;
    string entrada;

    do {
        cout << "\nMENU:\n";
        cout << "1. Comprimir archivo (ArchivoX.ext -> ArchivoX.cpm)\n";
        cout << "2. Descomprimir archivo (ArchivoX.cpm -> ArchivoX-descomprimido.ext)\n";
        cout << "0. Salir\n";
        cout << "Seleccione una opcion: ";

        if (!(cin >> opcion)) {
            // Manejo sencillo de error: si el usuario escribe letras u otra cosa
            cin.clear();
            cin.ignore(10000, '\n');
            cout << "Entrada invalida. Intente de nuevo.\n";
            continue;
        }

        switch (opcion) {
        case 1:
            cout << "\n--- COMPRESION ---\n";
            cout << "Ingrese ruta del archivo a comprimir (ArchivoX.ext): ";
            cin >> entrada;      // sencillo: sin espacios
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
