#ifndef VENDOR_CLIENT_HPP
#define VENDOR_CLIENT_HPP

#include <grpcpp/grpcpp.h>
#include <grpc/support/log.h>

#include "vendor.grpc.pb.h"
#include "store.grpc.pb.h"

// the idea is from https://github.com/grpc/grpc/blob/master/examples/cpp/helloworld/greeter_async_client.cc

class VendorClient {
  public:
    VendorClient(std::shared_ptr<grpc::Channel> channel) : stub_(vendor::Vendor::NewStub(channel)) {}

    vendor::BidReply getProductBid(const std::string& product_name) {
      vendor::BidQuery request;
      request.set_product_name(product_name);

      vendor::BidReply reply;
      grpc::ClientContext context;
      grpc::Status status;
      std::unique_ptr<grpc::ClientAsyncResponseReader<vendor::BidReply>> rpc;

      rpc = stub_->PrepareAsyncgetProductBid(&context, request, &cq_);
      rpc->StartCall();
      rpc->Finish(&reply, &status, this);

      void* got_tag;
			bool ok = false;
      GPR_ASSERT(cq_.Next(&got_tag, &ok));
      GPR_ASSERT(got_tag == this);
      GPR_ASSERT(ok);

      if (status.ok()) {
  			std::cout << "Bid received: " << got_tag << " "
          << "vendor id " << reply.vendor_id() << " "
          << "price " << reply.price() << std::endl;
        return reply;
      }
      else exit(EXIT_FAILURE);
    }

	private:
    std::unique_ptr<vendor::Vendor::Stub> stub_;
    grpc::CompletionQueue cq_;
};

#endif /* VENDOR_CLIENT_HPP */
