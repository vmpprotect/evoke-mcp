# evoke-mcp

small mcp server for low-level memory tooling.

it talks to your kernel driver and exposes a few simple routines over HTTP:

## small feature list

- `get_base_address`
- `read_memory`
- `write_memory`
- can be expanded based on your driver.
 - in theroy you can do like DLL injection, based on your prompt.

## stack

- C++
- Windows APIs
- JSON-RPC style MCP handlers
- `cpp-httplib` for transport

## notes

- currently set up around `FortniteClient-Win64-Shipping.exe` in `main.cpp`.
- server listens on `127.0.0.1:8888`. and uses streamable http for the MCP responses.
- intended for local tooling / reversing workflows.
- this can be paired **really** well with [mrexodia's IDA MCP](https://github.com/mrexodia/ida-pro-mcp)

## examples

[Fortnite](https://github.com/vmpprotect/evoke-mcp/wiki/Fortnite-Example)

## build

open the `.vcxproj` in Visual Studio and build for your target config.
