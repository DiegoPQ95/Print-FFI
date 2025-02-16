// printer_management.cpp
#include "pch.h"
#include "win_printer_management.h"

#include <winspool.h>
#include <map>
#include <sstream>
#include <memory>
#include <string>
#include <windows.h>


#pragma comment(lib, "Winspool.lib")

// -------------------- Helpers --------------------

// Formatea el error de Windows en una cadena legible.
std::wstring formatWindowsError(DWORD winErr) {
    LPWSTR lpMsgBuf = nullptr;
    FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        winErr,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPWSTR)&lpMsgBuf,
        0,
        NULL);
    std::wstring message = lpMsgBuf ? lpMsgBuf : L"";
    if (lpMsgBuf) {
        LocalFree(lpMsgBuf);
    }
    return message;
}
std::wstring sanitizeErrorMessage(const std::wstring& message) {
    std::wstring sanitized = message;
    sanitized.erase(std::remove_if(sanitized.begin(), sanitized.end(), [](wchar_t c) {
        return c == L'\r' || c == L'\n';
        }), sanitized.end());
    return sanitized;
}
// Construye el JSON final dado los parámetros.
// Se espera que 'responseJson' sea ya un fragmento JSON (por ejemplo, un objeto, array, string o número).
std::wstring buildJsonResult(int errorCode, const std::wstring& errorMessage, DWORD winErr, const std::wstring& responseJson, const std::wstring& errStep) {
    std::wostringstream oss;
    oss << L"{\"status\":" << errorCode
        << L",\"err_msg\":\"" << sanitizeErrorMessage(errorMessage)
        << L"\",\"err_step\":\"" << errStep
        << L"\",\"err_code\":" << winErr
        << L",\"response\":" << responseJson << L"}";
    return oss.str();
}

// -------------------- ClabuildJsonResultses y estructuras --------------------

// Manejador RAII para un HANDLE de impresora.
struct PrinterHandle {
    HANDLE handle;
    PrinterHandle(LPWSTR printerName) : handle(nullptr) {
        OpenPrinterW(printerName, &handle, NULL);
    }
    ~PrinterHandle() {
        if (handle)
            ClosePrinter(handle);
    }
    operator HANDLE() { return handle; }
};

// Diccionario de comandos para trabajos.
std::map<std::string, DWORD> jobCommands = {
    {"CANCEL", JOB_CONTROL_CANCEL},
    {"PAUSE", JOB_CONTROL_PAUSE},
    {"RESTART", JOB_CONTROL_RESTART},
    {"RESUME", JOB_CONTROL_RESUME},
    {"DELETE", JOB_CONTROL_DELETE},
    {"SENT-TO-PRINTER", JOB_CONTROL_SENT_TO_PRINTER},
    {"LAST-PAGE-EJECTED", JOB_CONTROL_LAST_PAGE_EJECTED}
};

// Asumiendo que PrinterInfo y JobInfo están definidos en printer_management.h, por ejemplo:
/// struct PrinterInfo {
///     std::wstring name;
///     std::wstring serverName;
///     std::wstring shareName;
///     std::wstring portName;
///     std::wstring driverName;
///     std::wstring comment;
///     std::wstring location;
///     DWORD status;
///     DWORD attributes;
///     DWORD jobs;
/// };
///
/// struct JobInfo {
///     DWORD id;
///     std::wstring document;
///     std::wstring userName;
///     DWORD status;
///     DWORD size;
///     DWORD pagesPrinted;
/// };

PrinterInfo parsePrinterInfo(PRINTER_INFO_2W* printer) {
    PrinterInfo info;
    info.name = printer->pPrinterName ? printer->pPrinterName : L"";
    info.serverName = printer->pServerName ? printer->pServerName : L"";
    info.shareName = printer->pShareName ? printer->pShareName : L"";
    info.portName = printer->pPortName ? printer->pPortName : L"";
    info.driverName = printer->pDriverName ? printer->pDriverName : L"";
    info.comment = printer->pComment ? printer->pComment : L"";
    info.location = printer->pLocation ? printer->pLocation : L"";
    info.status = printer->Status;
    info.attributes = printer->Attributes;
    info.jobs = printer->cJobs;
    return info;
}

