# Introduction

This document describes the naming convention of all the objects in the code. This document is valid for all languages used in the project. Please refer to it when reviewing pull requests.

# Conventions
The global naming convention should follow camel case.
## Variables

Variable names should be explicit and describe what the variable does. Abbreviations are accepted if they are obvious.
 **Public** variables are not prefixed with anything.
 ```C++
public:
    int clientId;
 ```
 **Private** variables are prefixed with an underscore.
 ```C++
private:
    int _clientId;
 ```
Variables that are **static** **const** should have an ALL CAPS name.
 ```C++
static const SERVER = celte::serv::UDPServer();
 ```

## Enumerations

The values of all Enumerations should be written in capital letters.

```C++
enum TYPE {
    FRIED_CHICKEN,
    PORK
}
```

## Methods

**Public** methods' names start with a capital letter.
 ```C++
public:
    int GetClientId();
 ```
 **Private** methods' names are prefixed with double underscores (__).
  ```C++
private:
    int __getClientId();
 ```

 Methods that will perform a IO operation involving Sockets should be prefixed with IO. This is valid too if the socket is called later in the call stack.

  ```C++
int IOFoo() {
    __IOBar();
    return 0;
}

void __IOBar() {
    write(this.socket, "hello", 5);
}

int __doMaths() {
    return 1 + 1;
}
 ```

 All methods and functions **must** have a comment explaining their purpose and giving indications about error handling and usage.
 If the method is declared in a header file and implemented elsewhere, the comment should be in the header file.

## Special classes

Interfaces' names should begin with an 'I' and abstract classes' names should begin with an 'A'.

 ```C++
class IClient; // interface
class AClient; // abstract class
class Client;  // instantiable class
 ```

## Files

Files are named on a per-language basis.
C++ and C# have snakeCase naming for their files.
Python has camel case.
Files that are tests should begin by 'test_'

```bash
test_ServerNodeUp.cs
serverNodeUp.cpp
server_node_up.py
```