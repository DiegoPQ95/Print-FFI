#include "pch.h"
#include "win_printer_management.h"
#include "convert_string_to_utf8.h"
#include <combaseapi.h>



BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}



// -------------------- Funciones exportadas (DLL interface) --------------------
extern "C" {

    // Cada función asigna la memoria para el string JSON con CoTaskMemAlloc.
    // El consumidor debe liberar la memoria con FreeString.
    __declspec(dllexport) char* GetPrintersJson() {
        // Obtener el JSON en formato wide string (UTF-16)
        std::wstring json = WinPrinterManagement::getPrintersJson();
		return ConvertWStringToUtf8(json);
    }

    __declspec(dllexport) char* GetDefaultPrinterNameJson() {
        std::wstring json = WinPrinterManagement::getDefaultPrinterNameJson();
        return ConvertWStringToUtf8(json);
    }

    __declspec(dllexport) char* GetPrinterJson(const wchar_t* printerName) {
        std::wstring json = WinPrinterManagement::getPrinterJson(printerName);
        return ConvertWStringToUtf8(json);
    }

    __declspec(dllexport) char* GetJobJson(const wchar_t* printerName, DWORD jobId) {
        std::wstring json = WinPrinterManagement::getJobJson(printerName, jobId);
        return ConvertWStringToUtf8(json);
    }

    __declspec(dllexport) char* SetJobJson(const wchar_t* printerName, DWORD jobId, const char* command) {
        std::wstring json = WinPrinterManagement::setJobJson(printerName, jobId, command);
        return ConvertWStringToUtf8(json);
    }

    __declspec(dllexport) char* GetSupportedJobCommandsJson() {
        std::wstring json = WinPrinterManagement::getSupportedJobCommandsJson();
        return ConvertWStringToUtf8(json);
    }

    __declspec(dllexport) char* GetSupportedPrintFormatsJson() {
        std::wstring json = WinPrinterManagement::getSupportedPrintFormatsJson();
        return ConvertWStringToUtf8(json);
    }

    __declspec(dllexport) char* PrintDirectJson(const wchar_t* printerName, const char* data, const wchar_t* docName, const wchar_t* dataType) {
        std::wstring json = WinPrinterManagement::printDirectJson(printerName, data, docName, dataType);
        return ConvertWStringToUtf8(json);
    }

    __declspec(dllexport) void FreeString(char* str) {
        if (str) {
            CoTaskMemFree(str);
        }
    }
}
