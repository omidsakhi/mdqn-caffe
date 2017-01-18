
#include "application.h"
#include "connection.h"
#include "caffe_network.h"

#include <gflags/gflags.h>
#include <glog/logging.h>

DEFINE_bool(gpu, false, "Use GPU");
DEFINE_bool(evaluate, false, "Evaluation only");
DEFINE_int32(device, -1, "GPU device");
DEFINE_string(model, "", "Model file to load");
DEFINE_string(solver, "solver.prototxt", "Solver parameter file (*.prototxt)");
DEFINE_int32(memory, 10000, "Capacity of replay memory");
DEFINE_double(gamma, 0.7, "Discount factor of future rewards (0,1]");
DEFINE_int32(learn, 300, "Stat learning threshold");
DEFINE_int32(batch, 64, "Batch size");

int main(int argc, char** argv)
{
	gflags::ParseCommandLineFlags(&argc, &argv, true);
	google::SetLogDestination(google::GLOG_INFO, "log-");	
	FLAGS_alsologtostderr = true;
	google::InitGoogleLogging(argv[0]); 
		

	CaffeNetwork cnet(FLAGS_solver,FLAGS_memory);
	cnet.Initialize(27,5,FLAGS_batch,FLAGS_gamma);

	if (FLAGS_gpu)
		cnet.SetBrewMode(caffe::Caffe::GPU);
	else
		cnet.SetBrewMode(caffe::Caffe::CPU);

	if (!FLAGS_model.empty())
	{		
		std::cout << "Loading " << FLAGS_model << std::endl;
		cnet.LoadTrainedModel(FLAGS_model);
	}

	Application app;

	Connection connection(cnet);
	connection.Open("http://127.0.0.1:3000");		

	while (true)
	{
		if (FLAGS_evaluate == false &&  cnet.ReplayMemorySize() > FLAGS_learn)
		{
			cnet.Update();			
			std::this_thread::sleep_for(std::chrono::microseconds(10));
		}
		else
			std::this_thread::sleep_for(std::chrono::seconds(1));
	}


	connection.Close();

	return 0;
}