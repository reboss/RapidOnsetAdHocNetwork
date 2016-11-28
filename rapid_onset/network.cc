/* ####################################################################
   CMPT 464
   Ad Hoc Network Deployment
   Authors: Aidan Bush, Elliott Sobek, Christopher Dubeau,
   John Mulvany-Robbins, Kevin Saforo
   Thursday, November 10

   File: network.cc
   Description: This file contains all the logic for sending packets
   and how to handle recieved packets.
   ####################################################################
*/

#include "sysio.h"
#include "serf.h"
#include "ser.h"
#include "plug_null.h"
#include "tcvphys.h"
#include "phys_cc1100.h"

#include "node_tools.h"
#include "packet_test.h"
#include "rssi_test.h"
#include "network_help.h"
#include "network.h"
#include "node_led.h"

#define MAX_P      56

//TODO: FIX LENGTHS
#define PING_LEN   8
#define STOP_LEN   8
#define ACK_LEN    8
#define DEPLOY_LEN 12
#define DEPLOYED_LEN 17
#define MAX_RETRY  10

#define PING       1
#define DEPLOY     2
#define COMMAND    3
#define STREAM     4
#define ACK        5
#define DEPLOYED   6
#define STOP       7

#define LED_YELLOW 0
#define LED_GREEN  1
#define LED_RED    2
#define LED_RED_S  3

#define SINK_ID     0 //move to .h so can be used else where

volatile int sfd, retries = 0;
volatile int seq = 0;
volatile bool acknowledged, pong;
// get id's from node_tools
extern int my_id, parent_id, child_id, dest_id;
extern cur_state;
extern int ping_delay, test;
extern int max_nodes;

char payload[MAX_P];
//Variable that tells the node if it can keep sending deploys
int cont = 1;
bool deployed = FALSE;

bool is_lost_con_retries(void) {
    return retries == MAX_RETRY;
}

bool is_lost_con_ping(int ping_retries) {
    return ping_retries == MAX_RETRY;
}

/*
   sends the same packet continuously until an ack is received.
   After 10 retries, lost connection is assumed.
*/


//sends the test messages to sink
fsm final_deploy {
    address packet;
    
    initial state INIT:
        packet = tcv_wnp(INIT, sfd, 8 + 20);
	    build_packet(packet, my_id, SINK_ID, STREAM, seq,
                     "TEAM FLABBERGASTED\0");
        tcv_endp(packet);
        finish;
}

fsm send_stop(int dest) {//refactor this is ugly

    initial state SEND:
        diag("Entered send_stop FSM\r\n");
	    if (acknowledged) {
            if (my_id < max_nodes - 1)//if not last node
		        runfsm send_deploy(test);
            else
                runfsm final_deploy;
            deployed = TRUE;
		    set_led(LED_GREEN);
            finish;
	    }
        //if (is_lost_con_retries())
		  //set_led(LED_RED_S);
        address packet = tcv_wnp(SEND, sfd, STOP_LEN);
        build_packet(packet, my_id, dest, STOP, seq, payload);//payload wrong?
        tcv_endp(packet);
        //retries++;
        delay(2000, SEND);//should use define not magic number
        release;
}

	//TODO: SEE DEPLOYED case in receive fsm
fsm send_deploy {

	//address packet;
    byte pl[3];
	 
    initial state SEND_DEPLOY_INIT:
	    pl[0] = test;
        pl[1] = max_nodes;
        pl[2] = '\0';
	    proceed SEND_DEPLOY_ACTIVE;
        

    //keep sending deploys
    state SEND_DEPLOY_ACTIVE:
        if (cont) {
	    address packet;
	    packet = tcv_wnp(SEND_DEPLOY_ACTIVE, sfd, DEPLOY_LEN);
	    build_packet(packet, my_id, my_id + 1, DEPLOY, seq, pl);
		diag("\r\nFunction Test:\r\nDest_ID: %x\r\nSource_ID: %x\r\n"
			 "Hop_ID: %x\r\nOpCode: %x\r\nEnd: %x\r\nLength: %x\r\n"
			 "SeqNum: %x\r\nPayload: %x\r\nMaxnodes: %x\r\nRSSI: %x\r\n",
             get_destination(packet), get_source_id(packet), get_hop_id(packet),
             get_opcode(packet), get_end(packet), get_length(packet),
             get_seqnum(packet), *get_payload(packet), get_payload(packet)[1],
             get_rssi(packet));
	    //diag("packet built\r\n");
		tcv_endp(packet);
			
		//temporary increment
		seq = (seq + 1) % 256;
		//diag("packet sent\r\n");
        delay(1000, SEND_DEPLOY_ACTIVE);
        release;
        } else {
		  //runfsm send_stop(my_id - 1);
          finish;
        }
}

