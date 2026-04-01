# Webserv Architecture

## The Big Picture

```
Client (browser) → TCP socket → Listener → EventLoop → ClientConnection → RequestParser → Router → Handler → Response
```

A browser never sends just one request per page load.
Every image, CSS file, and script is a separate TCP connection — each with its own fd.

---

## What is an fd?

fd = File Descriptor. Just a number the OS assigns to an open "thing".
In Unix, everything is a file — including network connections.

```
fd 0 → stdin
fd 1 → stdout
fd 2 → stderr
fd 3 → server listening socket
fd 4 → connection with client A
fd 5 → connection with client B
```

---

## What is a socket?

A socket is the endpoint of a connection — like a phone plug.

```
socket() → create the plug (server gets an fd)
bind()   → give it a number (port 8080)
listen() → turn it on, ready to ring
accept() → pick up! → new fd for that specific caller
```

The original socket (fd 3) keeps listening forever.
Every accept() returns a new fd for that specific connection.

---

## What is poll()?

poll() watches all open file descriptors at once and asks each one: "are you ready?"

Instead of blocking on one fd and missing others:
```
poll() asks every fd: "are you ready?"
→ fd 4: yes, data incoming!  → onReadable()
→ fd 5: not yet
→ fd 6: ready to write!      → onWritable()
```

poll() only returns when at least one fd is ready.
The server never reads or writes without poll() saying it is safe.

---

## Classes and their roles

### ServerManager
Top-level orchestrator. Creates Listener, ConnectionStore and EventLoop.
Entry point: ServerManager::run()

### Listener
One listening socket on a port.
Calls accept() when a new client connects → returns new fd.

### EventLoop
The heart of the server. Runs the poll() loop forever.
Each iteration:
1. rebuildPollFds() — collect all fds + what they want (POLLIN / POLLOUT)
2. poll()           — wait until at least one fd is ready
3. dispatchEvents() — call onReadable() or onWritable() on the right connection

### ConnectionStore
Owns all active ClientConnection objects.
Uses a map: fd → ClientConnection.
Handles adding and removing connections cleanly (RAII via unique_ptr + Fd).

### ClientConnection
Represents one TCP connection with one client.
Is a state machine — always in exactly one of three states:

```
[Reading]  → receiving bytes from client
[Writing]  → sending response back to client
[Closing]  → done, EventLoop will remove this connection
```

State transitions:
```
Reading → Writing  : request complete, response queued
Writing → Reading  : all bytes sent + keep-alive
Writing → Closing  : all bytes sent + connection: close
Reading → Closing  : client disconnected or bad request
```

### RequestParser  (Person 2)
Parses incoming bytes incrementally into an HttpRequest.
Why incremental? TCP may split one HTTP request across multiple recv() calls.
Sets keepAlive = true/false based on Connection header.

### Router + Handlers  (Person 3)
Router decides which Handler to use based on request path and method.
Handlers execute the actual logic (static file, upload, CGI).

### HttpRequest / HttpResponse  (shared interface)
HttpRequest  = output of RequestParser  → input for Router/Handlers
HttpResponse = output of Handlers       → input for ClientConnection (serialize + send)

---

## Who uses who?

```
ServerManager
├── Listener
├── ConnectionStore
│   └── ClientConnection  (one per fd)
│       ├── RequestParser
│       └── Router
│           └── Handlers
└── EventLoop
    ├── Listener (ref)
    └── ConnectionStore (ref)
```

---

## Responsibility split

| Person | Area        | Classes                                                        |
|--------|-------------|----------------------------------------------------------------|
| Person 1 | Networking | ServerManager, Listener, EventLoop, ConnectionStore, ClientConnection |
| Person 2 | HTTP Layer  | RequestParser, HttpRequest, HttpResponse                       |
| Person 3 | Config & Routing | Router, Handlers, RouteDecision                          |
