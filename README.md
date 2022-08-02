# Dynamic Web Server

This project has a three-tier architecture, with http-server as the application layer and mdb-lookup-server as the database layer. 

mdb-lookup-server can take in search terms from multiple clients and output matching messages from a message database(mdb). It communicates with the clients via TCP sockets.

http-server works in conjunction with mdb-lookup-server to serve static HTML and image files, as well as dynamic lookup results from a message database. It acts as a TCP server to the web page and as a TCP client to mdb-lookup-server.