JobInfo parseJobInfo(JOB_INFO_2W* job) {
    JobInfo info;
    info.id = job->JobId;
    info.document = job->pDocument ? job->pDocument : L"";
    info.userName = job->pUserName ? job->pUserName : L"";
    info.status = job->Status;
    info.size = job->Size;
    info.pagesPrinted = job->PagesPrinted;
    return info;
}

// -------------------- Funciones internas de PrinterManagement --------------------
namespace WinPrinterManagement {

    // Obtiene la lista de impresoras (sin JSON).
    std::vector<PrinterInfo> getPrinters() {
        std::vector<PrinterInfo> printers;
        DWORD needed = 0, count = 0;
        EnumPrintersW(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, NULL, 2, NULL, 0, &needed, &count);
        if (needed > 0) {
            std::unique_ptr<BYTE[]> buffer(new BYTE[needed]);
            if (EnumPrintersW(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, NULL, 2, buffer.get(), needed, &needed, &count)) {
                PRINTER_INFO_2W* info = reinterpret_cast<PRINTER_INFO_2W*>(buffer.get());
                for (DWORD i = 0; i < count; ++i) {
                    printers.push_back(parsePrinterInfo(&info[i]));
                }
            }
        }
        return printers;
    }

    // Obtiene el nombre de la impresora por defecto.
    std::wstring getDefaultPrinterName() {
        DWORD size = 0;
        GetDefaultPrinterW(NULL, &size);
        if (size > 0) {
            std::unique_ptr<WCHAR[]> buffer(new WCHAR[size]);
            if (GetDefaultPrinterW(buffer.get(), &size))
                return buffer.get();
        }
        return L"";
    }

    // Obtiene información de una impresora específica.
    // Devuelve true en caso de éxito; en caso de error, rellena winErr y errMsg.
    bool getPrinter(const std::wstring& printerName, PrinterInfo& outInfo, DWORD& winErr, std::wstring& errMsg, std::wstring& errStep) {
        PrinterHandle handle(const_cast<LPWSTR>(printerName.c_str()));
        if (!handle) {
            winErr = GetLastError();
            errMsg = formatWindowsError(winErr);
			errStep = L"OpenPrinterW";
            return false;
        }
        DWORD needed = 0;
        if (!GetPrinterW(handle, 2, NULL, 0, &needed)) {
            winErr = GetLastError();
            errMsg = formatWindowsError(winErr);
			errStep = L"GetPrinterW";
            return false;
        }
        std::unique_ptr<BYTE[]> buffer(new BYTE[needed]);
        if (!GetPrinterW(handle, 2, buffer.get(), needed, &needed)) {
            winErr = GetLastError();
            errMsg = formatWindowsError(winErr);
			errStep = L"GetPrinterW";
            return false;
        }
        outInfo = parsePrinterInfo(reinterpret_cast<PRINTER_INFO_2W*>(buffer.get()));
        winErr = 0;
        errMsg = L"";
		errStep = L"";
        return true;
    }

    // Obtiene información de un trabajo de impresión.
    bool getJob(const std::wstring& printerName, DWORD jobId, JobInfo& outJobInfo, DWORD& winErr, std::wstring& errMsg, std::wstring& errStep) {
        PrinterHandle handle(const_cast<LPWSTR>(printerName.c_str()));
        if (!handle) {
            winErr = GetLastError();
            errMsg = formatWindowsError(winErr);
			errStep = L"OpenPrinterW";
            return false;
        }
        DWORD needed = 0;
        if (!GetJobW(handle, jobId, 2, NULL, 0, &needed)) {
            winErr = GetLastError();
            errMsg = formatWindowsError(winErr);
			errStep = L"GetJobW";
            return false;
        }
        std::unique_ptr<BYTE[]> buffer(new BYTE[needed]);
        if (!GetJobW(handle, jobId, 2, buffer.get(), needed, &needed)) {
            winErr = GetLastError();
            errMsg = formatWindowsError(winErr);
			errStep = L"GetJobW";
            return false;
        }
        outJobInfo = parseJobInfo(reinterpret_cast<JOB_INFO_2W*>(buffer.get()));
        winErr = 0;
        errMsg = L"";
		errStep = L"";
        return true;
    }