fsm send_ack(int dest) {

  // ack sequence will match packet it is responding to
  int ack_sequence = 0;

  initial state SEND:
    address packet = tcv_wnp(SEND, sfd, ACK_LEN);
    build_packet(packet, my_id, dest, ACK, ack_sequence, NULL);
    tcv_endp(packet);
    finish;
}



fsm stream_data {
  
  initial state SEND:
        if (acknowledged)
            finish;
        if (is_lost_con_retries())
	    set_led(LED_RED_S);
        address packet;
        sint plen = strlen(payload);
        packet = tcv_wnp(SEND, sfd, plen);
        //should be forwarding not rebuilding
        build_packet(packet, my_id, dest_id, STREAM, seq, payload);
        tcv_endp(packet);
        retries++;
        seq++;
}

fsm send_pong {
  
  initial state SEND:
        address packet;
        packet = tcv_wnp(SEND, sfd, PING_LEN);
        build_packet(packet, my_id, dest_id, PING, seq, NULL);
        finish;
}

fsm send_ping {

  int ping_sequence = 0;
    int ping_retries = 0;

    initial state SEND:
        if (pong) {
            ping_sequence++;
            ping_retries = 0;
        }
        else
            ping_retries++;
        if (is_lost_con_ping(ping_retries))
	    set_led(LED_RED_S);

        pong = NO;
        address packet;
        packet = tcv_wnp(SEND, sfd, PING_LEN);
        build_packet(packet, my_id, dest_id, PING, ping_sequence, NULL);        delay(ping_delay, SEND);
        release;
}

fsm receive {
	address packet;
	sint plength;

	initial state INIT_CC1100:
		proceed RECV;
	
	state RECV:
	  diag("F");
		packet = tcv_rnp(RECV, sfd);
	    plength = tcv_left(packet);
		proceed EVALUATE;

	state EVALUATE:
		switch (get_opcode(packet)) {
		case PING:
		  if (get_hop_id(packet) < my_id) {
			        seq = 0;
				runfsm send_pong;
		  } else {
				pong = YES;
		  }
			break;
		case DEPLOY://turn into funciton to long/complicated
            if (deployed)
                break;
			set_ids(packet);
			set_led(LED_YELLOW);
			cur_state = 0;
            max_nodes = get_payload(packet)[1];//set max nodes
			switch(get_payload(packet)[0]) {
			case RSSI_TEST:
                diag("RSSI: %x\r\n", get_rssi(packet));
			  test = RSSI_TEST;
			  if (rssi_setup_test(packet)) {
                    set_ids(packet);//set ids
			        seq = 0;
					runfsm send_stop(my_id - 1);
			  }
			break;

			case PACKET_TEST:
                diag("P TEST SEQ: %x\r\n", get_seqnum(packet));
			  test = PACKET_TEST;
			  if (packet_setup_test(packet) == 1) {
                    set_ids(packet);//set id
			        seq = 0;
					runfsm send_stop(my_id - 1);
			  }
			break;

			default:
			  set_led(LED_RED_S);
			  diag("Unrecognized deployment type");
			  break;
			}
			break;
			  
			/* The DEPLOYED opcode is intended for the sink, nodes need to pass
			   it on and the sink has to keep track of when every node is
			   deployed, so it can begin streaming */
		case DEPLOYED:
		        //runfsm send_deployed;
			break;
		case STREAM:
		  diag("G");
		    diag("\r\nHOP PACKET!!!!!\r\n%s\r\n", get_payload(packet));
			// check sequence number for lost ack
			// check if packet has reached it's destination
			acknowledged = NO;
			//runfsm stream_data;
            address hop_packet;
			diag("A");
            hop_packet = tcv_wnp(EVALUATE, sfd,
                                 strlen(get_payload(packet)) + 1 + 8);
            build_packet(hop_packet, get_source_id(packet),
                         get_destination(packet), get_opcode(packet), seq++,
                         get_payload(packet));
			diag("B");
            tcv_endp(hop_packet);
			diag("C");
        //packet = tcv_wnp(INIT, sfd, 8 + 20);
	    //build_packet(packet, my_id, SINK_ID, STREAM, seq,
                     //"TEAM FLABBERGASTED\0");
			runfsm send_ack(get_hop_id(packet));
			diag("D");
			break;
		case ACK://deal w/ type
			acknowledged = YES;
			retries = 0;
			break;
		case COMMAND:
			break;
		case STOP:
		  if (get_destination(packet) == my_id) {
			runfsm send_ack(get_source_id(packet));
			cont = 0;
			diag("\r\nRECEIVED STOP...\r\n");
		  }
			break;
		default:
			break;
		}
		tcv_endp(packet);
		diag("E");
		proceed RECV;
}
