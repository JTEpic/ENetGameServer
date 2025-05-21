//#define ENET_IMPLEMENTATION
#include <enet/enet.h>
#include <stdio.h>
#include <chrono>
#include <vector>

int main(void)
{
  if (enet_initialize () != 0) {
    printf("An error occurred while initializing ENet.\n");
    return 1;
  }

  ENetAddress address = {0};

  address.host = ENET_HOST_ANY; /* Bind the server to the default localhost.     */
  address.port = 7777; /* Bind the server to port 7777. */

  #define MAX_CLIENTS 12

  /* create a server */
  ENetHost * server = enet_host_create(&address, MAX_CLIENTS, 2, 0, 0);

  if (server == NULL) {
    printf("An error occurred while trying to create an ENet server host.\n");
    return 1;
  }

  typedef struct {
    ENetPeer *peers[MAX_CLIENTS]; // Array to hold pointers to ENetPeer objects
    int count;                  // Current number of peers
  } PeerList;

  PeerList * peerList = (PeerList *)malloc(sizeof(PeerList));
  peerList->count = 0; // Initialize the count to 0
  for (int i = 0; i < MAX_CLIENTS; i++) {
    peerList->peers[i] = NULL; // Initialize all pointers to NULL
  }

  printf("Started a server...\n");

  ENetEvent event;
  std::vector<ENetPeer*> peerBuffer; // Keep track of recieved peers each tick
  std::vector<ENetPacket*> packetBuffer; // Buffer to store candidate packets
  int recievedPackets = 0;
  int sentPackets = 0;

  auto startTime = std::chrono::system_clock::now();
  double prevCountSeconds = std::chrono::duration<double>(startTime.time_since_epoch()).count();
  double prevTickSeconds = std::chrono::duration<double>(startTime.time_since_epoch()).count();
  const double tickInterval = 1.0 / 30.0; // Hz tickrate

  // Wait up to 10000 milliseconds for an event then shutdown
  while(enet_host_service(server, &event, 10000) > 0) {
    switch(event.type) {
      case ENET_EVENT_TYPE_CONNECT: {
        printf("A new client connected from %x:%u.\n",  event.peer->address.host, event.peer->address.port);
        /* Store any relevant client information here. */
        event.peer->data = (void*)"CLIENTidentifier";
        if (peerList->count < MAX_CLIENTS) {
          peerList->peers[peerList->count++] = event.peer; // Add the peer and increment the count
          printf("%d peers\n", peerList->count);
        }
        //else delete?
      } break;

      // Recieve
      case ENET_EVENT_TYPE_RECEIVE: {
        recievedPackets++;
        /*
        printf("A packet of length %lu containing %s was received from "
                "%s on channel %u.\n",
                event.packet->dataLength, event.packet->data,
                event.peer->data, event.channelID);
        */

        // Create a new packet to ensure safe sending
        ENetPacket *packet = enet_packet_create(event.packet->data, event.packet->dataLength, event.packet->flags);
        if (packet == NULL) {
          printf("Failed to create packet for broadcasting.\n");
          enet_packet_destroy(event.packet); // Clean up the original packet
          break;
        }

        // send packet to all except sender
        // enet_host_broadcast (server, 0, event.packet);
        /*for (int i = 0; i < peerList->count; i++) {
          if (peerList->peers[i] != event.peer)
            enet_peer_send(peerList->peers[i], 0, packet);
        }*/

        // Check if this packet is newer
        bool first = true;
        for(int x=0;x<peerBuffer.size();x++){
          if(peerBuffer[x]==event.peer){
            first = false;
            enet_packet_destroy(packetBuffer[x]);
            packetBuffer[x]=packet;
            break;
          }
        }
        if(first){
          packetBuffer.push_back(packet);
          peerBuffer.push_back(event.peer);
        }

        // Clean up the packet now that we're done using it
        enet_packet_destroy(event.packet);
      } break;

      //remove packet buffer/peer buffer as well, but done in next loop anyways
      case ENET_EVENT_TYPE_DISCONNECT: {
        printf("A client disconnected from %x:%u.\n",  event.peer->address.host, event.peer->address.port);
        //printf("%s disconnected.\n", event.peer->data);
        //delete disconnected peer
        int x = 0;
        for (int i = 0; i < peerList->count; i++) {
          if (peerList->peers[i] == event.peer) {
            // delete?
            peerList->peers[i]->data = NULL;
            peerList->peers[i] = NULL;
            x = i;
            break;
          }
        }
        //move list down
        for (int i = x; i < peerList->count-1; i++) {
          peerList->peers[i] = peerList->peers[i + 1];
        }
        peerList->count--;

        // Reset the peer's client information
        event.peer->data = NULL;

        printf("%d peers\n", peerList->count);
      } break;

      case ENET_EVENT_TYPE_NONE:
        break;
    }

    // Show packets count
    auto currentTime = std::chrono::system_clock::now();
    double currentSeconds = std::chrono::duration<double>(currentTime.time_since_epoch()).count();
    if(currentSeconds-prevCountSeconds >= 1.0){
      printf("%d packets recieved since last.\n", recievedPackets);
      printf("%d packets sent since last.\n", sentPackets);
      recievedPackets = 0;
      sentPackets = 0;
      prevCountSeconds = currentSeconds;
    }
    
    // Send/Process packets at the tickrate (depends on while loop?), does not check packet
    if((currentSeconds-prevTickSeconds) >= tickInterval) {
      // Check if nothing to be sent
      if(peerList->count==1 && peerBuffer.size()==1)
        enet_packet_destroy(packetBuffer[0]);
      else{
        // Broadcast all buffered packets to peers (except the sender)
        for (int x=0;x<packetBuffer.size();x++) {
          for (int y = 0; y < peerList->count; y++) {
            //ensure not sender
            if(peerList->peers[y] != peerBuffer[x]){
              enet_peer_send(peerList->peers[y], 0, packetBuffer[x]);
              sentPackets++;
            }
          }
        }
      }

      peerBuffer.clear();
      packetBuffer.clear();

      //printf("Sending Packets.\n");
      // One could just use enet_host_service() instead
      enet_host_flush(server); // Flush all sent packets
      // Enet handles destruction of packet
      
      prevTickSeconds = currentSeconds;
    }
  }

  printf("Shutting down.\n");

  // Delete Peer Array
  free(peerList);
  
  enet_host_destroy(server);
  enet_deinitialize();

  return 0;
}