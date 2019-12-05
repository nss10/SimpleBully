#include <stdio.h>
#include "simplebully.h"

int MAX_ROUNDS = 1;				   // number of rounds to run the algorithm
double TX_PROB = 1.0 - ERROR_PROB; // probability of transmitting a packet successfully

unsigned long int get_PRNG_seed()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	unsigned long time_in_micros = 1000000 * tv.tv_sec + tv.tv_usec + getpid(); //find the microseconds for seeding srand()

	return time_in_micros;
}

bool is_timeout(time_t start_time)
{
	return time(NULL) - start_time > TIME_OUT_INTERVAL;
}

bool will_transmit()
{
	// YOUR CODE GOES HERE
}

bool try_leader_elect()
{
	// first toss a coin: if prob > 0.5 then attempt to elect self as leader
	// Otherwise, just keep listening for any message
	double prob = rand() / (double)RAND_MAX; // number between [0.0, 1.0]
	bool leader_elect = (prob > THRESHOLD);

	return leader_elect;
}

int main(int argc, char *argv[])
{

	int myrank, np;
	int current_leader = 0; // default initial leader node

	// user input argv[1]: designated initial leader
	current_leader = atoi(argv[1]);

	// user input argv[2]: how many rounds to run the algorithm
	MAX_ROUNDS = atoi(argv[2]);

	// user input argv[3]: packet trasnmission success/failure probability
	TX_PROB = atof(argv[3]);


	printf("\n*******************************************************************");
	printf("\n*******************************************************************");
	printf("\n Initialization parameters:: \n\tMAX_ROUNDS = %d \n\tinitial leader = %d \n\tTX_PROB = %f\n", MAX_ROUNDS, current_leader, TX_PROB);
	printf("\n*******************************************************************");
	printf("\n*******************************************************************\n\n");

	// YOUR CODE FOR MPI Initiliazation GOES HERE
	MPI_Init(&argc, &argv);
	MPI_Comm_size(comm, &np);
	MPI_Comm_rank(comm, &myrank);

	srand(get_PRNG_seed()); // HINT: COMMENT THIS LINE UNTIL YOU ARE SURE THAT YOUR CODE IS CORRECT. THIS WILL AID IN THE DEBUGGING PROCESS

	int succ, pred; // succ = successor on ring; pred = predecessor on ring
	int mytoken;

	// YOUR CODE FOR SETTING UP succ and pred GOES HERE
	succ = (myrank + 1) % np;
	pred = myrank == 0 ? np - 1 : (myrank - 1);
	for (int round = 0; round < MAX_ROUNDS; round++)
	{
		printf("\n[process %d]****************** ROUND %d ***************************[Current Leader %d]\n", myrank,round,current_leader);
//		printf("I'm process %d and my succ and pred are %d and %d resp\n",myrank,succ,pred); fflush(stdout);

		if (myrank == current_leader)
		{
			printf("\n\tLeader executing by process %d\n",myrank);fflush(stdout);
			if (try_leader_elect())
			{
				// then send a leader election message to next node on ring, after
				// generating a random token number. Largest token among all nodes will win.
				mytoken =  rand() % MAX_TOKEN_VALUE;
				int send_buf[2] = {myrank, mytoken};
				printf("\n\t%d and %d are the msg values sent by current leader\n",send_buf[0],send_buf[1]);fflush(stdout);
				MPI_Send(send_buf, 2, MPI_INT, succ, LEADER_ELECTION_MSG_TAG, comm);
				printf("\n\t[rank %d][%d] SENT LEADER ELECTION MSG to node %d with TOKEN = %d, tag = %d\n", myrank, round, succ, mytoken, LEADER_ELECTION_MSG_TAG);
				fflush(stdout);
			}
			else
			{
				// Otherwise, send a periodic HELLO message around the ring
				MPI_Send("Hello", 6, MPI_BYTE, succ, HELLO_MSG_TAG, comm);
				printf("\n\t[rank %d][%d] SENT HELLO MSG to node %d with TOKEN = %d, tag = %d\n", myrank, round, succ, mytoken, HELLO_MSG_TAG);
				fflush(stdout);
			}

			// Now FIRST issue a speculative MPI_IRecv() to receive data back
			int recv_buf[2];
			int flag = 0; // Will keep track of whether a message was received before time out
			// YOUR CODE GOES HERE
			MPI_Request request;
			MPI_Status status;
			time_t start_time = time(NULL);
			MPI_Irecv(recv_buf,2,MPI_INT,pred,MPI_ANY_TAG,comm,&request);

			do
			{
				MPI_Test(&request, &flag, &status);
				//printf("Leader waiting for response\n");

			} while (!flag && !is_timeout(start_time));

				// Next, you need to check if time out has occured. If time out, then you need to cancel the earlier issued speculative MPI_Irecv.
			if (is_timeout(start_time))
			{
				printf("\n\t[rank %d][%d] CANCELLING RECEIVE DUE TO TIMEOUT\n",myrank,round);
				MPI_Cancel(&request);
				MPI_Request_free(&request);
			}
			// We receive the message from predecessor node and decide appropriate action based on the message TAG
			if (flag)
			{
				// If HELLO MSG received, do nothing
				// If LEADER ELECTION message, then determine who is the new leader and send out a new leader notification message
				printf("\n\tLeader received a response from %d with tag %d\n",status.MPI_SOURCE,status.MPI_TAG);
				switch (status.MPI_TAG)
				{
				case HELLO_MSG_TAG:
					printf("\n\t[rank %d][%d] HELLO MESSAGE completed ring traversal!\n", myrank, round);
					fflush(stdout);
					break;
				case LEADER_ELECTION_MSG_TAG:
					// Send a new leader message

					current_leader = recv_buf[0];
					MPI_Send(recv_buf, 2, MPI_INT, succ, LEADER_ELECTION_RESULT_MSG_TAG, comm);
					printf("\n\t[rank %d][%d] NEW LEADER FOUND! new leader = %d, with token = %d\n", myrank, round, current_leader, recv_buf[1]);


					printf("\n\tLeader Waiting for the Election result to complete it's ring\n");fflush(stdout);
					MPI_Recv(recv_buf,2,MPI_INT,pred,LEADER_ELECTION_RESULT_MSG_TAG,comm,&status);
					printf("\n\tElection result ring completed\n");fflush(stdout);

					break;
				default:
					printf("\n\tLeader entered default case in the switch case response::IMPROPER BEHAVIOR::\n");; // do nothing
				}
			}
		}
		else
		{
			// Wait for a message to arrive until time out occurs
			int flag = 0; // Will keep track of whether a message was received before time out
			MPI_Request request;
			MPI_Status status;
			time_t start_time = time(NULL);
			int msg[2];
			MPI_Irecv(msg,2,MPI_INT,pred,MPI_ANY_TAG,comm,&request);
			do
			{
				MPI_Test(&request, &flag, &status);

			} while (!flag && !is_timeout(start_time));

			if(is_timeout(start_time)){
				printf("\n\t[rank %d][%d] CANCELLING RECEIVE DUE TO TIMEOUT\n",myrank,round);

				MPI_Cancel(&request);
				MPI_Request_free(&request);
			}
			if (flag)
			{
				// You want to first receive the message so as to remove it from the MPI Buffer
				// Then determine action depending on the message Tag field

				if (status.MPI_TAG == HELLO_MSG_TAG)
				{
					// Forward the message to next node
					if(get_prob() > TX_PROB){
						MPI_Send("Hello", 6, MPI_BYTE, succ, HELLO_MSG_TAG, comm);
						printf("\n\t[rank %d][%d] Received and Forwarded HELLO MSG to next node = %d\n", myrank, round, succ);
						fflush(stdout);
					}
					else{
						printf("\n\t[rank %d][%d] Decided to drop the Hello message sent from %d\n",myrank,round,pred);fflush(stdout);
					}
				}
				else if (status.MPI_TAG == LEADER_ELECTION_MSG_TAG)
				{
					// Fist probabilistically see if wants to become a leader.
					// If yes, then generate own token and test if can become leader.
					// If can become leader, then update the LEADER ELECTION Message appropriately and retransmit to next node
					// Otherwise, just forward the original received LEADER ELECTION Message
					// With a probability 'p', forward the message to next node
					// This simulates link or node failure in a distributed system
					if (try_leader_elect())
					{
						mytoken =  rand() % MAX_TOKEN_VALUE;
						printf("\n\t[rank %d][%d] My new TOKEN = %d\n", myrank, round, mytoken);

						if ((mytoken >= msg[1]) && (myrank > msg[0]))
						{
							msg[0] = myrank;
							msg[1] = mytoken;
							printf("\n\t[rank %d][%d] Updating the election values with my token and rank\n",myrank,round);

						}else{
							printf("\n\t[rank %d][%d]Leaving message and token unchanged\n", myrank, round, mytoken);
						}fflush(stdout);

					}
					else
					{
						printf("\n\t[rank %d][%d] Will not participate in Leader Election.\n", myrank, round);
						fflush(stdout);
					}

					// Forward the LEADER ELECTION Message
					MPI_Send(msg, 2, MPI_INT, succ, LEADER_ELECTION_MSG_TAG, comm);
					printf("\n\t[rank %d][%d] Forwarded Leader Election message to node %d\n",myrank,round,succ);fflush(stdout);

					// Finally, wait to hear from current leader who will be the next leader
					MPI_Recv(msg,2,MPI_INT,pred,LEADER_ELECTION_RESULT_MSG_TAG,comm,&status);
					current_leader = msg[0];
					printf("\n\t[rank %d][%d] NEW LEADER :: node %d with TOKEN = %d\n", myrank, round, current_leader, msg[1]);fflush(stdout);

					// Forward the LEADER ELECTION RESULT MESSAGE
					MPI_Send(msg, 2, MPI_INT, succ, LEADER_ELECTION_RESULT_MSG_TAG, comm);

				}
			}
		}
		// Finally hit barrier for synchronization of all nodes before starting new round of message sending
		printf("\n\t[rank %d][%d] Waiting for other nodes at the barrier\n",myrank,round);
		MPI_Barrier(comm);
	}
	printf("\n** Leader for NODE %d = %d\n", myrank, current_leader);

	// Finalize MPI for clean exit
	MPI_Finalize();
	return 0;
}