    // Envía un comando a un trabajo de impresión.
    bool setJob(const std::wstring& printerName, DWORD jobId, const std::string& command, DWORD& winErr, std::wstring& errMsg, std::wstring& errStep) {
        auto it = jobCommands.find(command);
        if (it == jobCommands.end()) {
            winErr = 0;
            errMsg = L"Invalid job command";
			errStep = L"jobCommands.find";
            return false;
        }
        PrinterHandle handle(const_cast<LPWSTR>(printerName.c_str()));
        if (!handle) {
            winErr = GetLastError();
            errMsg = formatWindowsError(winErr);
			errStep = L"OpenPrinterW";
            return false;
        }
        BOOL result = SetJobW(handle, jobId, 0, NULL, it->second);
        if (!result) {
            winErr = GetLastError();
            errMsg = formatWindowsError(winErr);
			errStep = L"SetJobW";
            return false;
        }
        winErr = 0;
        errMsg = L"";
		errStep = L"";
        return true;
    }

    // Obtiene los comandos de trabajo soportados.
    std::vector<std::string> getSupportedJobCommands() {
        std::vector<std::string> commands;
        for (const auto& cmd : jobCommands) {
            commands.push_back(cmd.first);
        }
        return commands;
    }

    // Obtiene los formatos de impresión soportados.
	std::vector<std::wstring> getSupportedPrintFormats(DWORD& winErr, std::wstring& errMsg, std::wstring& errStep) {
        std::vector<std::wstring> formats;
        DWORD needed = 0, count = 0;
        if (!EnumPrintProcessorsW(NULL, NULL, 1, NULL, 0, &needed, &count)) {
			winErr = GetLastError();
			errMsg = formatWindowsError(winErr);
			errStep = L"EnumPrintProcessorsW";
            return formats;
        }
        if (needed > 0) {
            std::unique_ptr<BYTE[]> buffer(new BYTE[needed]);
            if (!EnumPrintProcessorsW(NULL, NULL, 1, buffer.get(), needed, &needed, &count)) {
                winErr = GetLastError();
                errMsg = formatWindowsError(winErr);
                errStep = L"EnumPrintProcessorsW";
                return formats;
            }
                PRINTPROCESSOR_INFO_1W* info = reinterpret_cast<PRINTPROCESSOR_INFO_1W*>(buffer.get());
                for (DWORD i = 0; i < count; ++i) {
                    DWORD dataNeeded = 0, dataCount = 0;
                    EnumPrintProcessorDatatypesW(NULL, info[i].pName, 1, NULL, 0, &dataNeeded, &dataCount);
                        if (dataNeeded <= 0) {
                            continue;
                        }

                        std::unique_ptr<BYTE[]> dataBuffer(new BYTE[dataNeeded]);
                        if (!EnumPrintProcessorDatatypesW(NULL, info[i].pName, 1, dataBuffer.get(), dataNeeded, &dataNeeded, &dataCount)) {
                            winErr = GetLastError();
                            errMsg = formatWindowsError(winErr);
                            errStep = L"EnumPrintProcessorDatatypesW";
                            return formats;
                        }
                            DATATYPES_INFO_1W* dataInfo = reinterpret_cast<DATATYPES_INFO_1W*>(dataBuffer.get());
                            for (DWORD j = 0; j < dataCount; ++j) {
                                formats.push_back(dataInfo[j].pName);
                            }
				}
        }
        return formats;
	}

