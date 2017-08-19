#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>

#define MAX_ROUTERS 22
// ./ospf -i id -f infile -o outfile -h hi -a lsai -s sp
void *send_hello_message(void *_);
void *receive_hello_reply_message(void *_);
void *lsa_message(void *_);
void *spf_algorithm(void *_);
void *clk_counter(void *_);
void make_links();
void dijkstras_algo();
void add_to_visited(int node);
int check_visited(int node);
void trace_path();
void print_routing_table();

pthread_t send_thread;
pthread_t receive_thread;
pthread_t lsa_thread;
pthread_t spf_thread;
pthread_t clk_thread;
pthread_mutex_t pt_lock1,pt_lock2,pt_lock3,lsa_lock1,lsa_lock2,lsa_lock3;
int port,node_id;
int sock;
int clock_counter=0;
int number_of_nodes,number_of_edges;
int hi,lsai,sp;
int links[MAX_ROUTERS][MAX_ROUTERS];
int neighbour_cost[MAX_ROUTERS];
int min_cost[MAX_ROUTERS][MAX_ROUTERS];
int max_cost[MAX_ROUTERS][MAX_ROUTERS];
int lsa_data[MAX_ROUTERS][MAX_ROUTERS][4];


// For Dijkstra's
int visited_nodes[MAX_ROUTERS];
int dist[MAX_ROUTERS];
int node_costs[MAX_ROUTERS][MAX_ROUTERS];
int previous_node[MAX_ROUTERS];
int new_links[MAX_ROUTERS][MAX_ROUTERS];
int unvisited_nodes[MAX_ROUTERS];
int path[MAX_ROUTERS][MAX_ROUTERS];
int size_of_q=0;

void *send_hello_message(void *_)
{
    int i,j;
	for(i=0;i<number_of_nodes;i++)
	{
		neighbour_cost[i]=10000;
	}
	while(1)
	{
		printf("counter clock is      %d\n",clock_counter);
		if(clock_counter%20 == 0 && clock_counter != 0)
		{
            printf("counter clock ......is      %d\n",clock_counter);
			pthread_mutex_lock(&pt_lock1);
		}
	    for(i=0;i<number_of_nodes;++i)
	    {
	        if(links[node_id-1][i] == 1)
	        {
	        	struct sockaddr_in client_addr;
	        	struct hostent *host;
	        	host = (struct hostent *) gethostbyname("localhost");
	        	client_addr.sin_family = AF_INET;
	    		client_addr.sin_port = htons(20000+i+1);
	    		client_addr.sin_addr = *((struct in_addr *) host->h_addr);
	    		bzero(&(client_addr.sin_zero), 8);
	    		sendto(sock, "HELLO", strlen("HELLO"), 0,
	                (struct sockaddr *) &client_addr, sizeof (struct sockaddr));  
	            
	        }
	    }
	    sleep(hi); 
    }
}

