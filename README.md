# Client-Server-Resource-Retrieval-Application
Client-server application written in C, demonstrating the fundamentals of network programming.

###  Simple Server:
Implemented a basic HTTP server in C. This server sets up a TCP connection, listens for incoming client requests, and processes these requests. The server reads HTTP requests, parses them, and sends back a HTTP response.

###  Multi-threaded Server:
Objective: Upgrades the simple server to a multi-threaded version, allowing it to handle multiple client connections concurrently. This multi-threaded approach enables the server to serve multiple clients simultaneously, increasing its capacity and efficiency. To do this, a thread pool is implemented to efficiently manage client connections. The main server thread accepts connections and then passes the communication responsibility to worker threads. The new additions include a thread-safe queue to manage connection sockets between the main thread and the worker threads, ensuring proper synchronization.
