// C++ thrift headers 
#include <thrift/concurrency/ThreadManager.h>
#include <thrift/concurrency/PosixThreadFactory.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TSimpleServer.h>
#include <thrift/server/TThreadPoolServer.h>
#include <thrift/server/TThreadedServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TTransportUtils.h>
#include <thrift/TToString.h>
#include <thrift/processor/TMultiplexedProcessor.h>

// Additional C++ headers for querying registered services
#include <thrift/transport/TSocket.h>

// Useful C++ headers
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <cstdlib>

// Thrift-generated stubs for RPC handling
#include "gen-cpp/CommandCenter.h"
#include "gen-cpp/commandcenter_types.h"

// Thrift-generated stubs for communicating with registered
// services
#include "../openephyra-thrift/gen-cpp/QAService.h"
#include "../speech-recognition/kaldi/scripts/kaldi-thrift/gen-cpp/KaldiService.h"
#include "../image-matching/matching-thrift/ImageMatchingService.h"

// Boost libraries
#include <boost/regex.hpp>

// Extras
#include "base64.h"

// define the number of threads in pool
#define THREAD_WORKS 16


using namespace std;
using namespace apache::thrift;
using namespace apache::thrift::concurrency;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace apache::thrift::server;
using namespace cmdcenterstubs;
using namespace qastubs;

/*class MachineData
{
public:
	MachineData(const std::string& _machineName, const int32_t _port)
		: machineName(_machineName), port(_port) {}
	std::string getMachineName() { return machineName; }
	int32_t getPort() { return port; }
private:
	std::string machineName;
	int32_t port;
};

struct Service
{
	std::string machine_name;
	int32_t port;
	std::string type;
	Service(){};
	Service(std::string machine_name_in, int32_t port_in, std::string type_in){
		machine_name = machine_name_in;
		port = port_in;
		type = type_in;
	}
};
*/

class BadImgFileException {};

class CommandCenterHandler : public CommandCenterIf
{
public:
	// ctor: initialize command center's tables
	CommandCenterHandler()
	{
		registeredServices = std::multimap<std::string, MachineData>();
		
	}

	// (dtor defined in CommandCenter.h)

	virtual void registerService(const std::string& serviceType, const MachineData& mDataObj)
	{
		cout << "/-----registerService()-----/" << endl;
		cout << "received request from " << mDataObj.name
		     << ":" << mDataObj.port << ", serviceType = " << serviceType
		     << endl;
		registeredServices.insert( std::pair<std::string, MachineData>(serviceType, mDataObj) );
	
		// DEBUG information (testing only)
		cout << "There are now " << registeredServices.size() << " registered services" << endl;
		cout << "LIST OF REGISTERED SERVICES:" << endl;
		std::multimap<std::string, MachineData>::iterator it;
		for (it = registeredServices.begin(); it != registeredServices.end(); ++it)
		{
			cout << "\t" << (*it).first << "\t"
			     << (*it).second.name << ":"
			     << (*it).second.port << endl;
		}
		// END_DEBUG
		/*cout << "received request from " << machine_name << ":" << port << ", serviceType = " << type << endl;
		if(type == "QA"){
			qa = Service(machine_name, port, type);
		} else if(type == "IMM"){
			imm = Service(machine_name, port, type);
		} else if(type == "ASR"){
			asr = Service(machine_name, port, type);
		}*/
	}