void *receive_hello_reply_message(void *_)
{
	while(1)
	{
		printf("Receive counter clock is      %d\n",clock_counter);
		if(clock_counter%20 == 0 && clock_counter != 0)
		{
			pthread_mutex_lock(&pt_lock2);
		}
		int from_port,from_node;
		struct sockaddr_in client_addr;
		char recv_data[1024];
		char *send_data=(char *)malloc(sizeof(char)*1024);
		int addr_len, bytes_read;
		bytes_read = recvfrom(sock, recv_data, 1024, 0,
	                (struct sockaddr *) &client_addr, &addr_len);
		recv_data[bytes_read] = '\0';
		printf("\n(%s , %d) said : ", inet_ntoa(client_addr.sin_addr),
	                ntohs(client_addr.sin_port));
	        printf("%s\n", recv_data);

	    from_port=ntohs(client_addr.sin_port);
	    from_node=from_port-20000;
	    if(strcmp(recv_data,"HELLO")==0 && from_node > 0)
	    {
	    	int cost_diff,rand_scale,rand_cost;
	    	char temp1[3],temp2[3],temp3[3];
	    	cost_diff=max_cost[node_id-1][from_node-1]-min_cost[node_id-1][from_node-1];
	    	rand_scale=rand()%cost_diff;
	    	rand_cost=min_cost[node_id-1][from_node-1]+rand_scale;
	    	sprintf(temp1, "%d",node_id);
	    	sprintf(temp2, "%d",from_node);
	    	sprintf(temp3, "%d",rand_cost);
	    	strcat(send_data,"HELLOREPLY ");
	    	strcat(send_data,temp1);
	    	strcat(send_data," ");
	    	strcat(send_data,temp2);
	    	strcat(send_data," ");
	    	strcat(send_data,temp3);
	    	sendto(sock, send_data, strlen(send_data), 0,
	                (struct sockaddr *) &client_addr, sizeof (struct sockaddr));  
	    	printf("%s\n",send_data);
	        
	    }
	    if(recv_data[5]=='R' && from_node > 0)
	    {
	    	int link_cost;
	    	char temp_recv_data[1024];
	    	char *token,*temp1,*temp2,*temp3;
	    	strcpy(temp_recv_data,recv_data);
	    	token = strtok(temp_recv_data, " ");
	    	temp1 = strtok(NULL, " ");
	    	temp2 = strtok(NULL, " ");
	    	temp3  = strtok(NULL, " ");
            link_cost=atoi(temp3);
	    	neighbour_cost[from_node-1]=link_cost;
	    }

	    if(recv_data[0]=='L' && from_node > 0)
	    {
	    	int i=0;
	    	char temp_recv_data[1024];
	    	int src_id,number_of_neigh,seq_num,nd,ct;
	    	char *token,*temp1,*temp2,*temp3,*temp4,*temp5,*temp6,*temp7;
	    	strcpy(temp_recv_data,recv_data);
	    	token = strtok(temp_recv_data, " ");
	    	temp1 = strtok(NULL, " ");
	    	temp2 = strtok(NULL, " ");
	    	temp3 = strtok(NULL, " ");
	    	src_id=atoi(temp1);
	    	seq_num=atoi(temp2);
	    	number_of_neigh=atoi(temp3);
	    	printf("............%d  %d  %d\n",src_id , seq_num , number_of_neigh);
	    	for(i=0;i<number_of_neigh;++i)
	    	{
	    		temp6 = strtok(NULL, " ");
	    		nd = atoi(temp6);
		    	temp7 = strtok(NULL, " ");
		    	ct = atoi(temp7);
	    		if(seq_num > lsa_data[src_id-1][nd-1][1])
	    		{
		    		
		    		lsa_data[src_id-1][nd-1][0]=1;
		    		lsa_data[src_id-1][nd-1][1]=seq_num;
		    		lsa_data[src_id-1][nd-1][2]=ct;
		    		lsa_data[nd-1][src_id-1][0]=1;
		    		lsa_data[nd-1][src_id-1][1]=seq_num;
		    		lsa_data[nd-1][src_id-1][2]=ct;
		    	}
	    	}
	    	for(i=0;i<number_of_nodes;++i)
	    	{
		        if(links[node_id-1][i] == 1 && i != (from_node-1))
		        {
		        	struct sockaddr_in client_addr;
		        	struct hostent *host;
		        	host = (struct hostent *) gethostbyname("localhost");
		        	client_addr.sin_family = AF_INET;
		    		client_addr.sin_port = htons(20000+i+1);
		    		client_addr.sin_addr = *((struct in_addr *) host->h_addr);
		    		bzero(&(client_addr.sin_zero), 8);
		    		sendto(sock, recv_data, strlen(recv_data), 0,
		                (struct sockaddr *) &client_addr, sizeof (struct sockaddr));  
		            
		        }
	    	}
	    

	    }
    }
}

