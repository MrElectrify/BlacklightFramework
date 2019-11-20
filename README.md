# BlacklightFramework
BlacklightFramework is a framework of tools created for and used in the development of Blacklight Software. All of the code is cross-platform. I did not initially have intentions of releasing any of this code, but with the end of development of the associated project, I felt it was fit for the public domain. Not everything is complete.

## BlacklightBase [Complete]
BlacklightBase is a base for a modular dynamic library that places hooks in the binary that it is loaded into. It has a central backbone that manages all of the modules, and an API For creating modules with arguments, as well as destroying them

## BlacklightCrypto [Complete]
BlacklightCrypto is a cryptography library which implements RSA of an arbitrary bitcount using MPIR and PicoSHA2, as well as a wrapper for arbitrary bit AES-GCM using CryptoPP

## BlacklightHooks [99%]
BlacklightHooks is a hooking library which allows for runtime hooking of functions using various methods, such as Detours, Virtual Detours, Virtual Modification, and Virtual Replacement. x86-64 detours have a limitation in creation of a trampoline with a larger-than-32-bit-signed displacement (more than 0x7fffffff bytes away), but otherwise is functionally complete. Both type of detours use Zydis for disassembling x86 instructions.

## BlacklightSockets [Complete]
BlacklightSockets is a networking library which is mostly deprecated outside of BLESocket. It implements an ASIO-inspired TCP and TLS socket which is designed to be polymorphic to add a layer of customization with how connections are handled and data is transmitted, with the exception that it does not use WinIOCP or the linux counterpart for networking (I will leave that to those who do networking for a living), and is therefore deprecated for performance reasons (there are also a few bugs that are sure to show up under extensive use). BLESocket implements an encrypted socket using `boost::asio` (which can easily be changed out with `asio` by @chriskohlhoff) with a handshake over RSA-4096 and bidirectional data encryption over AES-256-GCM (a little overkill, I know, change the templated bitcount if that bothers you). It has been extensively tested for stability and is used in many of my applications that require security with the perk that it uses an unknown protocol, unlike SSL/TLS, and provides a high level of security against brute-force attacks.

## BlacklightStringify [Complete]
BlacklightStringify is an internal tool which turns any file into either `std::array<unsigned char>` or `unsigned char[]` for uses in applications which need to store another file internally (think runtime executable decryption)

## BlacklightVM [~80%]
BlacklightVM is a virtual architecture that is designed to obfuscate code, primarily designed around networking code, with a trap opcode which calls primarily networking functions. It has 16 registers, and is partially inspired by x86, with elements of other architectures. It has an opcode to call an x86 function to further obfuscate code and make it difficult to understand, from a function table. The executor is not entirely complete as it does not support the trap table and only supports x86 function calls (`__stdcall`, `__fastcall`, and `__cdecl`) on Windows, but otherwise it is functionally complete, and provides an interface for constructing a buffer from opcode factories. Documentation exists in `docs/`.