	virtual void handleRequest(std::string& _return, const QueryData& data)
	{
		cout << "/-----handleRequest()-----/" << endl;

		// TODO: refactor
		//---- Select the data to be passed to the services ----//
		// TODO: THIS BREAKS MY COMMAND CENTER CLIENT TESTS
		std::string binary_audio = data.audioData;
		std::string binary_img = data.imgData;
		
		cout << "Decoding audio..." << endl;
		binary_audio = base64_decode(data.audioData);
		
		cout << "Decoding img..." << endl;
		binary_img = base64_decode(data.imgData);
		
/*
		if (data.audioData.b64format)
		{
			cout << "Decoding audio..." << endl;
			binary_audio = base64_decode(data.audioData.file);
		}

		if (data.imgData.b64format)
		{
			cout << "Decoding img..." << endl;
			binary_img = base64_decode(data.imgData.file);
		}
*/
		// NOTE: hard to break this up, b/c you need to pass the N clients around
		// I suppose you could use a struct
		//----Select services based on the client's query----//
		std::multimap<std::string, MachineData>::iterator it;
		
		// TODO: figure out how to resolve scoping to make clients visible
		// Must instantiate all clients in this way.
		// NOTE: TSocket values below are irrelevant; the clients to be used
		// will have their values overwritten, and the rest will be ignored
		boost::shared_ptr<TTransport> asr_socket(new TSocket("localhost", 8080));
		boost::shared_ptr<TTransport> asr_transport(new TBufferedTransport(asr_socket));
		boost::shared_ptr<TProtocol> asr_protocol(new TBinaryProtocol(asr_transport));
		KaldiServiceClient asr_client(asr_protocol);

		boost::shared_ptr<TTransport> qa_socket(new TSocket("localhost", 8080));
		boost::shared_ptr<TTransport> qa_transport(new TBufferedTransport(qa_socket));
		boost::shared_ptr<TProtocol> qa_protocol(new TBinaryProtocol(qa_transport));
		QAServiceClient qa_client(qa_protocol);
		
		boost::shared_ptr<TTransport> imm_socket(new TSocket("localhost", 8080));
		boost::shared_ptr<TTransport> imm_transport(new TBufferedTransport(imm_socket));
		boost::shared_ptr<TProtocol> imm_protocol(new TBinaryProtocol(imm_transport));
		ImageMatchingServiceClient imm_client(imm_protocol);

		if (data.audioData != "")
		{
			it = registeredServices.find("ASR");
			if (it != registeredServices.end())
			{
				boost::shared_ptr<TTransport> tmp_socket(
					new TSocket((*it).second.name, (*it).second.port)
				);
				boost::shared_ptr<TTransport> tmp_transport(new TBufferedTransport(tmp_socket));
				boost::shared_ptr<TProtocol> tmp_protocol(new TBinaryProtocol(tmp_transport));
				KaldiServiceClient tmp_client(tmp_protocol);
			
				asr_socket = tmp_socket;
				asr_transport = tmp_transport;
				asr_protocol = tmp_protocol;	
				asr_client = tmp_client;
				cout << "Selected " << (*it).second.name << ":" << (*it).second.port
				     << " for ASR server" << endl;
			}
			else
			{
				_return = "ASR requested, but not found";
				cout << _return << endl;
				return;
			}
		}
		/*if (data.textData != "")
		{*/
			it = registeredServices.find("QA");
			if (it != registeredServices.end())
			{
				boost::shared_ptr<TTransport> tmp_socket(
					new TSocket((*it).second.name, (*it).second.port)
				);
				boost::shared_ptr<TTransport> tmp_transport(new TBufferedTransport(tmp_socket));
				boost::shared_ptr<TProtocol> tmp_protocol(new TBinaryProtocol(tmp_transport));
				QAServiceClient tmp_client(tmp_protocol);

				qa_socket = tmp_socket;
				qa_transport = tmp_transport;
				qa_protocol = tmp_protocol;
				qa_client = tmp_client;
				cout << "Selected " << (*it).second.name << ":" << (*it).second.port
				     << " for QA server" << endl;

			}
			else
			{
				_return = "QA requested, but not found";
				cout << _return << endl;
				return;
			}
		//}
		if (data.imgData != "")
		{
			it = registeredServices.find("IMM");
			if (it != registeredServices.end())
			{
				boost::shared_ptr<TTransport> tmp_socket(
					new TSocket((*it).second.name, (*it).second.port)
				);
				boost::shared_ptr<TTransport> tmp_transport(new TBufferedTransport(tmp_socket));
				boost::shared_ptr<TProtocol> tmp_protocol(new TBinaryProtocol(tmp_transport));
				ImageMatchingServiceClient tmp_client(tmp_protocol);

				imm_socket = tmp_socket;
				imm_transport = tmp_transport;
				imm_protocol = tmp_protocol;
				imm_client = tmp_client;
				cout << "Selected " << (*it).second.name << ":" << (*it).second.port
				     << " for IMM server" << endl;
			}
			else
			{
				_return = "IMM requested, but not found";
				cout << _return << endl;
				return;
			}
		}

		//----Run pipeline----//
		std::string asrRetVal = "";
		std::string immRetVal = "";
		std::string question = "";
		//std::string answer = "";
		// TODO: use nlp libs to distinguish between voice cmd
		// and voice query
		if ((data.audioData != "") && (data.imgData != ""))
		{
			cout << "Starting ASR-IMM-QA pipeline..." << endl;
			//---Image matching
			imm_transport->open();
			imm_client.match_img(immRetVal, binary_img);
			imm_transport->close();
			cout << "IMG = " << immRetVal << endl;
			// image filename parsing
			try
			{
				immRetVal = parseImgFile(immRetVal);
			}
			catch (BadImgFileException)
			{
				cout << "Cmd Center: BadImgFileException" << endl;
				throw;
			}

			//---Speech recognition
			asr_transport->open();
			asr_client.kaldi_asr(asrRetVal, binary_audio);
			asr_transport->close();
			
			
			//---Question answer
			question = asrRetVal + " " + immRetVal;
			cout << "Your new question is: " << question << endl;
			qa_transport->open();
			qa_client.askFactoidThrift(_return, question);
			qa_transport->close();
		}
		else if (data.audioData != "")
		{
			cout << "Starting ASR-QA pipeline..." << endl;
			asr_transport->open();
			asr_client.kaldi_asr(asrRetVal, binary_audio);
			asr_transport->close();
	
			qa_transport->open();
			qa_client.askFactoidThrift(_return, asrRetVal);
			qa_transport->close();
		}
		/*else if (data.audioData != "")
		{
			asr_transport->open();
			asr_client.kaldi_asr(_return, binary_audio);
			asr_transport->close();
		}*/
		else if (data.textData != "")
		{
			qa_transport->open();
			qa_client.askFactoidThrift(_return, data.textData);
			qa_transport->close();
		}
		else if (data.imgData != "")
		{
			imm_transport->open();
			imm_client.match_img(_return, binary_img);
			imm_transport->close();
		}
		else
		{
			cout << "Nothing in the pipeline" << endl;
		}

	}

