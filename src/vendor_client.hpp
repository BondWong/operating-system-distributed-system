#ifndef VENDOR_CLIENT_HPP
#define VENDOR_CLIENT_HPP

#include <grpcpp/grpcpp.h>
#include <grpc/support/log.h>

#include "vendor.grpc.pb.h"
#include "store.grpc.pb.h"

// the idea is from https://github.com/grpc/grpc/blob/master/examples/cpp/helloworld/greeter_async_client2.cc

class VendorClient {
  public:
    VendorClient(std::shared_ptr<grpc::Channel> channel) : stub_(vendor::Vendor::NewStub(channel)) {}

    void getProductBid(const std::string& product_name) {
      vendor::BidQuery request;
      request.set_product_name(product_name);
      AsyncClientCall* call = new AsyncClientCall;

      call->response_reader = stub_->PrepareAsyncgetProductBid(&call->context, request, &cq_);
      call->response_reader->StartCall();
      call->response_reader->Finish(&call->reply, &call->status, (void*)call);
    }

    void AsyncCompleteRpc(store::ProductReply& product_reply) {
			void* got_tag;
			bool ok = false;
			store::ProductInfo* product_info;

			while (cq_.Next(&got_tag, &ok)) {
				AsyncClientCall* call = static_cast<AsyncClientCall*>(got_tag);
				GPR_ASSERT(ok);

				if (call->status.ok()) {
					std::cout << "Bid received: " << got_tag << " "
            << "vendor id " << call->reply.vendor_id() << " "
            << "price " << call->reply.price() << std::endl;
					// product_info = product_reply.add_products();
					// product_info->set_price(call->reply.price());
					// product_info->set_vendor_id(call->reply.vendor_id());
					// std::cout << "Added " << " "
          //   << product_info->vendor_id() << " "
          //   << product_info->price()
          //   << " size now " << product_reply.products_size() << std::endl;
				} else {
					std::cout << "RPC failed" << std::endl;
				}
				delete call;
			}
			std::cout << "About to exit " << product_reply.products_size() << std::endl;
		}

	private:
    // struct for keeping state and data information
    struct AsyncClientCall {
        // Container for the data we expect from the server.
        vendor::BidReply reply;
        grpc::ClientContext context;
        grpc::Status status;
        std::unique_ptr<grpc::ClientAsyncResponseReader<vendor::BidReply>> response_reader;
    };

    std::unique_ptr<vendor::Vendor::Stub> stub_;
    grpc::CompletionQueue cq_;
};

#endif /* VENDOR_CLIENT_HPP */
