#pragma once
#ifndef CLIENT_H
#define CLIENT_H

#include <enet/enet.h>
#include <stdio.h>
#include <optional>
#include <player.h>

struct playerData {
  int ID; // Identifiable Name
  glm::vec3 position;
  float yaw;
};

class Client {
  
  public:

      ENetHost *Eclient;

      ENetAddress address;
      ENetEvent event;
      ENetPeer *peer;

      bool connected;

      Client(){
          if (enet_initialize () != 0) {
              printf("An error occurred while initializing ENet.\n");
              exit(EXIT_FAILURE);
          }

          Eclient =
              enet_host_create(NULL /* create a client host */,
                              1 /* only allow 1 outgoing connection */,
                              2 /* allow up 2 channels to be used, 0 and 1 */,
                              0 /* assume any amount of incoming bandwidth */,
                              0 /* assume any amount of outgoing bandwidth */);
          if (Eclient == NULL) {
            fprintf(stderr, "An error occurred while trying to create an ENet "
                            "client host.\n");
            exit(EXIT_FAILURE);
          }

          //ENetAddress address;
          //ENetEvent event;
          //ENetPeer *peer;
          /* Connect to some.server.net:1234. */
          enet_address_set_host(&address, "127.0.0.1");
          address.port = 7777;
          connected=false;
          connect();

          // Receive some events
          //enet_host_service(Eclient, &event, 5000);
      }

      // Try to connect to server
      void connect() {
        /* Initiate the connection, allocating the two channels 0 and 1. */
        peer = enet_host_connect(Eclient, &address, 2, 0);

        if (peer == NULL) {
          connected=false;
          fprintf(stderr,"No available peers for initiating an ENet connection.\n");
          exit(EXIT_FAILURE);
        }

        /* Wait until timeout for the connection attempt to succeed. */
        if (enet_host_service(Eclient, &event, 50) > 0 && event.type == ENET_EVENT_TYPE_CONNECT) {
          connected=true;
          puts("Connection to some.server.net:1234 succeeded.");
        } else {
          /* Either the time is up or a disconnect event was */
          /* received. Reset the peer in the event the time  */
          /* had run out without any significant event.      */
          enet_peer_reset(peer);
          connected=false;
          puts("Connection to some.server.net:1234 failed.");
        }
      }

      // Ping server, peer->state update may have delay
      bool pingIt() {
        // Check if the peer is connected
        if (peer->state == ENET_PEER_STATE_CONNECTED) {
          connected=true;
          printf("Peer is connected.\n");
        } else {
          connected=false;
          printf("Peer is not connected.\n");
        }
        return connected;
      }

      // Send player information to server
      void send(Player *player){
        /* Create a reliable packet of size 7 containing "packet\0" */
        //ENetPacket * packet = enet_packet_create ("packet", 
        //                                    strlen ("packet") + 1, 
        //                                    ENET_PACKET_FLAG_RELIABLE);
        playerData data = {player->ID, player->position, player->yaw};

        ENetPacket * packet = enet_packet_create (&data, 
                                            sizeof(playerData), 
                                            ENET_PACKET_FLAG_RELIABLE);
        //std::cout << data.position.x << std::endl;
  
        /* Send the packet to the peer over channel id 0. */
        /* One could also broadcast the packet by         */
        /* enet_host_broadcast (host, 0, packet);         */
        enet_peer_send (peer, 0, packet);

        /* One could just use enet_host_service() instead. */
        enet_host_flush (Eclient);
      }

      // Recieve player information from server, likely set timeout to 0, currently gets one packet per frame
      int recievedPackets = 0;
      double prevRecieve = glfwGetTime();
      std::optional<playerData> recieve(){
        while (enet_host_service(Eclient, &event, 10) > 0) {
          switch (event.type) {
          case ENET_EVENT_TYPE_RECEIVE:
            double currentTime = glfwGetTime();
            recievedPackets++;
            if(currentTime-prevRecieve >= 1.0){
              printf("%d packets recieved since last.\n", recievedPackets);
              recievedPackets = 0;
              prevRecieve = currentTime;
            }
            /*
            printf("A packet of length %u containing %s was received from %x:%u "
                  "on channel %u.\n",
                  event.packet->dataLength, event.packet->data,
                  event.peer->address.host, event.peer->address.port,
                  event.channelID);
            */
            //return casted data
            playerData player = *(playerData*)event.packet->data;
            //std::cout << player.position.x << std::endl;
            /* Clean up the packet now that we're done using it. */
            enet_packet_destroy (event.packet);
            //delete playerP;
            return player;
            //return NULL;
            break; //needed?
          }
        }
        // Did not recieve anything
        return std::nullopt;
      }

      void disconnect(){
        if(connected==true){
          // Disconnect
          enet_peer_disconnect(peer, 0);

          uint8_t disconnected = false;
          /* Allow up to 3 seconds for the disconnect to succeed
          * and drop any packets received packets.
          */
          while (enet_host_service(Eclient, &event, 3000) > 0) {
            switch (event.type) {
              case ENET_EVENT_TYPE_RECEIVE:
                enet_packet_destroy(event.packet);
                break;
              case ENET_EVENT_TYPE_DISCONNECT:
                puts("Disconnection succeeded.");
                disconnected = true;
                break;
            }
          }

          // Drop connection, since disconnection didn't successed
          if (!disconnected) {
            enet_peer_reset(peer);
          }
          connected=false;
        }

        enet_host_destroy(Eclient);
        enet_deinitialize();
      }
  
  private:

};

#endif