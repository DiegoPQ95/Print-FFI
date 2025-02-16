#include "pch.h"
#include "convert_string_to_utf8.h"

#include <combaseapi.h>

// Función de utilidad que convierte un std::wstring a una cadena UTF-8 asignada con CoTaskMemAlloc.
char* ConvertWStringToUtf8(const std::wstring& wstr) {
    // Calcular el tamaño requerido para la cadena UTF-8, incluyendo el carácter nulo.
    int size = WideCharToMultiByte(
        CP_UTF8,        // Codificación de salida: UTF-8.
        0,              // Sin flags especiales.
        wstr.c_str(),   // Cadena de entrada (UTF-16).
        -1,             // Cadena terminada en nulo.
        nullptr,        // Solo se solicita el tamaño.
        0,
        nullptr,
        nullptr
    );
    if (size == 0)
        return nullptr;

    // Asignar memoria para la cadena UTF-8.
    char* result = (char*)CoTaskMemAlloc(size);
    if (!result)
        return nullptr;

    // Realizar la conversión a UTF-8.
    if (WideCharToMultiByte(
        CP_UTF8,
        0,
        wstr.c_str(),
        -1,
        result,
        size,
        nullptr,
        nullptr
    ) == 0) {
        CoTaskMemFree(result);
        return nullptr;
    }
    return result;
}