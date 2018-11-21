/*
 *
 *	veprof
 *
 * 	NEC SX Aurora-Tsubasa performace counter sampler
 *
 *	usefull demonstration how to sample performance counter of VE processes
 *	from VH.
 *
 *	usage:
 *		gather full profile of a serial application:
 *		./veprof ./ve_exe
 *
 * 	options:
 * 		-e executable	name of executable to take symbols from, usefull
 * 			for wrapper execution
 * 		-s sample frequency in Hz
 *
 *	(c) Holger Berger NEC Deutschland GmbH 2018
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h>
#include <curses.h>


#include <atomic>
#include <thread>
#include <string>
#include <sstream>
#include <iostream>
#include <vector>
#include <utility>
#include <algorithm>
#include <fstream>

#include "boost/program_options.hpp"
namespace po = boost::program_options;

// BUG veosinfo.h, no guard in include, we do it here
extern "C" {
#include <veosinfo.h>
}
#include <libved.h>


#define FILEVERSION "0.1"

const long default_sample_time_usec = 10000; // 10000 = 100 Hz
long sample_time_usec;

const int counters=19;

// global variables
//  thread communication
std::atomic<long> doneflag; 
std::atomic<long> card;
std::atomic<long> pid;

//  flags
bool debug = false;
bool verbose = false;
int fullcardnr=-1;
std::vector<std::string> command;
char **cargs;
char *carray;

// data, separated from search list, keep it small
struct data_e {
	uint64_t addr;			// sample address
	uint64_t count;			// number of hits to this sample
	double e_time;			// elapsed time
	uint64_t vals[counters-1];	// counters as sampled, to be aggregated, -1 as IC can be ommited
	std::string *symbolname;	// symbolic name
};

// search list for binary search, with pointer to real data to keep it small for good cache usage
struct addr_e {
	uint64_t addr;	// address of symbol entry
	data_e *data;	// pointer to data with name and samples
};

// global data
std::vector<addr_e> searchtable;
std::vector<data_e> datatable;
uint64_t pmmr = 0xffffffffffffffff;

// forward prototypes
void get_ve_pid_list(int cardid, std::vector<pid_t> &pidlist);
void sig_handler_nothing(int);
void sig_handler_setdone(int);

// collect the data
void sample() {
	int regs[counters] = 	{IC, USRCC, PMC00, PMC01, PMC02, PMC03, PMC04, PMC05, PMC06, PMC07, PMC08, PMC09,
        		PMC10, PMC11, PMC12, PMC13, PMC14, PMC15, VL};
	uint64_t vals[counters],oldvals[counters];
	std::vector<pid_t> pidlist;

	/*
	// signal handler for this thread
	struct sigaction act;
	act.sa_handler = sig_handler_setdone;
	sigaction(SIGINT, &act, NULL);
 	*/

	while(1) {
		struct timespec time1,time2, time3;
		double dtime1, dtime2, dtime3;
		clock_gettime(CLOCK_MONOTONIC_RAW, &time1);
		dtime1 = time1.tv_nsec + time1.tv_sec*1000000000.0;

		// data collection from running aurora cards
		if (pid.load()!=-1) {

			if (pmmr == 0xffffffffffffffff) {
				int reg = PMMR;
				int r=ve_get_regvals(card, pid, 1, &reg, vals);
				pmmr=vals[0];
			}

			// sample ON process
			int r=ve_get_regvals(card, pid, 19, regs, vals);	
			clock_gettime(CLOCK_MONOTONIC_RAW, &time3);
			dtime3 = time3.tv_nsec + time3.tv_sec*1000000000.0;

			// search symbol to address
			if (vals[0] != 0) {
				auto addr = std::upper_bound(searchtable.begin(), searchtable.end(), addr_e{vals[0], nullptr}, 
								[](const addr_e &a, const addr_e &b){ return (a.addr < b.addr); } );
				if (addr != searchtable.end() && addr!=searchtable.begin()) {
					addr--;
					addr->data->count++;
					addr->data->e_time+=(dtime3-dtime1);
					for(int i=0; i<counters-1; i++) {
						addr->data->vals[i]+=vals[i+1]-oldvals[i+1];
					}	
					for(int i=0; i<counters; i++) {
						oldvals[i]=vals[i];
					}
				} else {
					//if (debug) 
					//	std::cerr << "veprof: unknown symbol" << std::endl;
					// TODO: count unknown symbols
				}
			}
		} else {
			// sample ALL processes on one card
			pidlist.clear();
			get_ve_pid_list(fullcardnr, pidlist);
			for(auto ppid : pidlist) {
				if (debug) std::cout << "<< sampling " << ppid << " >> " << std::endl;

				if (pmmr == 0xffffffffffffffff) {
					int reg = PMMR;
					//FIXME nr is wrong
					//r=ve_get_regvals(fullcardnr, ppid, 1, &reg, vals);
					int r=ve_get_regvals(0, ppid, 1, &reg, vals);
					pmmr=vals[0];
				}

				// FIXME this card nr is probably wrong
				// int r=ve_get_regvals(fullcardnr, ppid, 19, regs, vals);	
				int r=ve_get_regvals(0, ppid, 19, regs, vals);	
				clock_gettime(CLOCK_MONOTONIC_RAW, &time3);
				dtime3 = time3.tv_nsec + time3.tv_sec*1000000000.0;

				if (debug) std::cout << "<< addr = " << vals[0] << " " << r << std::endl;

				// search symbol to address
				if (vals[0] != 0) {
					auto addr = std::upper_bound(searchtable.begin(), searchtable.end(), addr_e{vals[0], nullptr}, 
									[](const addr_e &a, const addr_e &b){ return (a.addr < b.addr); } );
					if (addr != searchtable.end() && addr!=searchtable.begin()) {
						addr--;
						addr->data->count++;
						addr->data->e_time+=(dtime3-dtime1);
						for(int i=0; i<counters-1; i++) {
							addr->data->vals[i]+=vals[i+1]-oldvals[i+1];
						}	
						for(int i=0; i<counters; i++) {
							oldvals[i]=vals[i];
						}
					} else {
						//if (debug) 
							std::cerr << "veprof: unknown symbol" << std::endl;
						// TODO: count unknown symbols
					}
				}
			}
		}


		if (doneflag.load(std::memory_order_acquire)>=1) break;

		// wait until sample interval is over
		clock_gettime(CLOCK_MONOTONIC_RAW, &time2);
		dtime2 = time2.tv_nsec + time2.tv_sec*1000000000.0;
		long diff = (sample_time_usec-(long)((dtime2-dtime1)/1000.0));
		if (diff>=0) {
			usleep(diff);
		} else {
			if (verbose)
				std::cerr << "<< veprof: processing did not complete within sample interval >>" << std::endl;
		}
	}
}