    // Envía datos directamente a la impresora en modo RAW.
    // Devuelve true si tuvo éxito y asigna el ID del trabajo en outJobId.
    bool printDirect(const std::wstring& printerName, const std::string& data,
        const std::wstring& docName, const std::wstring& dataType,
        DWORD& outJobId, DWORD& winErr, std::wstring& errMsg, std::wstring& errStep) {

        PrinterHandle handle(const_cast<LPWSTR>(printerName.c_str()));
        if (!handle) {
            winErr = GetLastError();
            errMsg = formatWindowsError(winErr);
            errStep = L"OpenPrinterW";
            return false;
        }
        DOC_INFO_1W docInfo = { const_cast<LPWSTR>(docName.c_str()), NULL, const_cast<LPWSTR>(dataType.c_str()) };
        outJobId = StartDocPrinterW(handle, 1, reinterpret_cast<BYTE*>(&docInfo));
        if (outJobId == 0) {
            winErr = GetLastError();
            errMsg = formatWindowsError(winErr);
            errStep = L"StartDocPrinterW";
            return false;
        }
        if (!StartPagePrinter(handle)) {
            winErr = GetLastError();
            errMsg = formatWindowsError(winErr);
            EndDocPrinter(handle);
            errStep = L"StartPagePrinter";
            return false;
        }
        DWORD written = 0;
        if (!WritePrinter(handle, (LPVOID)data.data(), (DWORD)data.size(), &written) || written != data.size()) {
            winErr = GetLastError();
            errMsg = formatWindowsError(winErr);
            EndPagePrinter(handle);
            EndDocPrinter(handle);
            errStep = L"WritePrinter";
            return false;
        }
        EndPagePrinter(handle);
        EndDocPrinter(handle);
        winErr = 0;
        errMsg = L"";
        errStep = L"";
        return true;
    }


    // -------------------- Wrappers JSON --------------------

    // getPrintersJson
    std::wstring getPrintersJson() {
        auto printers = getPrinters();

		if (printers.empty()) {
			DWORD winErr = GetLastError();
			std::wstring errMsg = formatWindowsError(winErr);
			return buildJsonResult(1, errMsg, winErr, L"[]", L"EnumPrintersW");
		}

        std::wostringstream response;
        response << L"[";
        bool first = true;
        for (const auto& printer : printers) {
            if (!first)
                response << L",";
            first = false;
            response << L"{";
            response << L"\"name\":\"" << printer.name << L"\",";
            response << L"\"serverName\":\"" << printer.serverName << L"\",";
            response << L"\"shareName\":\"" << printer.shareName << L"\",";
            response << L"\"portName\":\"" << printer.portName << L"\",";
            response << L"\"driverName\":\"" << printer.driverName << L"\",";
            response << L"\"comment\":\"" << printer.comment << L"\",";
            response << L"\"location\":\"" << printer.location << L"\",";
            response << L"\"status\":" << printer.status << L",";
            response << L"\"attributes\":" << printer.attributes << L",";
            response << L"\"jobs\":" << printer.jobs;
            response << L"}";
        }
        response << L"]";
        return buildJsonResult(0, L"", 0, response.str(), L"");
    }

    // getDefaultPrinterNameJson
    std::wstring getDefaultPrinterNameJson() {
        std::wstring name = getDefaultPrinterName();
        if (name.empty()) {
            DWORD winErr = GetLastError();
            std::wstring errMsg = formatWindowsError(winErr);
            return buildJsonResult(1, errMsg, winErr, L"null", L"GetDefaultPrinterW");
        }
        else {
            std::wostringstream oss;
            oss << L"\"" << name << L"\"";
            return buildJsonResult(0, L"", 0, oss.str(), L"");
        }
    }

    // getPrinterJson
    std::wstring getPrinterJson(const std::wstring& printerName) {
        PrinterInfo info;
        DWORD winErr = 0;
        std::wstring errMsg;
        std::wstring errStep;
        if (!getPrinter(printerName, info, winErr, errMsg, errStep)) {
            return buildJsonResult(1, errMsg, winErr, L"{}", errStep);
        }
        std::wostringstream oss;
        oss << L"{";
        oss << L"\"name\":\"" << info.name << L"\",";
        oss << L"\"serverName\":\"" << info.serverName << L"\",";
		oss << L"\"shareName\":\"" << info.shareName << L"\",";
        oss << L"\"portName\":\"" << info.portName << L"\",";
        oss << L"\"driverName\":\"" << info.driverName << L"\",";
        oss << L"\"comment\":\"" << info.comment << L"\",";
        oss << L"\"location\":\"" << info.location << L"\",";
        oss << L"\"status\":" << info.status << L",";
        oss << L"\"attributes\":" << info.attributes << L",";
        oss << L"\"jobs\":" << info.jobs;
        oss << L"}";
        return buildJsonResult(0, L"", 0, oss.str(), L"");
    }

