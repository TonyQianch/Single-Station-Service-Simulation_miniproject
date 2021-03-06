#include "pch.h"
#define _CRT_SECURE_NO_WARNINGS
#include <cstdio>
#include <iostream>
#include <cmath>
#include <ctime>
#include <cstdlib>
#include <string>
#include <float.h>

using namespace std;

#define Q_LIMIT	100
#define HOGOUT 2
#define HOGIN 3
#define DEPART 4
#define EXITQ 5

class trainparameter
{
public:
	int trainname; //code name of the train
	int crewcode; //crew number currently on/assigned to the train
	bool trainhoggedout;
	float unloadtime; //time needed to unload, uniform(3.5,4.5)
	float remaintime; //remaining time of crew, initially uniform(6,11), 12-hogouttime
	float timeinsystem; //start at arrival, end at departure
	float timeinQ; //start at enterQ, end at exitQ; if no Q, set to 0
	float timeout; //start at hog out, end at hog in, can be count multiple times
	float arrival; //record arrival time
	float startwait; //record enterQ time
	float starthogout; //record start hog out time
	int hogoutcount; //+1 each time hog out
};

bool readfile; //determin if reading schedule file based on if the input is two or three statements
bool trainondock; //whether there is a train on dock
float currenttime, time_last_event; //time recording variable
float time_end, mean_interarrival; //variables from user input
int	next_event_type; //indicator of next event type
float time_next_event[6]; //recorder of earliest schedule of each event time
int total_arrival, crew_num; //ID for each new entity
int currentq, maxq; //statistics variable for Q
float total_timeinsystem, total_timeinQ, total_timeout, max_timeinsystem, max_timeinQ, max_timeout, area_numinq; //stats variable for train
float dockidletime, dockhogouttime, dockbusytime, dockstartbusy, dockstartidle; //statistics variable for dock
trainparameter trainQ[Q_LIMIT + 1]; //record the sequence of trains in Q
float hogtime[Q_LIMIT + 1]; //hogout schedule for all current trains
float backtime[Q_LIMIT + 1]; //crew back time for all current trains
int hog0, hog1, hog2, hog3, hog4, hog5, hogmore; //hog counts for histogram
int hogouttrain, hogintrain; //hog out train indicator
float totalruntime;
FILE *infile1;
FILE *infile2;
FILE *outfile;

/*major event functions*/
void initialize(void);
void timing(void);
void arrival(void); //event 1
void unload(void);
void hogout(int k); //event 2
void hogin(int k); //event 3
void depart(void); //event 4
void exitQ(void); //event 5
void report(void); //event 6

void update_stats(void); //function to calculate time average
void update_next_hog(void); //function to determine the next hog event, called every time a hog is scheduled
void update_next_return(void); //function to determine the next return, called every time a return is scheduled
void addtohist(int k);
void printhist(void);

/*random variable generating functions*/
float interarrival(float mean); //generate time for the next train to arrive
float unloadingtime(float lower, float upper); //time needed to unload the train
float crewremainingtime(float lower, float upper); //remaining time of crew when train arrives
float crewreturntime(float lower, float upper); //time to return after hog out

float max(float a, float b); //maximum function

float max(float a, float b) {
	if (a >= b) return a;
	else return b;
}//function to find maximum of two values

float interarrival(float mean) {
	float u;
	u = (float)rand() / (float)RAND_MAX;
	return -log(u)*mean;
}//exponential random value

float unloadingtime(float lower, float upper) {
	float u;
	u = (float)rand() / (float)RAND_MAX;
	return u * (upper - lower) + lower;
}//uniform random value

float crewremainingtime(float lower, float upper) {
	float u;
	u = (float)rand() / (float)RAND_MAX;
	return u * (upper - lower) + lower;
}//uniform random value

float crewreturntime(float lower, float upper) {
	float u;
	u = (float)rand() / (float)RAND_MAX;
	return u * (upper - lower) + lower;
}//uniform random value

void update_next_hog(void) {
	float nexthogtime = (float) 1.0e+10;
	int nexthogtrain = 0;
	for (int i = 0; i <= currentq; i++) { //find earliest hog time for all current trains
		if (nexthogtime > hogtime[i]) {
			nexthogtime = hogtime[i];
			nexthogtrain = i;
		}
	}
	time_next_event[HOGOUT] = nexthogtime;
	hogouttrain = nexthogtrain;
}

void update_next_return(void) {
	float nextreturntime = 1.0e10;
	int nextreturntrain = 0;
	for (int i = 0; i <= currentq; i++) { //find earliest return time for all current trains
		if (nextreturntime > backtime[i]) {
			nextreturntime = backtime[i];
			nextreturntrain = i;
		}
	}
	time_next_event[HOGIN] = nextreturntime;
	hogintrain = nextreturntrain;
}