void *lsa_message(void *_)
{
	int sequence_num=0;
	while(1)
	{
		printf("LSA counter clock is      %d\n",clock_counter);
		if(clock_counter%20 == 0 && clock_counter != 0)
		{
			pthread_mutex_lock(&pt_lock3);		
		}
		int i,j;
		int number_of_neighbours=0;
		for(i=0;i<number_of_nodes;++i)
		{
			if(neighbour_cost[i] < 10000)
				++number_of_neighbours;
		}
		for(j=0;j<number_of_nodes;++j)
		{
			if(neighbour_cost[j]<10000)
			{
				char temp1[3],temp2[3],temp3[3],temp4[3],temp5[3];
				char *lsa_data=(char *)malloc(sizeof(char)*1024);
				++sequence_num;
			    sprintf(temp1, "%d",node_id);
			    sprintf(temp2, "%d",sequence_num);
			    sprintf(temp3, "%d",number_of_neighbours);
			    strcat(lsa_data,"LSA ");
				strcat(lsa_data,temp1);
				strcat(lsa_data," ");
				strcat(lsa_data,temp2);
				strcat(lsa_data," ");
				strcat(lsa_data,temp3);
				for(i=0;i<number_of_nodes;++i)
				{
					if(neighbour_cost[i] < 10000)
					{
						sprintf(temp4, "%d",i+1);
			    		sprintf(temp5, "%d",neighbour_cost[i]);
			    		strcat(lsa_data," ");
						strcat(lsa_data,temp4);
						strcat(lsa_data," ");
						strcat(lsa_data,temp5);
					}
				}
				struct sockaddr_in client_addr;
				struct hostent *host;
				host = (struct hostent *) gethostbyname("localhost");
				client_addr.sin_family = AF_INET;
				client_addr.sin_port = htons(20000+j+1);
				client_addr.sin_addr = *((struct in_addr *) host->h_addr);
				bzero(&(client_addr.sin_zero), 8);
				sendto(sock, lsa_data, strlen(lsa_data), 0,
				       (struct sockaddr *) &client_addr, sizeof (struct sockaddr)); 
				printf("lsa message : %s\n",lsa_data);
			}
		}
		sleep(lsai);
	}

}

void *spf_algorithm(void *_)
{
	int i=0,j=0;
	while(1)
	{

		printf("I am in algo.............................\n");
		for(i=0;i<number_of_nodes;i++)
		{
			for(j=0;j<number_of_nodes;j++)
			{
				printf("-----------%d  %d  %d  %d\n",lsa_data[i][j][0]  , lsa_data[i][j][1] , lsa_data[i][j][2] , lsa_data[i][j][3]);
			}
		}

		//Dijkstra's Algorithm
        make_links();
        dijkstras_algo();
        print_routing_table();

		pthread_mutex_unlock(&pt_lock1);
		pthread_mutex_unlock(&pt_lock2);
		pthread_mutex_unlock(&pt_lock3);
		sleep(20);
	}

}

void *clk_counter(void *_)
{
	int i=0,j=0;
	while(1)
	{
		sleep(1);
		++clock_counter;

		/*if(clock_counter % 100   == 0 && clock_counter != 0)
		{
			for(i=0;i<number_of_nodes;i++)
			{
				neighbour_cost[i]=10000;
			}
			for(i=0;i<number_of_nodes;i++)
			{
				for(j=0;j<number_of_nodes;j++)
				{
					lsa_data[i][j][0]=0;
					lsa_data[i][j][1]=0;
					lsa_data[i][j][2]=0;
					lsa_data[i][j][3]=0;
				}
			}
		}*/
	}
}

