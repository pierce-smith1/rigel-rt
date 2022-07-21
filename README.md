# rigel-rt

`rigel-rt` is a Windows application that corrupts the memory of a process in real time. 
It accomplishes this by scanning through the process's entire virtual address space every
few seconds, finding contiguous regions of private commited memory (`MEM_COMMIT | MEM_PRIVATE`)
and twiddling a random byte in each region.

This occurs with unspectacular results. Unless you are unfathomably lucky, the process
will simply crash. Don't expect anything fun to happen.

## Build

On a Visual Studio command prompt, run:

```
build.bat
```

This will produce a `rigel.exe` executable.

## Run

`rigel` takes a single argument, which is the program to spawn for corruption. For instance,
to spawn a new Notepad and start destroying it, use:

```
rigel C:\Windows\notepad.exe
```

You cannot attach to an already running process.

## Future Work?

I plan to come back to this once I can find a more interesting way to execute this idea,
since for now, it is very unexciting.