void update_stats(void) {
	float time_since_last_event; //duration between events (width of integral block)
	time_since_last_event = currenttime - time_last_event;
	area_numinq += currentq * time_since_last_event; //time total for Q: block area = width * height

	time_last_event = currenttime; //update last event
}

void addtohist(int k) {
	switch (k)
	{
	case 0:
		hog0++;
		break;
	case 1:
		hog1++;
		break;
	case 2:
		hog2++;
		break;
	case 3:
		hog3++;
		break;
	case 4:
		hog4++;
		break;
	case 5:
		hog5++;
		break;
	default:
		hogmore++;
		break;
	}
}

void printhist(void) {
	int i;
	fprintf(outfile, "\n [0]: \t%d",hog0);
	fprintf(outfile, "\n [1]: \t%d", hog1);
	if (hog2 > 0) fprintf(outfile, "\n [2]: \t%d", hog2);
	if (hog3 > 0) fprintf(outfile, "\n [3]: \t%d", hog3);
	if (hog4 > 0) fprintf(outfile, "\n [4]: \t%d", hog4);
	if (hog5 > 0) fprintf(outfile, "\n [5]: \t%d", hog5);
	if (hogmore > 0) fprintf(outfile, "\n [>5]: \t%d", hogmore);

	fprintf(outfile, "\n\n\n");
}

void initialize(void) //beginning of simulation
{
	/*Initialize simulation clock*/
	currenttime = 0.0;
	time_last_event = currenttime;
	fprintf(outfile, "\nTime 0.00, START SIMULATION.\n");

	/*Initialize the state variables*/
	trainondock = 0;
	currentq = 0;
	total_arrival = 0;
	maxq = 0;

	/*Initialize all statistics to 0*/
	total_timeinsystem = 0;
	total_timeinQ = 0;
	total_timeout = 0;
	max_timeinsystem = 0;
	max_timeinQ = 0;
	max_timeout = 0;
	area_numinq = 0;
	dockidletime = 0;
	dockhogouttime = 0;
	dockbusytime = 0;

	/*Initialize empty queue*/
	for (int i = 0; i < Q_LIMIT + 1; i++) {
		//trainQ[i] = NULL;
		hogtime[i] = (float) 1.0e+30;
		backtime[i] = (float) 1.0e+30;
	}

	/*Initialize hogout count for histogram*/
	hog0 = 0;
	hog1 = 0;
	hog2 = 0;
	hog3 = 0;
	hog4 = 0;
	hog5 = 0;
	hogmore = 0;

	/*Initialize other events list*/
	for (int i = 0; i < 6; i++) {
		time_next_event[i] = (float) 1.0e+30;
	}

	//Schedule first arrival
	if (readfile) { //if reading from schedule
		float firstarrival;
		fscanf(infile1, "%f", &firstarrival);
		time_next_event[1] = firstarrival;
	}
	else //if generating random value
		time_next_event[1] = currenttime + interarrival(mean_interarrival);


	/*Initialize dock idle status*/
	dockstartidle = currenttime;
	dockstartbusy = (float) 1.0e+30;
}

void timing(void) {
	int i;
	float min_next_event = (float) 1.0e+10;
	next_event_type = 0;
	for (i = 1; i <= 5; i++) {
		if (time_next_event[i] < min_next_event) {
			min_next_event = time_next_event[i];
			next_event_type = i;
		}
	}
	if (next_event_type == 0) {
		fprintf(outfile, "\nTime %.2f, event list empty. Proceed to generate report.\n\nEND OF SIMULATION\n", currenttime);
		next_event_type = 6;
		totalruntime = currenttime;
	}
	time_last_event = currenttime;
	currenttime = min_next_event;
}

