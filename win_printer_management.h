#ifndef PRINTER_MANAGEMENT_H
#define PRINTER_MANAGEMENT_H

#include <windows.h>
#include <string>
#include <vector>

// Estructuras para la información de la impresora y el trabajo de impresión.
struct PrinterInfo {
    std::wstring name;
    std::wstring serverName;
    std::wstring shareName;
    std::wstring portName;
    std::wstring driverName;
    std::wstring comment;
    std::wstring location;
    DWORD status;
    DWORD attributes;
    DWORD jobs;
};

struct JobInfo {
    DWORD id;
    std::wstring document;
    std::wstring userName;
    DWORD status;
    DWORD size;
    DWORD pagesPrinted;
};

namespace WinPrinterManagement {

    // Funciones internas que devuelven respuestas en formato JSON (UTF-16).
    std::wstring getPrintersJson();
    std::wstring getDefaultPrinterNameJson();
    std::wstring getPrinterJson(const std::wstring& printerName);
    std::wstring getJobJson(const std::wstring& printerName, DWORD jobId);
    std::wstring setJobJson(const std::wstring& printerName, DWORD jobId, const std::string& command);
    std::wstring getSupportedJobCommandsJson();
    std::wstring getSupportedPrintFormatsJson();
    std::wstring printDirectJson(const std::wstring& printerName, const uint8_t* data, const size_t dataLen,
        const std::wstring& docName, const std::wstring& dataType);
}

#endif