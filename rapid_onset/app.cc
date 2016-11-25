/* ####################################################################
   CMPT 464
   Ad Hoc Network Deployment
   Authors: Aidan Bush, Elliott Sobek, Christopher Dubeau,
   John Mulvany-Robbins, Kevin Saforo
   Thursday, November 10

   File: app.cc
   Description: This file contains the menu, calls to other functions
   for the deployment of an ad hoc network, and the streaming of data
   through nodes.
   ####################################################################
*/


#include "sysio.h"
#include "serf.h"
#include "ser.h"
#include "plug_null.h"
#include "tcvphys.h"
#include "phys_cc1100.h"

#include "network.h"
#include "node_tools.h"

char message[30];
extern int my_id, sfd;
int receiver = 0;
word current_state;

int ping_delay = 2;//2 Seconds default


//Global that indicates if the node is the sink or not
int sink = 0; 

fsm root {

	char selection = ' ';

	initial state INIT:
		runfsm receive;
		sink = 1;
		proceed DISPLAY;
		

	state DISPLAY:
		ser_outf(DISPLAY, "Rapid Onset; Node id (%d)\r\n"
			"(C)hange Ping Rate\r\n"
			"(P)acket Deployment\r\n"
			"(R)SSI Deployment\r\n"
			"(S)ink Status\r\n"
			"Selection: ",
			my_id);
	        proceed SELECTION;
	state SELECTION:
		switch (selection) {
		case 'C':
			ser_outf(SELECTION, "The current ping rate is: %d\r\n", ping_delay);
			proceed PROMPT;
			break;
		case 'P':
			selection = 'P';
			proceed PROMPT;
			break;
		case 'R':
			selection = 'R';
			proceed PROMPT;
 			break;
		case 'S':
			selection = 'S';
			proceed PROMPT;
			break;
		default:
			ser_inf(SELECTION, "%c", &selection);
			proceed PROMPT;
			break;
		}
		proceed PROMPT;
		release;
	state PROMPT:
		switch (selection) {
		case 'C':
			ser_inf(PROMPT, "%d", &ping_delay);
			ser_outf(PROMPT, "New ping: %d\r\n", ping_delay);
			selection = ' ';
			proceed DISPLAY;
			break;
		case 'P':
			ser_out(PROMPT, "\r\nBeginning Packet Deployment... \r\n");
			//TODO: Add Packet Deployment functions
			// deploy_packet();
			selection = ' ';
			proceed DISPLAY;
			break;
		case 'R':
			diag("\r\nBeginning RSSI Deployment... \r\n");
			//TODO: Add RSSI Deployment functions
			runfsm send_deploy;
			selection = ' ';
			proceed DISPLAY;
			break;
		case 'S':
			ser_out(PROMPT, "\r\nChecking Sink Status... \r\n");
			//TODO: Do we need this?
			selection = ' ';
			proceed DISPLAY;
			break;
		default:
			selection = ' ';
			proceed DISPLAY;
			break;
		}
}