void arrival(void)
{
	total_arrival++; //count arrival
	crew_num++; //update crew number code

	/*Generate unloading time and remaining crew time for the arrived train*/
	trainparameter train;

	if (readfile) {
		fscanf(infile1, "%f %f", &train.unloadtime, &train.remaintime);
		float nextarrival;
		if (!feof(infile1)) {
			fscanf(infile1, "%f", &nextarrival); //schedule next arrival
			time_next_event[1] = nextarrival;
		}
		else time_next_event[1] = (float) 1.0e+30; //terminate at end of file.
	}
	else {
		train.unloadtime = unloadingtime(3.5, 4.5);
		train.remaintime = crewremainingtime(6, 11);
		time_next_event[1] = currenttime + interarrival(mean_interarrival);
		if (time_next_event[1] > time_end) {
			time_next_event[1] = (float) 1.0e+30; //arrivals after end time will be denied
		}
	}

	/*Initialize variables for the arrived train*/
	train.trainname = total_arrival;
	train.crewcode = crew_num;
	train.arrival = currenttime;
	train.hogoutcount = 0;
	train.starthogout = (float) 1.0e+30;
	train.startwait = 0.0;
	train.timeinQ = 0.0;
	train.timeinsystem = 0.0;
	train.timeout = 0.0;
	train.trainhoggedout = 0;

	//report arrival event
	fprintf(outfile, "\nTime %.2f, train %i arrived with crew %i (remaining %f hours), with cargo for %.2f hours of unloading.", currenttime, train.trainname, train.crewcode, train.remaintime, train.unloadtime);

	if (!trainondock && currentq == 0) { //if no train on dock and no queue
		//change dock status
		trainondock = 1;
		dockstartbusy = currenttime;
		dockidletime += currenttime - dockstartidle;

		train.timeinQ = 0.0; //record 0 waiting time

		trainQ[0] = train; //locate the train on dock
		hogtime[0] = currenttime + train.remaintime; //schedule time to hog out
		unload();
	}
	else { //otherwise
		train.startwait = currenttime;
		currentq++;
		trainQ[currentq] = train;
		hogtime[currentq] = currenttime + train.remaintime;
		maxq = (int)max((float)currentq, (float)maxq);
	}
	update_next_hog();
}

void unload(void) {
	trainparameter train;
	train = trainQ[0];
	fprintf(outfile, "\nTime %.2f, train %i enter dock and start unloading for %.2f hours.", currenttime, train.trainname, train.unloadtime);
	time_next_event[DEPART] = currenttime + trainQ[0].unloadtime;
}

void hogout(int k) {

	float hogouttime;
	trainparameter train;

	train = trainQ[k];

	train.trainhoggedout = 1;
	train.starthogout = currenttime; // record time train hogged out
	train.hogoutcount++; //count hog times
	crew_num++;
	train.crewcode = crew_num;

	//generate crew return time
	if (readfile) {
		if (!feof(infile2)) fscanf(infile2, "%f", &hogouttime);
		else {
			fprintf(outfile, "\n\nNot enough crew to come in. Simulation terminated.\n");
			report();
			fprintf(outfile, "!ERROR!\nSimulation terminated with a problem. Check input data file.\n");
			exit(0);
		}
	}
	else hogouttime = crewreturntime(2.5, 3.5);

	train.remaintime = 12 - hogouttime;
	fprintf(outfile, "\nTime %.2f, train %i hogged out, crew %i has been called and will take %.2f hours to arrive.", currenttime, train.trainname, crew_num, hogouttime);
	if (k == 0) { //in the case of server hog out
		fprintf(outfile, "(Server hogged out)");
		time_next_event[DEPART] += hogouttime; //delay depart time
		dockbusytime += currenttime - dockstartbusy; //record dock busy time
		dockstartidle = currenttime; //start recording dock idle time
	}
	hogtime[k] = currenttime + 12;
	backtime[k] = currenttime + hogouttime;
	update_next_hog();
	update_next_return();
	trainQ[k] = train;
}

void hogin(int k) {
	fprintf(outfile, "\nTime %.2f, crew %i returned to train %i.", currenttime, trainQ[k].crewcode, trainQ[k].trainname);
	trainQ[k].timeout += currenttime - trainQ[k].starthogout; //record total hogout time for the train
	trainQ[k].trainhoggedout = 0.0;
	backtime[k] = (float) 1.0e+30;
	update_next_return();
	if (k == 0) { //in the case of server hog out
		fprintf(outfile, "(Resume unloading)");
		dockstartbusy = currenttime; //start recording dock busy time
		dockidletime += currenttime - dockstartidle; //record dock idle time
		dockhogouttime += currenttime - dockstartidle; //record dock hogout time
	}
}

