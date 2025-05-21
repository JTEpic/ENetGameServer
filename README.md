# ENetGameServer

One can directly compile and run (as an executable) the server code if they attach the ENet library
Beware as no packet is currently checked or cleaned

The client code requires you to have a class in place holding player ID, position, and yaw. The ID I set was simply the time they started the program at
Again no recieved packet is checked or cleaned
glfwGetTime() can be replaced with the chrono time implementation
