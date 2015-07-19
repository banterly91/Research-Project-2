/*
 * ipop-tap
 * Copyright 2013, University of Florida
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
 
//libraries and definitions required for this implementation
#define THREAD_BUFFER 1000
#include <sched.h>
#define BILLION 1e9


#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
//end libraries and definitions required for this implementation
#if defined(LINUX) || defined(ANDROID)
#include <sys/socket.h>
#include <net/if.h>
#include <arpa/inet.h>
#elif defined(WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdint.h>
#include <win32_tap.h>
#endif

#include "peerlist.h"
#include "headers.h"
#include "translator.h"
#include "tap.h"
#include "ipop_tap.h"
#include "packetio.h"

//structure definition that will be used as argument when creating parallel threads
typedef struct parallel_arg {
    thread_opts_t *opts;
	volatile unsigned int produceCount, consumeCount;
	volatile unsigned char ipop_buf[THREAD_BUFFER][BUFLEN];
	volatile int rcount[THREAD_BUFFER];
} parallel_arg_t;

//producer function for the sender 
void *producer(void *argumentsp) {
	parallel_arg_t *args=(parallel_arg_t *) argumentsp;
	volatile unsigned int IDp = 0;
	int tap=args->opts->tap;
	//for timing the code
	//struct timespec senderstart, senderstop;
    //double senderaccum;

	while (1) {
		 // clock_gettime(CLOCK_REALTIME, &senderstart); 
		while (args->produceCount - args->consumeCount == THREAD_BUFFER);
			//printf("sendbufffull\n");
			//sched_yield(); // buffer is full
                
		  
		if ((args->rcount[IDp] = read(tap, args->ipop_buf[IDp]+BUF_OFFSET, BUFLEN-BUF_OFFSET)) < 0) {
			fprintf(stderr, "tap read failed\n");
			break;
		}	
        // a memory_barrier could be put here so we dont have to use volatile
		IDp = (IDp + 1) % THREAD_BUFFER;
		++(args->produceCount);
		 //clock_gettime(CLOCK_REALTIME, &senderstop);
	    //senderaccum= (senderstop.tv_sec - senderstart.tv_sec ) + 
	//(( senderstop.tv_nsec - senderstart.tv_nsec)/BILLION);
		//printf("producer 1 readtap %lf\n", senderaccum);

		}
	}

//Consumer function for the sender 
void *consumer(void *argumentsc){
			int arp=0;
			parallel_arg_t *args=(parallel_arg_t *) argumentsc;
			int tap=args->opts->tap;
			int sock4 =args->opts->sock4;
			int sock6 =args->opts->sock6;
			int ncount;
			thread_opts_t *opts = args->opts;
			struct in_addr local_ipv4_addr;
			struct in6_addr local_ipv6_addr;
			struct peer_state *peer = NULL;
			int result, is_ipv4;
			volatile unsigned int IDc = 0;
		    //struct timespec senderstart, senderstop;
         	//  double senderaccum;

			while(1){
  			//	clock_gettime(CLOCK_REALTIME, &senderstart); 
      
				while (args->produceCount - args->consumeCount == 0);
					//printf("sendbuffempty\n");
				//	sched_yield(); // buffer is empty
  	                
		ncount = args->rcount[IDc] + BUF_OFFSET;

        /*---------------------------------------------------------------------
        Switchmode
        ---------------------------------------------------------------------*/
        if (opts->switchmode) {
            /* If the frame is broadcast message, it sends the frame to
               every TinCan links as physical switch does */
            if (is_broadcast(args->ipop_buf[IDc]+BUF_OFFSET)) {
                reset_id_table();
                while( !is_id_table_end() ) {
                    if ( is_id_exist() )  {
                        /* TODO It may be better to retrieve the iterator rather
                           than key string itself.  */
                        peer = retrieve_peer();
                        set_headers(args->ipop_buf[IDc], peerlist_local.id, peer->id);
                        if (opts->send_func != NULL) {
                            if (opts->send_func((const char*)args->ipop_buf[IDc], ncount) < 0) {
                                fprintf(stderr, "send_func failed\n");
                            }
                        }
                    }
                    increase_id_table_itr();
                }
				IDc = (IDc + 1) % THREAD_BUFFER;
				++(args->consumeCount);
                continue;
            }

            /* If the MAC address is in the table, we forward the frame to
               destined TinCan link */
            peerlist_get_by_mac_addr(args->ipop_buf[IDc]+BUF_OFFSET, &peer);
            set_headers(args->ipop_buf[IDc], peerlist_local.id, peer->id);
            if (opts->send_func != NULL) {
                if (opts->send_func((const char*)args->ipop_buf[IDc], ncount) < 0) {
                    fprintf(stderr, "send_func failed\n");
                }
            }
			IDc = (IDc + 1) % THREAD_BUFFER;
			++(args->consumeCount);
	 //   clock_gettime(CLOCK_REALTIME, &senderstop);
	 //   senderaccum= (senderstop.tv_sec - senderstart.tv_sec ) + 
	//(( senderstop.tv_nsec - senderstart.tv_nsec)/BILLION);
		//printf("consumer 1 process readtap %lf\n", senderaccum);

            continue;
        }

        /*---------------------------------------------------------------------
        Conventional IPOP Tap (non-switchmode)
        ---------------------------------------------------------------------*/

        // checks to see if this is an ARP request, if so, send response
        if ((args->ipop_buf[IDc]+BUF_OFFSET)[12] == 0x08 && (args->ipop_buf[IDc]+BUF_OFFSET)[13] == 0x06 && (args->ipop_buf[IDc]+BUF_OFFSET)[21] == 0x01
            && !opts->switchmode) {
            if (create_arp_response(args->ipop_buf[IDc]+BUF_OFFSET) == 0) {
#if defined(LINUX) || defined(ANDROID)
                int r = write(tap,args->ipop_buf[IDc]+BUF_OFFSET, args->rcount[IDc]);
#elif defined(WIN32)
                int r = write_tap(win32_tap, (char *)args->ipop_buf[IDc]+BUF_OFFSET, args->rcount[IDc]);
#endif
                // This doesn't handle partial writes yet, we need a loop to
                // guarantee a full write.
                if (r < 0) {
                    fprintf(stderr, "tap write failed\n");
					pthread_exit(NULL);
                    break;
                }
            }
			IDc = (IDc + 1) % THREAD_BUFFER;
		    ++(args->consumeCount);
            continue;
        }


        if (((args->ipop_buf[IDc]+BUF_OFFSET)[14] >> 4) == 0x04) { // ipv4 packet
            memcpy(&local_ipv4_addr.s_addr, args->ipop_buf[IDc]+BUF_OFFSET + 30, 4);
            is_ipv4 = 1;
        } else if (((args->ipop_buf[IDc]+BUF_OFFSET)[14] >> 4) == 0x06) { // ipv6 packet
            memcpy(&local_ipv6_addr.s6_addr, args->ipop_buf[IDc]+BUF_OFFSET + 38, 16);
            is_ipv4 = 0;
        } else if ((args->ipop_buf[IDc]+BUF_OFFSET)[12] == 0x08 && (args->ipop_buf[IDc]+BUF_OFFSET)[13] == 0x06 && opts->switchmode) {
            arp = 1;
            is_ipv4 = 0;
        } else {
            fprintf(stderr, "unknown IP packet type: 0x%x\n", (args->ipop_buf[IDc]+BUF_OFFSET)[14] >> 4);
			IDc = (IDc + 1) % THREAD_BUFFER;
		    ++(args->consumeCount);
            continue;
        }

        // we need to update the size of packet to account for ipop header
        ncount = args->rcount[IDc] + BUF_OFFSET;

        // we need to initialize peerlist
        peerlist_reset_iterators();
        while (1) {
            if (arp || is_ipv4) {
                result = peerlist_get_by_local_ipv4_addr(&local_ipv4_addr,
                                                         &peer);
            } else {
                result = peerlist_get_by_local_ipv6_addr(&local_ipv6_addr,
                                                         &peer);
            }

            // -1 means something went wrong, should not happen
            if (result == -1) break;
			

            if (arp) {
                // ARP message should not be forwarded to peers but to 
                // controller only
                set_headers(args->ipop_buf[IDc], peerlist_local.id, null_peer.id);
            } else {
                // we set ipop header by copying local peer uid as first
                // 20-bytes and then dest peer uid as the next 20-bytes. That is
                // necessary for routing by upper layers
                set_headers(args->ipop_buf[IDc], peerlist_local.id, peer->id);
            }

            // we only translate if we have IPv4 packet and translate is on
            if (!arp && is_ipv4 && opts->translate) {
                translate_packet(args->ipop_buf[IDc]+BUF_OFFSET, NULL, NULL,args->rcount[IDc]);
            }

            // If the send_func function pointer is set then we use that to
            // send packet to upper layers, in IPOP-Tincan this function just
            // adds to a send blocking queue. If this is not set, then we
            // send to the IP/port stored in the peerlist when the node was
            // added to the network
            if (opts->send_func != NULL) {
                if (opts->send_func((const char*)args->ipop_buf[IDc], ncount) < 0) {
                    fprintf(stderr, "send_func failed\n");
                }
            }
            else {
                // this is portion of the code allows ipop-tap nodes to
                // communicate directly among themselves if necessary but
                // there is no encryption provided with this approach
                struct sockaddr_in dest_ipv4_addr_sock = {
                    .sin_family = AF_INET,
                    .sin_port = htons(peer->port),
                    .sin_addr = peer->dest_ipv4_addr,
                    .sin_zero = { 0 }
                };
                // send our processed packet off
                if (sendto(sock4, (const char *)args->ipop_buf[IDc], ncount, 0,
                           (struct sockaddr *)(&dest_ipv4_addr_sock),
                           sizeof(struct sockaddr_in)) < 0) {
                    fprintf(stderr, "sendto failed\n");
                }
            }
            if (result == 0) break;
        }
		IDc = (IDc + 1) % THREAD_BUFFER;
		++args->consumeCount;
    }
}