void depart(void) {
	trainparameter train;
	train = trainQ[0];

	fprintf(outfile, "\nTime %f, train %d finish unload and leave dock", currenttime, train.trainname);

	/*schedule the first train in Q to exit, otherwise set dock to idle*/
	if (currentq != 0) {
		if (trainQ[1].trainhoggedout) {
			time_next_event[EXITQ] = backtime[1];
			dockbusytime += currenttime - dockstartbusy; //record dock busy time
			trainondock = 0; //change dock status to idle
			dockstartidle = currenttime; //start recording dock idle time
			fprintf(outfile, "\nTime %.2f, train %d is called to exit queue while hogged out, dock is idle", currenttime, trainQ[1].trainname);
		}
		else {
			time_next_event[EXITQ] = currenttime;
		}
	}
	else {
		dockbusytime += currenttime - dockstartbusy; //record dock busy time
		trainondock = 0; //no train on dock
		dockstartidle = currenttime; //start recording dock idle time
		fprintf(outfile, "; no train in queue, dock is idle.");
	}

	train.timeinsystem = currenttime - train.arrival; //record train time in system;
	total_timeinsystem += train.timeinsystem;
	max_timeinsystem = max_timeinsystem > train.timeinsystem ? max_timeinsystem : train.timeinsystem;
	total_timeinQ += train.timeinQ;
	max_timeinQ = max_timeinQ > train.timeinQ ? max_timeinQ : train.timeinQ;
	total_timeout += train.timeout;
	max_timeout = max_timeout > train.timeout ? max_timeout : train.timeout;

	addtohist(train.hogoutcount);

	hogtime[0] = (float) 1.0e+30;
	backtime[0] = (float) 1.0e+30;
	time_next_event[DEPART] = (float) 1.0e+30;
	update_next_hog();
	update_next_return();
}

void exitQ(void) {
	trainparameter train;
	train = trainQ[1];
	trainQ[1].timeinQ = currenttime - train.startwait;//calculate time in Q
	fprintf(outfile, "\nTime %.2f, train %d enter dock for unloading after waiting in Q for %.2f hours.", currenttime, train.trainname, train.timeinQ);
	if (!trainondock) { //if dock is idle, set it to busy
		trainondock = 1;
		dockstartbusy = currenttime;
		dockidletime += currenttime - dockstartidle;
	}
	//move train to dock and move Q one position forward
	for (int i = 0; i <= currentq; i++) {
		trainQ[i] = trainQ[i + 1];
		hogtime[i] = hogtime[i + 1];
		backtime[i] = backtime[i + 1];
	}
	update_next_hog();
	unload();
	currentq--;
	time_next_event[EXITQ] = (float) 1.0e+30;
}

void report(void) {
	float p_busy, p_idle, p_out; //calculate dock percentage times
	p_busy = 100 * dockbusytime / totalruntime;
	p_idle = 100 * dockidletime / totalruntime;
	p_out = 100 * dockhogouttime / totalruntime;

	float average_q;
	average_q = area_numinq / totalruntime;

	fprintf(outfile, "\n\n\nTotal number of trains served: %d", total_arrival);
	fprintf(outfile, "\nMaximum time in system for a train: %.4f hours", max_timeinsystem);
	fprintf(outfile, "\nAverage time in system for a train: %.4f hours", total_timeinsystem / total_arrival);
	fprintf(outfile, "\nMaximum time in queue for a train: %.4f hours", max_timeinQ);
	fprintf(outfile, "\nAverage time in queue for a train: %.4f hours", total_timeinQ / total_arrival);
	fprintf(outfile, "\nPercentage of dock busy time: %.4f percent", p_busy);
	fprintf(outfile, "\nPercentage of dock idle time: %.4f percent", p_idle);
	fprintf(outfile, "\nPercentage of dock hogged out time: %.4f percent", p_out);
	fprintf(outfile, "\nThe maximum number of trains in queue was: %d", maxq);
	fprintf(outfile, "\nTime average number of trains in queue was: %.4f", average_q);

	fprintf(outfile, "\nThe histogram of hog out count per train is:\n");
	printhist();
}

int main(int argc, char *argv[])
{
	srand((unsigned int)time(NULL));
	/*check number of arguements input*/
	if (argc == 4) { //2 arguments,
		infile1 = fopen(argv[2], "r");
		infile2 = fopen(argv[3], "r");
		readfile = 1;
	}
	else if (argc == 3) {
		time_end = atof(argv[1]);
		mean_interarrival = atof(argv[2]);
		readfile = 0;
	}
	else {
		printf("\nInput not valid\n");
		exit(1);
	}

	/*Write report heading and input parameter*/
	outfile = fopen("train_output.txt", "w");
	fprintf(outfile, "Train unloading system simulation\n\n");

	/*Initialize simulation*/
	initialize();

	do {
		timing();

		update_stats();

		switch (next_event_type)
		{
		case 1: {
			arrival();
			break;
		}
		case HOGOUT:
			hogout(hogouttrain);
			break;
		case HOGIN:
			hogin(hogintrain);
			break;
		case DEPART:
			depart();
			break;
		case EXITQ:
			exitQ();
			break;
		case 6:
			report();
			break;
		}
	} while (next_event_type != 6);

	//close all files
	if (readfile) {
		fclose(infile1);
		fclose(infile2);
	}
	fclose(outfile);

	return 0;
}

