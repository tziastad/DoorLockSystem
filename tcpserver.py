# Socket server in python using select function


import socket, select
import sqlite3
import sys
import traceback

import Crypto
from Crypto.Cipher import PKCS1_OAEP
from Crypto.PublicKey import RSA
from Crypto import Random
from Crypto.PublicKey import RSA
from Crypto.Cipher import AES
from Crypto import Random
import rsa
import numpy as np




def search_for_pair_in_database(device_id,card_id):
    # connect with database
    conn = sqlite3.connect(r"C:\Users\Dora\Desktop\Start Diplomatiki\DoorLock.db")


    print("pair: ",device_id,"-", card_id)

    cur = conn.cursor()

    cur.execute("SELECT DoorID FROM `User` WHERE ID = ?", [card_id])

    result = cur.fetchone()
    if (result == None):
        return "Sorry, you can't access."
    else:
        for row in result:
            if(row==device_id):
                return "Door is opened."

def search_for_device_id_in_database(device_id):
    # connect with database
    conn = sqlite3.connect(r"C:\Users\Dora\Desktop\Start Diplomatiki\DoorLock.db")

    cur = conn.cursor()

    cur.execute("SELECT ID FROM `Door` WHERE ID = ?", [device_id])

    result = cur.fetchone()
    if (result == None):
        return "Sorry, this device id is unknown."
    else:
        for row in result:
            if(row==device_id):
                return "Device id is known."


def main():
    CONNECTION_LIST = []  # list of socket clients
    RECV_BUFFER = 4096  # Advisable to keep it as an exponent of 2
    PORT = 8080

    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    # this has no effect, why ?
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_socket.bind(("0.0.0.0", PORT))
    server_socket.listen(1)

    # Add server socket to the list of readable connections
    CONNECTION_LIST.append(server_socket)

    print("Chat server started on port " + str(PORT))


    #RSA KEYS
    #link: https://www.geeksforgeeks.org/how-to-encrypt-and-decrypt-strings-in-python/

    key = RSA.generate(1024)
    public = key.publickey().exportKey("DER")
    print(len(public))
    print(public)
    print(public.hex())
    #decMessage = rsa.decrypt(encMessage, privateKey).decode()

    while 1:
        # Get the list sockets which are ready to be read through select
        read_sockets, write_sockets, error_sockets = select.select(CONNECTION_LIST, [], [])

        for sock in read_sockets:

            # New connection
            if sock == server_socket:
                # Handle the case in which there is a new connection recieved through server_socket
                sockfd, addr = server_socket.accept()
                CONNECTION_LIST.append(sockfd)
                print("Client (%s, %s) connected" % addr)

            # Some incoming message from a client
            else:
                # Data recieved from client, process it
                try:
                    # In Windows, sometimes when a TCP program closes abruptly,
                    # a "Connection reset by peer" exception will be thrown
                    data = sock.recv(RECV_BUFFER)
                    print("len of data:", len(data))
                    dataAsHex=data.hex()
                    access_message=""
                    #print("data as hex:", dataAsHex)
                    if (data[0:1].decode("utf-8") == "!"):
                        print("message is:", data.decode("utf-8"))
                        #sockfd.send(public[26:247])
                        sockfd.send(public)

                    elif(data[0:1].decode("utf-8")=="#"):
                        print("Data device:", data[0:1].decode("utf-8") + dataAsHex[2:len(dataAsHex)])
                        device_id=dataAsHex[2:len(dataAsHex)]
                        #print(device_id)
                        access_message = search_for_device_id_in_database(device_id)
                        sockfd.send(access_message.encode())
                    elif(data[0:1].decode("utf-8")=="@"):
                        print("Data card:", data[0:1].decode("utf-8") + dataAsHex[2:len(dataAsHex)])
                        card_id = dataAsHex[2:len(dataAsHex)]
                        print(card_id)
                        access_message=search_for_pair_in_database(device_id,card_id)
                        sockfd.send(access_message.encode())

                # client disconnected, so remove from socket list
                except Exception:

                    print(traceback.format_exc())
                    # broadcast_data(sock, "Client (%s, %s) is offline" % addr)
                    print("Client (%s, %s) is offline" % addr)
                    sock.close()
                    CONNECTION_LIST.remove(sock)
                    continue
    conn.close()
    server_socket.close()


if __name__ == "__main__":
    main()