//the sender thread which creates the consumer and producer threads
void *
ipop_send_thread(void *data)
{
	
    // thread_opts data structure is shared by both send and recv threads
    // so do not modify its contents only read
    thread_opts_t *opts = (thread_opts_t *) data;
    int sock4 = opts->sock4;
    int sock6 = opts->sock6;
    int tap = opts->tap;
    
    parallel_arg_t *argument = malloc(sizeof *argument);
	pthread_t prod, cons;
	argument->opts=(thread_opts_t *) data;
	argument->produceCount=0;
	argument->consumeCount=0;
	pthread_create( &prod, NULL, producer, argument);
	pthread_create( &cons, NULL, consumer, argument);
    pthread_join(cons, NULL);
		
    close(sock4);
    close(sock6);
#if defined(LINUX) || defined(ANDROID)
    tap_close();
#elif defined(WIN32)
    // TODO - Add close socket for tap
    WSACleanup();
#endif
    pthread_exit(NULL);
    return NULL;
}

 
/////////////////////////////////////////////////////////////////////////////////////////////////
//Receiving Code
/////////////////////////////////////////////////////////////////////////////////////////////////

//Producer function for the receiver
 void *producer2(void *argumentsp) {

 parallel_arg_t *args=(parallel_arg_t *) argumentsp;
	volatile unsigned int IDp2 = 0;
	int tap=args->opts->tap;
	thread_opts_t *opts = args->opts;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
	char source_id[ID_SIZE] = { 0 };
    char dest_id[ID_SIZE] = { 0 };
    struct peer_state *peer = NULL;
    		// struct timespec senderstart, senderstop;
         //	  double senderaccum;
  while (1) {
	// clock_gettime(CLOCK_REALTIME, &senderstart); 
        while (args->produceCount - args->consumeCount == THREAD_BUFFER);
		
//	sched_yield(); // buffer is full
        if (opts->recv_func != NULL) {
            if ((args->rcount[IDp2] = opts->recv_func((char *)(args->ipop_buf[IDp2]), BUFLEN)) < 0) {
              fprintf(stderr, "recv_func failed\n");
              break;
            }
        }
        else if ((args->rcount[IDp2] = recvfrom(opts->sock4, (char *)args->ipop_buf[IDp2], BUFLEN, 0,
                               (struct sockaddr*) &addr, &addrlen)) < 0) {
      
            fprintf(stderr, "udp recv failed\n");
            break;
        }
        

        args->rcount[IDp2] -= BUF_OFFSET;

        get_headers(args->ipop_buf[IDp2], source_id, dest_id);
      
        if ((args->ipop_buf[IDp2])[52] == 0x08 && (args->ipop_buf[IDp2])[53] == 0x06 && 
            ((args->ipop_buf[IDp2])[61] == 0x02 || (args->ipop_buf[IDp2])[61] == 0x01)) {
             mac_add((const unsigned char *) &(args->ipop_buf[IDp2]));
        }

        
        if (((args->ipop_buf[IDp2]+BUF_OFFSET)[14] >> 4) == 0x04 && opts->translate) {
            int peer_found = peerlist_get_by_id(source_id, &peer);
          
            if (peer_found != -1) {
             
                translate_packet(args->ipop_buf[IDp2], (char *)(&peer->local_ipv4_addr.s_addr),
                               (char *)(&peerlist_local.local_ipv4_addr.s_addr),
                               args->rcount[IDp2]);
              
                translate_headers(args->ipop_buf[IDp2], (char *)(&peer->local_ipv4_addr.s_addr),
                               (char *)(&peerlist_local.local_ipv4_addr.s_addr),
                               args->rcount[IDp2]);
            }
        }

        if ( opts->switchmode == 0 ||
             (memcmp(args->ipop_buf[IDp2]+BUF_OFFSET, args->ipop_buf[IDp2]+BUF_OFFSET+6, 6) == 0 && opts->switchmode == 1)) {
            update_mac(args->ipop_buf[IDp2]+BUF_OFFSET, opts->mac);
        }
		IDp2 = (IDp2 + 1) % THREAD_BUFFER;
		++(args->produceCount);


 }
 }
 
 //Consumer function for the receiver
 
