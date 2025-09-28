/*
	Original author of the starter code
    Tanzir Ahmed
    Department of Computer Science & Engineering
    Texas A&M University
    Date: 2/8/20
	
	Please include your Name, UIN, and the date below
	Name: Licheng Yi
	UIN: 433009558
	Date: 9/22/2025
*/
#include "common.h"
#include "FIFORequestChannel.h"

using namespace std;
#include <sys/wait.h>


int main (int argc, char *argv[]) {
	int opt;
	int p = 1;
	double t = 0.0;
	int e = 1;

	bool createNewChannel = false;
	vector<FIFORequestChannel*> chans;
	
	string filename = "1.csv";
	while ((opt = getopt(argc, argv, "p:t:e:f:c")) != -1) {
		switch (opt) {
			case 'p':
				p = atoi (optarg);
				break;
			case 't':
				t = atof (optarg);
				break;
			case 'e':
				e = atoi (optarg);
				break;
			case 'f':
				filename = optarg;
				break;
			case 'c':
				createNewChannel = true;
				break;
		}
	}
	//fork client
	pid_t id = fork();

	if(id == 0){
		//child becomes server side
		const char* prog = "./server";
        vector<char*> args;
        args.push_back(const_cast<char*>(prog));
        args.push_back(nullptr);

		execvp(prog, args.data());
	}

	FIFORequestChannel chan("control", FIFORequestChannel::CLIENT_SIDE);

	//create new channel if option -c is included
	if(createNewChannel){
		MESSAGE_TYPE m = NEWCHANNEL_MSG;
		chan.cwrite(&m, sizeof(MESSAGE_TYPE));
		
		//read and connect to new server
		char newChanName[30];
		chan.cread(&newChanName, sizeof(newChanName));
		
		FIFORequestChannel* chan2 = new FIFORequestChannel(newChanName, FIFORequestChannel::CLIENT_SIDE);
		chans.push_back(chan2);
	}

	//choose active channel
	FIFORequestChannel* active = createNewChannel ? chans.front() : &chan;

	char buf[MAX_MESSAGE]; // 256
	datamsg x(p, t, e);
	
	memcpy(buf, &x, sizeof(datamsg));
	active->cwrite(buf, sizeof(datamsg)); // question
	double reply;
	active->cread(&reply, sizeof(double)); //answer
	cout << "For person " << p << ", at time " << t << ", the value of ecg " << e << " is " << reply << endl;
	
	//first thousand data points
	double dt = 0.004;
	double t_i= 0;

	ofstream firstThousand("./received/x1.csv");

	//loop through 1000 times
	for(int i = 0; i < 1000; i++){
		//collect both e's
		datamsg d1(p, t_i, 1);
		datamsg d2(p, t_i, 2);
		
		
		memcpy(buf, &d1, sizeof(datamsg));
		active->cwrite(buf, sizeof(datamsg));
		double e1 = 0;
		active->cread(&e1, sizeof(double));

		memcpy(buf, &d2, sizeof(datamsg));
		active->cwrite(buf, sizeof(datamsg));
		double e2 = 0;
		active->cread(&e2, sizeof(double));

		//write these into x1.csv
		firstThousand << t_i << "," << e1 << "," << e2 << "\n";
		t_i += dt;
	}
	

    //sending a non-sense message, you need to change this
	filemsg fm(0, 0);
	string fname = filename;

	//get file size
	int len = sizeof(filemsg) + (fname.size() + 1);
	char* buf2 = new char[len];
	memcpy(buf2, &fm, sizeof(filemsg));
	strcpy(buf2 + sizeof(filemsg), fname.c_str());
	active->cwrite(buf2, len);  // I want the file length;

	__int64_t fileSize;
	active->cread(&fileSize, sizeof(__int64_t));

	//open file as binary
	string path = "./received/" + filename;
	ofstream outFile(path, ios::binary);
	vector<char> dataBuf(MAX_MESSAGE);

	__int64_t total = fileSize;
	__int64_t offset = 0;

	//transfer files
	while (offset < total) {
		__int64_t remaining = total - offset;
		__int64_t chunk = 0;

		if(remaining > MAX_MESSAGE){
			chunk = MAX_MESSAGE;
		}
		else{
			chunk = remaining;
		}

		filemsg fm1(offset, chunk);
		memcpy(buf2, &fm1, sizeof(filemsg));
		strcpy(buf2 + sizeof(filemsg), fname.c_str());
		active->cwrite(buf2, len);

		active->cread(dataBuf.data(), chunk);
		outFile.write(dataBuf.data(), chunk);

		offset += chunk;
	}
	

	delete[] buf2;

	//close all created channels
	for(FIFORequestChannel* c: chans){
		MESSAGE_TYPE m = QUIT_MSG;
		c->cwrite(&m, sizeof(MESSAGE_TYPE));
		delete c;
	}
	
	// closing the channel    
    MESSAGE_TYPE m = QUIT_MSG;
    chan.cwrite(&m, sizeof(MESSAGE_TYPE));


	//wait here
	int status = 0;
	wait(&status);
}
