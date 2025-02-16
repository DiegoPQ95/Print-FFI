# Print-FFI

Yo, I used to work with [tojocky/node-printer](https://github.com/tojocky/node-printer/tree/master) back in the day on Node v6. Now I'm switching to Bun, and I still need the same functionality and interface. So, I decided to build this project. Tried to keep the same vibes as the original.  
__Right now, it's only working on Windows.__

# What's this for?
I needed a way to print RAW esc/pos commands to local printers. Nothing fancy, just some good ol' printer action.

My dev setup:
- Visual Studio 2022
- Windows 10/Windows 11 (both x64)
- Bun 1.2.2

### Prerequisites
- Install Visual Studio 2022 (the Community edition works fine). Make sure to add C/C++ development and the Windows SDK 10.0.18362.0 (for **Windows 10** compatibility).
- Build, baby, build!

### What's in the project?
- `dllmain.cpp`: This is the entry point for the library.
- `win_printer_management.cpp`: The Windows printer class manager. I added a little extra to the original tojocky project with the `functionname**Json**` to return a JSON string.
- `convert_string_to_utf8`: Exactly what it says on the tin.

### Integrating with Bun
Example:
```ts
const lib = dlopen("/path/to/your/compiled-library", {
    GetPrintersJson: { args: [], returns: FFIType.pointer },
    GetDefaultPrinterNameJson: { args: [], returns: FFIType.pointer },
    GetPrinterJson: { args: [FFIType.pointer], returns: FFIType.pointer },
    GetJobJson: { args: [FFIType.pointer, FFIType.u32], returns: FFIType.pointer },
    SetJobJson: { args: [FFIType.pointer, FFIType.u32, FFIType.pointer], returns: FFIType.pointer },
    GetSupportedJobCommandsJson: { args: [], returns: FFIType.pointer },
    GetSupportedPrintFormatsJson: { args: [], returns: FFIType.pointer },
    PrintDirectJson: { args: [FFIType.pointer, FFIType.pointer, FFIType.pointer, FFIType.pointer], returns: FFIType.pointer },
    FreeString: { args: [FFIType.pointer], returns: FFIType.void },
});
```

### Notes
So, this project handles strings as `utf-16le` (which in C++ is `std::wstring`/`wchar_t`), since that's how Windows APIs like to work.  
BUT, my project was already vibing with `ESC/POS` encoded in `utf8`, so I made sure the `printDirect` function accepts `char_t*` instead of `wchar_t`.  
Also, from Bun's side, I had to send the strings as Pointers (encoded in `utf-16le`). It’s all a bit of a dance, but it works.

### How I handle string allocation
I made a little helper function for you:
```ts
function AllocString(value: string, encoding: "utf8" | "utf-16le" = "utf-16le") {
    return ptr(Buffer.from(value + "\0", encoding));
}
```

And I use it like this:
```ts
PrinterFFI.printDirect(
  AllocString("MY_PRINTER_NAME"),
  AllocString("document_name_in_printer_queue"),
  AllocString("HELLO WORLD | RATE STAR \n\n\n\n\n", "utf8")
)
```

## Why not just use `bun:ffi`'s `cc` function?
Trust me, I tried.  
BUT!  
The `tinyCC` embedded in Bun (apparently) doesn't search all the Windows SDK paths, so you have to manually add them. I couldn’t even get it to compile with `tinyCC`.

### Why Visual Studio 2022?
- Compiling with `tinyCC` takes *way* too much effort (you have to manually mess with all the SDK paths).
- Compiling with `gcc` works, but Bun doesn’t recognize the DLL properly. No idea why.
- Visual Studio 2022 just works! Like a charm.

TinyCC compiled DLLs work too, but the hassle with SDK paths was a no-go for me. I couldn’t get that to click.

### Contributions
I hope this helps you out, and feel free to contribute if you're feeling generous. Any feedback or comments are super welcome!

Also, just a heads up: this project was 70% coded by AI. I’m **not** a C/C++ wizard, so keep that in mind when you dive into it. 😅
