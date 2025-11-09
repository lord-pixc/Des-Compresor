#include "Huffman Des-Compresor.cpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <queue>
#include <map>
#include <string>
#include <cstdint>

using namespace std;

bool decompressFile(const string& cpmPath) {
    vector<uint8_t> fileData;
    if (!readFile(cpmPath, fileData)) {
        return false;
    }

    size_t offset = 0;
    vector<string> codes(256);
    int paddedBits = 0;
    uint64_t originalSize = 0;
    string originalName;

    if (!readHeader(fileData, offset, codes, paddedBits, originalSize, originalName)) {
        cerr << "Header de Huffman invalido.\n";
        return false;
    }

    // Datos comprimidos desde offset hasta el final
    vector<uint8_t> compressedBytes(fileData.begin() + offset, fileData.end());
    string bitString = bytesToBits(compressedBytes);

    // Quitamos bits de relleno
    if (paddedBits > 0 && paddedBits <= (int)bitString.size()) {
        bitString.resize(bitString.size() - paddedBits);
    }

    // Mapa inverso: código -> símbolo
    map<string, uint8_t> reverseCodes;
    for (int i = 0; i < 256; ++i) {
        if (!codes[i].empty()) {
            reverseCodes[codes[i]] = static_cast<uint8_t>(i);
        }
    }

    vector<uint8_t> output;
    output.reserve(static_cast<size_t>(originalSize));

    string current;
    for (char c : bitString) {
        current.push_back(c);
        auto it = reverseCodes.find(current);
        if (it != reverseCodes.end()) {
            output.push_back(it->second);
            current.clear();
            if (originalSize > 0 && output.size() >= originalSize) {
                // Por seguridad cortamos si alcanzamos el tamaño original
                break;
            }
        }
    }

    if (originalSize > 0 && output.size() > originalSize) {
        output.resize(static_cast<size_t>(originalSize));
    }

    // CAMBIO: generamos "ArchivoX-descomprimido.ext"
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