void *
ipop_recv_thread(void *data)
{
  
    thread_opts_t *opts = (thread_opts_t *) data;
    int sock4 = opts->sock4;
    int sock6 = opts->sock6;
    int tap = opts->tap;
			// struct timespec senderstart, senderstop;
         	  //double senderaccum;
	
	volatile unsigned int IDc2 = 0;
	parallel_arg_t *argument2 = malloc(sizeof *argument2);
	pthread_t prod2;
	argument2->produceCount=0;
	argument2->consumeCount=0;
    argument2->opts=(thread_opts_t *) data;
	pthread_create(&prod2,NULL, producer2, argument2);
    while (1) {
          	// clock_gettime(CLOCK_REALTIME, &senderstart); 
		while (argument2->produceCount - argument2->consumeCount == 0);
	
	//	sched_yield(); // buffer is empty
	
        if (write(tap,argument2->ipop_buf[IDc2]+BUF_OFFSET,argument2->rcount[IDc2]) < 0) {
	
            fprintf(stderr, "write to tap error\n");
            break;
        }
		IDc2 = (IDc2 + 1) % THREAD_BUFFER;
		++(argument2->consumeCount);
	 
    }

    close(sock4);
    close(sock6);
    tap_close();
    pthread_exit(NULL);
    return NULL;
}