void dijkstras_algo()
{
	int i,j;
	size_of_q=number_of_nodes;
	while(size_of_q != 0)
    {
    	//printf("Size of unvisited is %d\n",size_of_q);
    	int temp,temp_dist;
		int temp_node;
		int counter=0;
		for(i=0;i<number_of_nodes;++i)
		{
			//printf("Unvisited[%d] : %d\n",i,unvisited_nodes[i]);
			if(unvisited_nodes[i]>=0)
			{
				++counter;
				if(counter == 1)
				{
					temp_node=unvisited_nodes[i];
					temp=i;
				}
				if(dist[temp_node] > dist[unvisited_nodes[i]])
				{
					//printf("....Temp node : %d   i : %d   link : %d\n",temp_node,unvisited_nodes[i],dist[unvisited_nodes[i]]);
					temp_node = unvisited_nodes[i];
					temp=i;
				}
			}
		}
		//printf("Temp node is -------------- %d\n",temp_node);
		counter = 0;
		unvisited_nodes[temp]=-1;
		add_to_visited(temp_node);
		--size_of_q;
		for(i=0;i<number_of_nodes;++i)
		{
			if(dist[i] == 0)
				previous_node[i]=i;
			//printf("Temp node : %d   i : %d   link : %d\n",temp_node,i,check_visited(i));
			if(new_links[temp_node][i] == 1 && check_visited(i) == 0)
			{
				//printf("Temp node : %d   i : %d   link : %d\n",temp_node,i,node_costs[temp_node][i]);
				temp_dist=dist[temp_node] + node_costs[temp_node][i];
				//printf("Temp node : %d   i : %d   tempp dist : %d\n",temp_node,i,temp_dist);
				if(temp_dist < dist[i])
				{
					dist[i]=temp_dist;
					previous_node[i]=temp_node;
					//printf("Dist[%d]   is   %d\n",i,dist[i]);
				}
				//printf("Temp is : %d   dist[%d] : %d\n",temp_node,i,dist[i]);
			}
		}
	}

	for(i=0;i<number_of_nodes;++i)
	{

		printf("Distance to node %d is %d\n",i+1,dist[i]);
		printf("The previous node is %d \n",previous_node[i]);
	}
	trace_path();
	return;

}

void trace_path()
{
	int i,j;
	int temp;
	for(i=0;i<number_of_nodes;++i)
	{
		temp=previous_node[i];
		while(temp != -1)
		{
			for(j=(number_of_nodes-1);j>=0;--j)
			{
				if(path[i][j]==-1)
				{
					path[i][j]=temp;
					break;
				}
			}
			if(temp == previous_node[temp])
				break;
			else
				temp = previous_node[temp];
		}
	}

	for(i=0;i<number_of_nodes;++i)
	{
		printf("Path for node %d is : ",i);
		for(j=(number_of_nodes-1);j>=0;--j)
		{
			printf(" %d ",path[i][j]);
		}
		printf("\n");
	}
	return;
}

void make_links()
{
	int i,j;
	for(i=0;i<number_of_nodes;++i)
	{
		for(j=0;j<number_of_nodes;++j)
		{
			new_links[i][j]=0;
			path[i][j]=-1;
		}	
	}
	for(i=0;i<number_of_nodes;++i)
	{
		for(j=0;j<number_of_nodes;++j)
		{
			if(lsa_data[i][j][0] == 1)
			{
				new_links[i][j]=1;
				node_costs[i][j]=lsa_data[i][j][2];
				//printf("Node Cost between %d %d is   %d\n",i,j,node_costs[i][j]);
			}
		}	
	}

	for(i=0;i<number_of_nodes;++i)
	{
		previous_node[i]=-1;
		unvisited_nodes[i]=i;
		visited_nodes[i]=-1;
		dist[i]=10000;
		if(i==(node_id-1))
		{
			dist[i]=0;
			//printf("Distance to node %d is %d\n",i+1,dist[i]);
		}
	}

	
}