// call nnm to get sorted list of address/symbols
void getsymboltable(const char *cmd, std::vector<std::pair<uint64_t, std::string>> &list) {
	FILE *pipe;
	const int maxline=8192;
	char linebuffer[maxline];

	uint64_t address;
	std::string type;
	std::string symbol;

	std::string cmdline = std::string("/opt/nec/ve/bin/nnm -C -n ")+cmd;
	pipe=popen(cmdline.c_str(), "r");
	
	if (pipe) {
		while(!feof(pipe)) {
				if (fgets(linebuffer, maxline, pipe) != NULL) {
				// we need Text symbols only
				if (strlen(linebuffer)>=18 && linebuffer[17] == 'T') { 
					// trim \n
					linebuffer[strlen(linebuffer)-1]='\0';
					std::istringstream line(linebuffer);
					line >> std::hex >> address >> type;
					line.ignore(1,'\n');	// ignore space in front of symbol
					std::getline(line, symbol);
					list.push_back(std::make_pair(address, symbol));
				}
			}
		}
		pclose(pipe);
	} else {
		std::cerr << "could not read symbol table from exectuable.\n" << std::endl;
		exit(2);
	}
}

// stolen from EF
int nodeid_of_pid(pid_t pid)
{
	int i, nodeid;
	struct ve_nodeinfo ni;

	if (ve_node_info(&ni) != 0) {
		fprintf(stderr, "ve_node_info failed!\n");
		exit(1);
	}

	if (debug)
		printf(">> nodeid_of_pid: pid=%ld ni.total_node_count=%d\n",pid, ni.total_node_count);

	// find pid in one of the VE nodes...
	for (i = 0; i < ni.total_node_count; i++) {
		if (ni.status[i])
			continue;
		nodeid = ni.nodeid[i];
		if (!ve_check_pid(nodeid, pid))
			return nodeid;
	}

	return -1;
}