    // getJobJson
    std::wstring getJobJson(const std::wstring& printerName, DWORD jobId) {
        JobInfo info;
        DWORD winErr = 0;
        std::wstring errMsg;
        std::wstring errStep;
        if (!getJob(printerName, jobId, info, winErr, errMsg, errStep)) {
            return buildJsonResult(1, errMsg, winErr, L"{}", errStep);
        }
        std::wostringstream oss;
        oss << L"{";
        oss << L"\"id\":" << info.id << L",";
        oss << L"\"document\":\"" << info.document << L"\",";
        oss << L"\"userName\":\"" << info.userName << L"\",";
        oss << L"\"status\":" << info.status << L",";
        oss << L"\"size\":" << info.size << L",";
        oss << L"\"pagesPrinted\":" << info.pagesPrinted;
        oss << L"}";
        return buildJsonResult(0, L"", 0, oss.str(), L"");
    }

    // setJobJson
    std::wstring setJobJson(const std::wstring& printerName, DWORD jobId, const std::string& command) {
        DWORD winErr = 0;
        std::wstring errMsg;
        std::wstring errStep;
        bool result = setJob(printerName, jobId, command, winErr, errMsg, errStep);
        if (!result) {
            return buildJsonResult(1, errMsg, winErr, L"null", errStep);
        }
        return buildJsonResult(0, L"", 0, result ? L"true" : L"false", L"");
    }

    // getSupportedJobCommandsJson
    std::wstring getSupportedJobCommandsJson() {
        std::wostringstream oss;
        try {
            auto cmds = getSupportedJobCommands();
            oss << L"[";
            bool first = true;
            for (const auto& cmd : cmds) {
                if (!first)
                    oss << L",";
                first = false;
                // Convertimos el std::string a std::wstring
                std::wstring wcmd(cmd.begin(), cmd.end());
                oss << L"\"" << wcmd << L"\"";
            }
            oss << L"]";
            return buildJsonResult(0, L"", 0, oss.str(), L"getSupportedJobCommands");
        }
        catch (...) {
            return buildJsonResult(1, L"Error getting supported job commands", 0, L"[]", L"TryCatch");
        }
    }

    // getSupportedPrintFormatsJson
    std::wstring getSupportedPrintFormatsJson() {
        std::wostringstream oss;
        try {
            DWORD winErr = 0;
            std::wstring errMsg;
            std::wstring errStep;
            auto fmts = getSupportedPrintFormats(winErr, errMsg, errStep);
			if (fmts.empty()) {
				return buildJsonResult(1, errMsg, winErr, L"[]", errStep);
			}
            oss << L"[";
            bool first = true;
            for (const auto& fmt : fmts) {
                if (!first)
                    oss << L",";
                first = false;
                oss << L"\"" << fmt << L"\"";
            }
            oss << L"]";
            return buildJsonResult(0, L"", 0, oss.str(), L"");
        }
        catch (...) {
            return buildJsonResult(1, L"Error getting supported print formats", 0, L"[]", L"TryCatch");
        }
    }

    // printDirectJson
    std::wstring printDirectJson(const std::wstring& printerName, const std::string& data,
        const std::wstring& docName, const std::wstring& dataType) {
        DWORD jobId = 0;
        DWORD winErr = 0;
        std::wstring errMsg;
        std::wstring errStep;
        if (!printDirect(printerName, data, docName, dataType, jobId, winErr, errMsg, errStep)) {
            return buildJsonResult(1, errMsg, winErr, L"null", errStep);
        }
        std::wostringstream oss;
        oss << jobId;
        return buildJsonResult(0, L"", 0, oss.str(), L"");
    }
} // namespace PrinterManagement