void print_routing_table()
{
	int i,j;
	char *temp=(char *)malloc(sizeof(char)*3);char *temp1=(char *)malloc(sizeof(char)*3);
	char *head=(char *)malloc(sizeof(char)*22);
	char *output_file=(char *)malloc(sizeof(char)*22);
	sprintf(temp, "%d",node_id);
	strcpy(output_file,"output_");
	sprintf(temp, "%d",node_id);
	strcat(output_file,temp);
	FILE *fp;
	fp=fopen(output_file,"w");
	strcat(head,"Routing table for router ");
	strcat(head,temp);
	strcat(head," at time ");
	sprintf(temp, "%d",clock_counter);
	strcat(head,temp);
	//strcat(head,"\n Destination | Cost | Path\n");
	fprintf(fp,"%s",head);
	fprintf(fp,"%s","\n");
	strcpy(head," Destination | Cost | Path ");
	fprintf(fp,"%s",head);
	fprintf(fp,"%s","\n");
	for(i=0;i<number_of_nodes;++i)
	{
		sprintf(temp, "%d",i+1);
		fprintf(fp,"%s","       ");
		fprintf(fp,"%s",temp);
		fprintf(fp,"%s","     |   ");
		if(dist[i] < 10000)
			sprintf(temp, "%d",dist[i]);
		else
			strcpy(temp,"_");
		fprintf(fp,"%s",temp);
		fprintf(fp,"%s","  |  ");
		int t=0;
		for(j=(number_of_nodes-1);j>=0;--j)
		{
			if(path[i][j] != -1)
			{
				++t;
				if(t != 1)
					fprintf(fp,"%s","-");
				sprintf(temp, "%d",path[i][j]+1);
				fprintf(fp,"%s",temp);
			}
		}
		fprintf(fp,"%s","\b");
		fprintf(fp,"%s","\n");
	}

	fclose(fp);
}

void add_to_visited(int node)
{
	int i;
	for(i=0;i<number_of_nodes;++i)
	{
		if(visited_nodes[i] == -1)
		{
			visited_nodes[i]=node;
			return;
		}
	}
	return;
}

int check_visited(int node)
{
	int i;
	for(i=0;i<number_of_nodes;++i)
	{
		if(visited_nodes[i] == node)
			return 1;
	}
	return 0;
}


main(int argc, char *argv[])
{
	int i,j,temp1,temp2,temp3,temp4;
    node_id=atoi(argv[2]);
    char *infile=(char *)malloc(sizeof(char)*22);
    infile=argv[4];
    char *outfile=(char *)malloc(sizeof(char)*22);
    outfile=argv[6];
    hi=atoi(argv[8]);
    lsai=atoi(argv[10]);
    sp=atoi(argv[12]);
    port=20000+node_id;
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("Socket");
        exit(1);
    }
	struct sockaddr_in server_addr, client_addr;
	server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    bzero(&(server_addr.sin_zero), 8);

    if (bind(sock, (struct sockaddr *) &server_addr,
            sizeof (struct sockaddr)) == -1) {
        perror("Bind");
        exit(1);
    }

    FILE *fp;
	fp=fopen(argv[4],"r");
	fscanf(fp, "%d %d", &number_of_nodes, &number_of_edges);
	
	for(i=0;i<number_of_nodes;i++)
	{
		for(j=0;j<number_of_nodes;j++)
		{
			links[i][j]=-1;
			min_cost[i][j]=-1;
			max_cost[i][j]=-1;
		}
	} 
	for(i=0;i<number_of_edges;i++)
	{
		fscanf(fp, "%d",&temp1);
		fscanf(fp, "%d",&temp2);
		fscanf(fp, "%d",&temp3);
		fscanf(fp, "%d",&temp4);
		links[temp1-1][temp2-1]=1;
		links[temp2-1][temp1-1]=1;
		min_cost[temp1-1][temp2-1]=temp3;
		min_cost[temp2-1][temp1-1]=temp3;
		max_cost[temp1-1][temp2-1]=temp4;
		max_cost[temp2-1][temp1-1]=temp4;
	}
	
    pthread_create(&send_thread,NULL,send_hello_message,NULL);
    pthread_create(&receive_thread,NULL,receive_hello_reply_message,NULL);
    pthread_create(&lsa_thread,NULL,lsa_message,NULL);
    pthread_create(&spf_thread,NULL,spf_algorithm,NULL);
    pthread_create(&clk_thread,NULL,clk_counter,NULL);
    pthread_exit(NULL);
}