// get process list of all cards with cardid=-1 or a specific card
void get_ve_pid_list(int cardid, std::vector<pid_t> &pidlist) {
	char pathbuffer[256];
	unsigned long pid;

	for(int c=0; c<10; c++) {
		if (cardid==-1 || cardid==c) {
			std::ostringstream path(pathbuffer);
			path << "/sys/class/ve/ve" << c << "/task_id_all";
			std::ifstream file(path.str());
			if (file.good()) {
				pid=-1;
				while(!file.eof()) {
						file >> pid;
						if(pid!=-1) {
							// if (debug) std::cout << "<< append to pid list " << pid << std::endl;	
							pidlist.push_back((pid_t)pid);
						}
				}
				file.close();	
			}
		}
	}
}


// get parent process id for pid
pid_t getppid(pid_t pid) {
	// if (debug) std::cout << "<< getppid " << pid << " >>" << std::endl;
	char pathbuffer[256];
	std::ostringstream path(pathbuffer);
	path << "/proc/" << pid << "/stat";
	std::ifstream file(path.str());
	if (file.good()) {
		std::string token, state;
		pid_t pid;
		while(!file.eof()) {
			file >> token;
			// if (debug) std::cout << "<< token " << token << std::endl;
			if (token[token.length()-1]==')') {
				file >> state >> pid;
				// if (debug) {
				//	std::cout << "<< state " << state << std::endl;
				//	std::cout << "<< pid " << pid << std::endl;
				//}
				file.close();
				return pid;
			}
		}
		file.close();	
	} 
	return -1;
}

// search for a process having the given process as parent on card 
int find_child_pid(pid_t pid) {
	// if (debug) std::cout << "<< searching child >>" << std::endl;
	std::vector<pid_t> pidlist;
	get_ve_pid_list(-1, pidlist);
	for(auto apid : pidlist) {
		pid_t ppid=-2;
		while(ppid!=1 and ppid!=-1) {
			ppid = getppid(apid);
			if (ppid==pid) return apid;
			apid=ppid;
		}
	} 	
	return -1;
}

// find pid to monitor (if pid itself is not found, search for child processes on cards) and card it runs on
void find_pid_and_card(pid_t childpid, std::string cmd) {
		// first see if we can use the childpid

		bool search = true;

		// check for shebang to speed up wrapper detection
		std::ifstream file(cmd);
		char c1 = file.get();
		char c2 = file.get();
		file.close();
		if (c1=='#' and c2=='!') {
			if (verbose) std::cout << "<< wrapper detected >>" << std::endl;
			search = false;
			card.store(-1, std::memory_order_release);
		}

		// store card ID where that process is running
		// we have to wait a bit to give the process a chance to start, the max iterations might need some tuning
		int waitcount=0;
		while(search) {
			waitcount++;
			card.store(nodeid_of_pid(childpid), std::memory_order_release);
			if (card.load()==-1) {
				if (waitcount>100) break; // waited too long
				usleep(waitcount*1000);   // wait 1 ms
			} else {
				break;
			}
		}
		if (card.load()==-1) {
			// we did not find the process, so we search for a child on all cards, and store pid and card
			pid_t newpid;
			// we again try several times in case process did not yet startup
			int waitcount = 0;
			while(1) {
				waitcount++;
				newpid = find_child_pid(childpid);
				if (newpid!=-1) break;
				if (waitcount>100) break; // waited too long
				usleep(waitcount*1000);   // wait 1 ms
			}
			if (newpid==-1) {
				std::cout << "<< no process to sample found! >>" << std::endl;
				exit(2);
			}

			pid.store(newpid, std::memory_order_release);
			card.store(nodeid_of_pid(newpid), std::memory_order_release);
			if (verbose)
				std::cout << "<< sampling child pid " << newpid << " >>" << std::endl;
		} else {
			// we found pid on card, so we use this pid
			pid.store(childpid, std::memory_order_release);
			if (verbose)
				std::cout << "<< sampling forked pid>>" << std::endl;
		}
		if (card.load()==-1) {
			std::cout << "<< could not find process to sample! exiting. >>" << std::endl;
			exit(2);
		}
		if (verbose)
			std::cout << "<< sampling on card: " <<  card.load() << " >>" << std::endl;
}

