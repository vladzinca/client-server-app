# Client Server App

## 💬 What is it?

This is a C application that manages messages in a [client-server model](https://en.wikipedia.org/wiki/Client%E2%80%93server_model) implemented using TCP and UDP.

The topology works by having a server which opens two sockets, one TCP and one UDP. It further functions as a broker between **TCP clients**, who can subscribe or unsubscribe to topics by sending messages to the server, and **UDP clients**, who send the server messages labeled with a certain topic and expect the server to further the messages to all the TCP clients subscribed to it.

![image](https://user-images.githubusercontent.com/74200913/224403585-7dd33090-e948-4421-bf64-253670409ccd.png)

I worked on this for 3 days during May 2022, and it encapsulates my expanding knowledge of transport protocols and their concrete implementation.

## 🦾 How to run it?

1.  Clone this repository. 
2.  Run automatic tests by using `python3 test.py` (I did not write the script myself).
3.  Enjoy!

## 📖 How does it work?

### Server

The server is implemented in `server.c` and starts by using the command `./server <PORT>`.

It then monitors when TCP clients go online and offline, and outputs this as such:

```
New client <ID> connected from <IP>:<PORT>.
Client <ID> disconnected.
```

The only command the server takes is `exit`, which closes the server and all the TCP clients connected to it.

Other than these, the server can receive messages from the UDP clients that have 3 fields:

1.  A `topic` field that is a string of less than 50 characters.
2.  A `data_type` field that is an unsigned int of 1 byte.
3.  A `valoare_mesaj` field, which contains the content of the message, and can not have more than 1500 characters.

### TCP Client

The TCP client is implemented in `subscriber.c` and starts by using the command `./subscriber ID IP PORT`.

If a TCP client attempts to connect again to the server while being connected, the server will output the following message:

```
Client <ID> already connected.
```

The commands a TCP client can take are either `subscribe <TOPIC> <SF>`, `unsubscribe <TOPIC>`, and this will lead it to output `Subscribed to topic.` or `Unsubscribed from topic.` respectively. It can also take the command `exit`.

If `<SF>` (store-and-forward) is turned to 1, it means that the client will receive the messages even if they are not connected, the next time they log in.

When it gets a message from an UDP client via the server, the TCP client will print it as:

```
<IP>:<PORT> - <TOPIC> - <DATA_TYPE> - <VALOARE_MESAJ>
```

### UDP Client

I received this one and I did not code it myself. Find it in `pcom_hw2_udp_client/`.

## 🏆 What did I achieve?

I remember working 14 hours non-stop the day before the deadline, and even though I was scared I wouldn't be able to finish it on time, I actually did.

As you can see from the code files, the app is pretty complex, and debugging usually meant re-running the checker, which was slow (I was working inside a VM) and would sometimes break, and so I would have to restart the VM and try again.

This made the whole process very draining, but what's important is that I actually did it. I remember telling myself that if it were easy, everyone would be doing it.

Afterwards, and mainly because of this project, I felt (and still feel) like I could do anything with code, given the proper time. And this, to me, is a big achievement.
