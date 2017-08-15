/*
 * Copyright 2017 Antonio Pastor anpastor{at}it.uc3m.es
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <atomic>         // std::atomic
#include <iostream>
#include <sstream>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <unistd.h>
#include <utility>
#include <unordered_map>
#include <pqxx/pqxx>
#include <gflags/gflags.h>
#include <boost/thread.hpp>

#include "zmqpp/zmqpp.hpp"

using std::cout;
using std::endl;
using std::string;

typedef std::pair<int, int> score_pair;
typedef std::unordered_map<string, score_pair> lookup_map;

// GLOBAL VARIABLES
std::shared_ptr<lookup_map> referrerLookup;
string sendToSocket, receiveFromSocket, fwdToSocket;
boost::thread_group workers;

DEFINE_string(initday, "none", "Day of the database to use for the hash tables (in format YYMMDD)");
DEFINE_string(dspIP, "127.0.0.1", "IP address of the DSP");
DEFINE_string(dbIP, "127.0.0.1", "IP address of the database (data processing module)");
DEFINE_string(dbPWD, "password", "password of the database");
DEFINE_string(dbUSER, "username", "database user");
DEFINE_string(dbNAME, "nameles", "database name");
DEFINE_int32(nWorkers, 4, "Number of workers");
DEFINE_int32(rcvport, 58501, "\"Receive from\" port");
DEFINE_int32(sndport, 58505, "\"Send to\" port");
DEFINE_int32(fwdport, 58510, "Data analysis forwarding port (at database host)");
DEFINE_int32(notifyport, 58520, "Port for listening notifications of score updates (0 for none)");
DEFINE_int32(min_total, 250, "Minimum number of visits to consider a domain score");

void SIGINT_handler(int s){
           printf("Caught signal %d\n",s);
           workers.interrupt_all();
}

void worker_func();
void score_updates_listener(const string& scoreUpdatesSocket, const string& db_connect, const int min_total);
std::shared_ptr<lookup_map> retrieve_scores(const string& db_connect, const string& day, const int min_total);


int main(int argc, char *argv[]) {
	google::ParseCommandLineFlags(&argc, &argv, true);

	sendToSocket = "tcp://" + FLAGS_dspIP + ":" + std::to_string(FLAGS_sndport);
	receiveFromSocket = "tcp://" + FLAGS_dspIP + ":" + std::to_string(FLAGS_rcvport);
  if (FLAGS_fwdport!=0){
	   fwdToSocket = "tcp://" + FLAGS_dbIP + ":" + std::to_string(FLAGS_fwdport);
  } else {
		fwdToSocket = "none";
	}
	string scoreUpdatesSocket("tcp://" + FLAGS_dbIP + ":" + std::to_string(FLAGS_notifyport));

	string db_connect("dbname=" + FLAGS_dbNAME + " user="+ FLAGS_dbUSER + " host="+ FLAGS_dbIP + " password=" + FLAGS_dbPWD);

	std::shared_ptr<lookup_map> newLookup;
	if (FLAGS_initday != "none"){
	newLookup = retrieve_scores(db_connect, FLAGS_initday, FLAGS_min_total);
} else {
	newLookup = std::make_shared<lookup_map>();
}
	std::atomic_store(&referrerLookup, newLookup);

	for( int x=0; x<FLAGS_nWorkers; ++x ) {
	    workers.create_thread(worker_func);
	}

	boost::thread updaterThread(score_updates_listener, scoreUpdatesSocket, db_connect, FLAGS_min_total);
	struct sigaction sigIntHandler;

	sigIntHandler.sa_handler = SIGINT_handler;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;

	sigaction(SIGINT, &sigIntHandler, NULL);

	workers.join_all();
	updaterThread.interrupt();
	updaterThread.join();

	return 0;
}


void worker_func(){
	zmqpp::context_t context;


	zmqpp::socket_t puller(context, zmqpp::socket_type::pull); //  Socket to receive messages on
	zmqpp::socket_t reply_pusher(context, zmqpp::socket_type::push); //  Socket to send messages to
	zmqpp::socket_t fwder(context, zmqpp::socket_type::push); //  Socket to foward messages for the data processing module
	puller.set(zmqpp::socket_option::receive_timeout, 10000); // 10 seconds
	reply_pusher.set(zmqpp::socket_option::send_timeout, 0); // 0 seconds
	fwder.set(zmqpp::socket_option::receive_timeout, 10000); // 10 seconds
	try {
		puller.connect(receiveFromSocket);
		reply_pusher.connect(sendToSocket);
		if (fwdToSocket!= "none"){
			fwder.connect(fwdToSocket);
		}
	} catch (zmqpp::zmq_internal_exception &e){
		cout << "Exception: " << e.what() << endl;
		puller.close();
		reply_pusher.close();
		fwder.close();
		context.terminate();
		return;
	}

	zmqpp::message_t query, reply;
	uint32_t reqID;
	// string ip;
	string referrer;
	lookup_map::iterator ref_it;
	while ( ! boost::this_thread::interruption_requested() ) {
		if (puller.receive(query)){
			query.get(reqID, 0);
			query.get(referrer, 1);
			//	query.get(ip, 2);
			// cout << reqID << " " << referrer << " " << ip << endl;
			auto lookupTable = std::atomic_load(&referrerLookup);
			ref_it = lookupTable->find(referrer);
			if (ref_it != lookupTable->end()){
				reply << reqID << ref_it->second.first << ref_it->second.second;
				reply_pusher.send(reply);//,true);
				if (fwdToSocket!= "none"){
					fwder.send(query);
				}
			}
//			cout << t0.tv_nsec << ' ' << t1.tv_nsec << ' ' << latency << endl;
//			cout << reqID << " - " << referrer.c_str() << " - " << ip << endl;
		}
		// else{
		// 	cout << "worker "<<  boost::this_thread::get_id() << " timeout " << boost::this_thread::interruption_requested() << endl;
		// }
	}

	// puller.close();
	// reply_pusher.close();
	// fwder.close();
	//context.terminate();
}

void score_updates_listener(const string& scoreUpdatesSocket, const string& db_connect, const int min_total){
	zmqpp::context_t context;
	zmqpp::socket_t socket(context, zmqpp::socket_type::pull);
	try {
			socket.bind("tcp://*:1138");
	} catch (zmqpp::zmq_internal_exception &e){
		cout << "Exception: " << e.what() << endl;
		socket.close();
		context.terminate();
		exit(-1);
}
	zmqpp::message_t msg;
	string msg_str;
	// forever loop
	while ( ! boost::this_thread::interruption_requested() ) {

			//  Wait for next request from client
			socket.receive(msg);
			msg.get(msg_str, 0);
			std::atomic_store(&referrerLookup, retrieve_scores(db_connect, FLAGS_initday, FLAGS_min_total));
	}
	socket.close();
	context.terminate();
	return;
}

std::shared_ptr<lookup_map> retrieve_scores(const string& db_connect, const string& day, const int min_total){

	std::shared_ptr<lookup_map> referrerLookup_ptr = std::make_shared<lookup_map>();
	pqxx::connection c(db_connect);
	pqxx::read_transaction txn(c);

	pqxx::result r = txn.exec("SELECT max(score_" + day + "), "
			+ " percentile_cont(0.75) within group (order by score_" + day + ") as p075, "
			+ " percentile_cont(0.50) within group (order by score_" + day + ") as p05, "
			+ " percentile_cont(0.25) within group (order by score_" + day + ") as p025 "
			+ " FROM stats.referrer WHERE total_" + day + ">=" + std::to_string(min_total) + ";");

	int perc100 = r[0][0].as<int>();
	int perc75 = r[0][1].as<int>();
	int perc50 = r[0][2].as<int>();
	int perc25 = r[0][3].as<int>();

	cout << "perc100: " << perc100 << endl;
	cout << "perc75: " << perc75 << endl;
	cout << "perc50: " << perc50 << endl;
	cout << "perc25: " << perc25 << endl;

	double UHR = perc100 - perc50;
	double IQR = perc75 - perc25;

	double th_noConf = perc25 - 1.5*IQR;
	double th_lowConf = perc100 -3*UHR;
	double th_modConf = perc100 - 2*UHR;

	cout << "th_modConf: " << th_modConf << endl;
	cout << "th_lowConf: " << th_lowConf << endl;
	cout << "th_noConf: " << th_noConf << endl;

	r = txn.exec("SELECT referrer, score_" + day + ","
			+ " CASE WHEN score_" + day + "<" + std::to_string(th_noConf) + " THEN 0 "
			+ "      WHEN score_" + day + "<" + std::to_string(th_lowConf) + " THEN 1 "
			+ "      WHEN score_" + day + "<" + std::to_string(th_modConf) + " THEN 2 "
			+ " ELSE 3 END"
			+ " FROM stats.referrer WHERE total_" + day + ">=" + std::to_string(min_total) + ";");
	txn.commit();
	cout << "Lookup table starts with " << referrerLookup_ptr->size() << " domains -> filled with ";

	for (auto row: r){
		referrerLookup_ptr->insert(std::make_pair<string,score_pair>(row[0].as<string>(),score_pair(row[1].as<int>(),row[2].as<int>())));
	}
	cout << referrerLookup_ptr->size() << endl;

	return referrerLookup_ptr;

}