// parse command line options
po::variables_map option_parser(int argc, char* argv[])
{
	int samplerate;

	po::options_description desc("options");
	desc.add_options()
	("help,h", "produce help message")
	("command,c", po::value<std::vector<std::string>>(&command), "command (for execution), or first positional argument")
	("executable,e", po::value<std::string>(), "executable (for symboltable)")
	("samplerate,s", po::value<int>(&samplerate), "sample rate [Hz]")
	("fullcard,f", po::value<int>(&fullcardnr), "full card mode, read counters for all processes on card# assuming it is executable given with -e")
	("verbose,v", po::bool_switch(&verbose)->default_value(false), "verbose output")
	("debug,d", po::bool_switch(&debug)->default_value(false), "debug output")
	;

	po::positional_options_description pp;
	pp.add("command", -1);

	po::variables_map vm;
	po::store(po::command_line_parser(argc, argv).options(desc).positional(pp).run(), vm);
	po::notify(vm);

	if (vm.count("help")) {
		std::cout << "Usage: " << argv[0] << ": [options] command [command options]" << std::endl;
		std::cout << desc << "\n";
		exit(0);
	}

	if (vm.count("samplerate")) {
		sample_time_usec= 1000000/samplerate;
	} else {
		sample_time_usec = default_sample_time_usec;
	}
	if (verbose) 
		std::cout << "<< sample interval: "  << sample_time_usec  << " ns >>\n" << std::endl;

	if (vm.count("fullcard") and not (vm.count("executable") or vm.count("command"))) {
		std::cout << "please combine full card mode with -e to specify executable for symbols." << std::endl;
		exit(-1);
	}
	
	// check if there is a command to sample
	if (vm.count("command")==0) {
		std::cout << "no command to sample specified!" << std::endl;
		exit(-1);
	}
		
	// we have to prepare a C array of arguments, this is a mess...
	cargs = new char*[command.size()+1];
	int carraylen=0;
	for(int i=1; i<command.size(); i++) carraylen+=command[i].size()+1;
	carray = new char[carraylen+1];
	char *p=carray;
	for(int i=0; i<command.size(); i++) {
		if (debug)
			std::cout << "<< copying arg " << command[i] << " >>" << std::endl;
		memcpy(p, command[i].c_str(), command[i].size()+1);
		cargs[i]=p;
		p+=command[i].size()+1;
	}
	cargs[command.size()]=nullptr;

	return vm;
}

// for and run forks, runs a given exectuble on the card, figures out pid and samples it
void fork_and_run(po::variables_map &options) {
	// fork a child to execute application on VE
	pid_t childpid = fork();
	if (childpid) {
		
		// we have to find the pid to monitor and the card where to monitor
		find_pid_and_card(childpid, options["command"].as<std::vector<std::string>>()[0]);

		// unset flag before thread spawn
		doneflag.store(0, std::memory_order_release);
		
		// start sampler thread
		std::thread sampler(sample);
		
		// wait for child to terminate
		while(1) {
			int status;
			waitpid(childpid, &status, 0);
			if (debug)
				printf(">> waitpid ended <<\n");
			if (WIFEXITED(status) || WIFSIGNALED(status)) break;
			if (debug)
				printf(">> waitpid loop continued <<\n");
		}
		if (debug)
			printf(">> waitpid loop exited <<\n");
		
		// set flag to terminate
		doneflag.fetch_add(1, std::memory_order_acquire);
		
		// wait for thread to exit
		if (debug)
			printf(">> waiting for join <<\n");

		sampler.join();

		if (debug)
			printf(">> joined <<\n");

		// write out data  TODO: put mpi rank into name
		std::ofstream ofs;
		ofs.open("veprof.out",std::ofstream::out);
		struct ve_cpuinfo ci;
		ve_cpu_info(card.load(), &ci);
		ofs << "# version,PMMR,hostname,card,mhz" << "\n";
		ofs << FILEVERSION << "," << pmmr << "," << getenv("HOSTNAME") << "," << card.load() << "," << ci.mhz << "\n";
		ofs << "# samples,USRCC,EX,VX,FPEC,VE,VECC,L1MCC,VE2,VAREC,VLDEC,PCCC,PMC10,VLEC,VLCME,FMAEC,PTCC,TTCC,VL,symbol\n";
		for(auto e : datatable ) {
			if (e.count>0) {
				ofs << e.count << ",";
				for(int i=0; i<counters-1; i++) {
					ofs << e.vals[i] << ",";
				}
				ofs << *e.symbolname << std::endl;
			}
		}		
		ofs.close();
	} else {
		// we are in another process here

		// start process to monitor
		int r = execv(options["command"].as<std::vector<std::string>>()[0].c_str(), cargs);
		if (r) {
			perror("could not launch on VE:");
		} else {
			printf(">> VE applicationen ended <<\n");
		}
	}	
}