	virtual void askTextQuestion(std::string& _return, const std::string& question)
	{
		cout << "Command Center: askTextQuestion()" << endl;
		// TODO: this is hard-coded; make this extensible
		int serverPort = 9091;
		boost::shared_ptr<TTransport> socket(new TSocket("localhost", serverPort));
		boost::shared_ptr<TTransport> transport(new TBufferedTransport(socket));
		boost::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));
		QAServiceClient client(protocol);
		try
		{
			// Extract question from input
			std::string answer;

			transport->open();
			
			// Ask question
			client.askFactoidThrift(answer, question);
			
			// Report response
			cout << "Command Center forwarded the question successfully..." << endl;
			cout << "ANSWER = " << answer << endl;
			cout << endl;
			transport->close();
		}
		catch (TException &tx)
		{
			cout << "COMMAND CENTER ERROR: " << tx.what() << endl;
		}

	}

	void ping()
	{
		cout << "ping!" << endl;
	}

private:
	// registeredServices: a table of all servers that registered with
	// the command center via the registerService() method
	// TODO: this is a poor model, because it doesn't allow you to
	// select available servers easily, for a given key.
	std::multimap<std::string, MachineData> registeredServices;

	std::string parseImgFile(const std::string& immRetVal)
	{
		// Everything must be escaped twice
		const char *regexPattern = "\\A(/?)([\\w\\-]+/)*([\\w\\-]+)(\\.jpg)\\z";
		std::cout << "Passing the following pattern to regex engine: "
			<< regexPattern << std::endl;
		boost::regex re(regexPattern);
		std::string fmt("$3");
		std::string outstr = immRetVal;
		if (boost::regex_match(immRetVal, re))
		{
			std::cout << immRetVal << " matches pattern"  << std::endl;
			outstr = boost::regex_replace(immRetVal, re, fmt);
			std::cout << "Input: " << immRetVal << std::endl;
			std::cout << "Result: " << outstr << std::endl;
		}
		else
		{
			std::cout << "No match for " << immRetVal << "..." << std::endl;
			throw(BadImgFileException());
		}

		return outstr;
	}

/*	
	// command center's tables
	Service qa;
	Service imm;
	Service asr;
*/
	
};

int main(int argc, char **argv) {
	int port = 8081;
	if (argv[1])
	{
		port = atoi(argv[1]);
	}
	else
	{
		cout << "Command center port not specified; using default" << endl;
	}

	boost::shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory());
	boost::shared_ptr<CommandCenterHandler> handler(new CommandCenterHandler());
	boost::shared_ptr<TProcessor> processor(new CommandCenterProcessor(handler));
	/*TMultiplexedProcessor multiplexed_processor = new TMultiplexedProcessor();
	multiplexed_processor.registerProcessor(
		"cmdcenter",
		processor
	);*/

	boost::shared_ptr<TServerTransport> serverTransport(new TServerSocket(port));
	boost::shared_ptr<TTransportFactory> transportFactory(new TBufferedTransportFactory());

	// Initialize the command center server.
	// The server listens for packets transmitted via <transport> in <protocol> format.
	// The processor handles serialization/deserialization and communication w/ handler.
	//TSimpleServer server(processor, serverTransport, transportFactory, protocolFactory);
	//TSimpleServer server(multiplexed_processor, serverTransport, transportFactory, protocolFactory);


	// initialize the thread manager and factory
	boost::shared_ptr<ThreadManager> threadManager = ThreadManager::newSimpleThreadManager(THREAD_WORKS);
	boost::shared_ptr<PosixThreadFactory> threadFactory = boost::shared_ptr<PosixThreadFactory>(new PosixThreadFactory());
	threadManager->threadFactory(threadFactory);
	threadManager->start();

	// initialize the image matching server
	TThreadPoolServer server(processor, serverTransport, transportFactory, protocolFactory, threadManager);
	cout << "Starting the command center server on port " << port << "..." << endl;
	server.serve();
	cout << "Done." << endl;
	return 0;
}
