# Client joins the server
The client connects to the apache kafka cluster. The master server issues a UUID and the client validates its connection by acknoledging that it recognizes this UUID as its own.

- client joins ands picks up a UUID from the UUID topic
- client publishes the choosen UUID to master.hello.clients topic
- master executes gamedev hook to find out the spawn position, and determines which server the client should be assigned to
- the master notifies the server of the client's arrival (SN takes authority) and spwn position
- the master notifies the client of the UUID of the SN it managing it
- the client starts listening for events on the variouts SN's topics. (such events may include spawn notifications)

Potential issues:
- synchronizing the procedure so that the client does not miss the notification of its own spawn on first connection.