void sig_handler_setdone(int) {
	printf("SIGNAL DONE\n");
	doneflag.store(1, std::memory_order_release);
}

void sig_handler_nothing(int) {
	printf("SIGNAL NOTHING\n");
}


// full card mode, sample all processes on card until ctrl-c is pressed
void full_card(po::variables_map options) {

	pid.store(-1, std::memory_order_release);
	// start sampler thread
	std::thread sampler(sample);

	struct sigaction act;
	act.sa_handler = sig_handler_setdone;
	sigaction(SIGINT, &act, NULL);
	
	// wait for thread to exit
	if (debug)
		printf(">> waiting for join <<\n");

	sampler.join();

	if (debug)
		printf(">> joined <<\n");

	// write out data  TODO: put mpi rank into name
	std::ofstream ofs;
	ofs.open("veprof.out",std::ofstream::out);
	struct ve_cpuinfo ci;
	ve_cpu_info(card.load(), &ci);
	ofs << "# version,PMMR,hostname,card,mhz" << "\n";
	ofs << FILEVERSION << "," << pmmr << "," << getenv("HOSTNAME") << "," << fullcardnr << "," << ci.mhz << "\n";
	ofs << "# samples,USRCC,EX,VX,FPEC,VE,VECC,L1MCC,VE2,VAREC,VLDEC,PCCC,PMC10,VLEC,VLCME,FMAEC,PTCC,TTCC,VL,symbol\n";
	for(auto e : datatable ) {
		if (e.count>0) {
			ofs << e.count << ",";
			for(int i=0; i<counters-1; i++) {
				ofs << e.vals[i] << ",";
			}
			ofs << *e.symbolname << std::endl;
		}
	}		
	ofs.close();
}

// start VE application, and sampler
int main(int argc, char** argv) {

	auto options = option_parser(argc, argv);	
	// if we get here, we have a command to run

	if (debug) {
		if (options.count("executable")) {
			std::cout << "<< executable specified: " << options["executable"].as<std::string>()  
					<<   " >>" << std::endl;
		}
		std::cout << "<< command: " << options["command"].as<std::vector<std::string>>()[0] 
					<< " >>" << std::endl;
	}

	// read symbol table
	std::vector<std::pair<uint64_t, std::string>> symbollist;
	if (options.count("executable")) {
		getsymboltable(options["executable"].as<std::string>().c_str(), symbollist);
	} else {
		getsymboltable(options["command"].as<std::vector<std::string>>()[0].c_str(), symbollist);
	}


	// create search and data tables
	datatable.reserve(symbollist.size());
	searchtable.reserve(symbollist.size());
	if (debug) 
		std::cout << "<< read " << symbollist.size() << " symbols >>" << std::flush;
	for(auto &e : symbollist) {
		data_e de;
		de.addr = e.first;
		de.count = 0;
		de.e_time = 0.0;
		for(int i=0; i<counters-1; i++) {
			de.vals[i] = 0;
		}
		de.symbolname = &e.second; 	// store pointer into symbollist
		datatable.push_back(de);	// store a copy
		addr_e ae = { e.first , &datatable[datatable.size()-1]};
		searchtable.push_back(ae);	// store a copy
	}
	
	// run one of the sample modes
	if (options.count("fullcard")) {
		full_card(options);
	} else {
		fork_and_run(options);
	}
	
}
