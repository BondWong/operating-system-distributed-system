#include "threadpool.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <functional>

#include <grpcpp/grpcpp.h>
#include <grpc/support/log.h>

#include "vendor_client.hpp"
#include "store.grpc.pb.h"

const std::string VENDOR_ADDRESSES = "./vendor_addresses.txt";

// The CLassData class idea is from
// https://github.com/grpc/grpc/blob/v1.15.0/examples/cpp/helloworld/greeter_async_server.cc

class StoreServer {
public:
	StoreServer(std::string server_addr, int max_num)
		: server_address(server_addr), num_max_threads(max_num) {
			thread_pool = new threadpool(num_max_threads);
		};

	~StoreServer() {
		server_->Shutdown();
		cq_->Shutdown();
	}

	void run() {
		grpc::ServerBuilder builder;
		builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
		builder.RegisterService(&service_);
		cq_ = builder.AddCompletionQueue();
		server_ = builder.BuildAndStart();
		std::cout << "Server listening on " << server_address << " threads " << num_max_threads << std::endl;
		// Proceed to the server's main loop.
		handleRpcs();
	}

private:
	threadpool* thread_pool;

	std::string server_address;
	int num_max_threads;

	std::unique_ptr<grpc::ServerCompletionQueue> cq_;
  store::Store::AsyncService service_;
  std::unique_ptr<grpc::Server> server_;

  class CallData {
  public:
    CallData(store::Store::AsyncService* service, grpc::ServerCompletionQueue* cq)
        : service_(service), cq_(cq), responder_(&ctx_), status_(CREATE) {
      Proceed();
    }

    void Proceed() {
      if (status_ == CREATE) {
        status_ = PROCESS;
        service_->RequestgetProducts(&ctx_, &request_, &responder_, cq_, cq_, this);
      } else if (status_ == PROCESS) {
				std::ifstream address_file (VENDOR_ADDRESSES);
				if (!address_file.is_open()) {
					std::cerr << "Failed to open file " << VENDOR_ADDRESSES << std::endl;
					exit(EXIT_FAILURE);
				}

				std::string ip_addr;
				while (getline(address_file, ip_addr)) ip_addresses.push_back(ip_addr);
				address_file.close();

				if (ip_addresses.size() == 0) {
					std::cerr << "No vendors found in ./vendor_addresses.txt" << std::endl;
					exit(EXIT_FAILURE);
				}

        // Spawn a new CallData instance to serve new clients while we process
        // the one for this CallData. The instance will deallocate itself as
        // part of its FINISH state.
        new CallData(service_, cq_);

				for (auto ip_addr: ip_addresses) {
					VendorClient vc = VendorClient(grpc::CreateChannel(ip_addr, grpc::InsecureChannelCredentials()));
					vendor::BidReply bid_reply_ = vc.getProductBid(request_.product_name());
				}

				status_ = FINISH;
				responder_.Finish(reply_, grpc::Status::OK, this);
			} else {
				GPR_ASSERT(status_ == FINISH);
				// Once in the FINISH state, deallocate ourselves (CallData).
				delete this;
			}
		}

	private:
		store::Store::AsyncService* service_;
		grpc::ServerCompletionQueue* cq_;
		grpc::ServerContext ctx_;

		threadpool* thread_pool;
		std::vector<std::string> ip_addresses;

		store::ProductQuery request_;
		store::ProductReply reply_;
		grpc::ServerAsyncResponseWriter<store::ProductReply> responder_;

		// Let's implement a tiny state machine with the following states.
		enum CallStatus { CREATE, PROCESS, FINISH };
		CallStatus status_;  // The current serving state.
	};

	void handleRpcs() {
		new CallData(&service_, cq_.get());
		void* tag;
		bool ok;
		while (true) {
			GPR_ASSERT(cq_->Next(&tag, &ok));
			GPR_ASSERT(ok);
			threadpool::Runnable job;
			job.run = std::bind(&CallData::Proceed, static_cast<CallData*>(tag));
			thread_pool->execute(job);
		}
	}
};


int main(int argc, char** argv) {
	// command line handling copied from provided test harness
	int num_max_threads;
	std::string server_addr;

	if (argc != 3) {
		std::cerr << "To run this command: ./store host:port num_max_threads" << std::endl;
		exit(EXIT_FAILURE);
	}

	server_addr = std::string(argv[1]);
	num_max_threads = std::min( 20, std::max(0,atoi(argv[2])));
	StoreServer storeServer(server_addr, num_max_threads);
	storeServer.run();

	return EXIT_SUCCESS;
}
