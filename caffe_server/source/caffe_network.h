#pragma once

#include <random>
#include <caffe/caffe.hpp>
#include <caffe/sgd_solvers.hpp>
#include <caffe/solver.hpp>
#include <caffe/layers/memory_data_layer.hpp>

enum Action {
	GO_FORWARD,
	TURN_LEFT_SLOW,
	TURN_RIGHT_SLOW,
	TURN_LEFT_FAST,
	TURN_RIGHT_FAST
};

struct Transition {
	std::vector<float> currentState;
	Action action;
	double reward;
	std::vector<float> nextState;
};

template <typename Dtype> bool HasBlobSize(const caffe::Blob<Dtype>& blob, const int batch_size, const int channels, const int height, const int width) {
	return blob.num() == batch_size && blob.channels() == channels && blob.height() == height && blob.width() == width;
}

template <typename Dtype> void PrintBlobSize(const caffe::Blob<Dtype>& blob) {
	std::cout << "Num: " << blob.num() << " Channels: " << blob.channels() << " Height: " << blob.height() << " Width: " << blob.width() << std::endl;	
}

class CaffeNetwork {
public:
	CaffeNetwork(
		const std::string solver_proto_filename,
		const int replay_memory_capacity
	) :
		_solver_proto_filename(solver_proto_filename),
		_replay_memory_capacity(replay_memory_capacity)
	{
		LOG(INFO) << "CaffeNetwork::Constructor";
		LOG(INFO) << "Solver:" << _solver_proto_filename;		
	}
	void SetBrewMode(caffe::Caffe::Brew brew) {
		LOG(INFO) << "SetBrewMode";
		caffe::Caffe::set_mode(brew);
	}
	void Initialize(int input_layer_size, int output_layer_size, int minibatch_size, double gamma, double epsilon) {		
		_mutex.lock();

		_input_layer_size = input_layer_size;
		_output_layer_size = output_layer_size;
		_minibatch_size = minibatch_size;		
		_gamma = gamma;
		_epsilon = epsilon;
		
		LOG(INFO) << "Initialize";		
		caffe::SolverParameter solver_param;
		caffe::ReadProtoFromTextFileOrDie(_solver_proto_filename, &solver_param);
		LOG_IF(FATAL, !solver_param.IsInitialized()) << "Failed to read solver prototxt.";
		_solver.reset(caffe::SolverRegistry<float>::CreateSolver(solver_param));
		_net = _solver->net();		
		LOG_IF(FATAL, _net == NULL) << "Failed to create network.";

		_input_layer = boost::dynamic_pointer_cast<caffe::MemoryDataLayer<float>> (_net->layer_by_name("input_layer"));		
		LOG_IF(FATAL, _input_layer == NULL) << "Failed to get input_layer.";		
		_input_blob = _net->blob_by_name("input_layer_output");

		_target_layer = boost::dynamic_pointer_cast<caffe::MemoryDataLayer<float>> (_net->layer_by_name("target_layer"));
		LOG_IF(FATAL, _target_layer == NULL) << "Failed to get target_layer.";
		_target_blob = _net->blob_by_name("target_layer_output");
		
		_input_blob->Reshape(_minibatch_size, 1, _input_layer_size, 1);
		_target_blob->Reshape(_minibatch_size, _output_layer_size, 1, 1);
		_net->Reshape();				

		LOG_IF(FATAL, HasBlobSize(*_input_blob, _minibatch_size, 1, input_layer_size, 1) == false) << "Input layer size error.";
		LOG_IF(FATAL, HasBlobSize(*_target_blob, _minibatch_size, output_layer_size, 1, 1) == false) << "Target layer size error.";		

		_output_blob = _net->blob_by_name("ip3_relu_layer_output");
		LOG_IF(FATAL, HasBlobSize(*_output_blob, _minibatch_size, output_layer_size, 1, 1) == false) << "Output layer size error.";
		LOG_IF(FATAL, _output_blob == NULL) << "Failed to get ip3_layer_output";
		LOG(INFO) << "Initialize [Done]";		

		_mutex.unlock();
	}
	void LoadTrainedModel(const std::string& model_bin_filename)
	{
		LOG(INFO) << "Loading Trained Model ...";
		_mutex.lock();
		_net->CopyTrainedLayersFrom(model_bin_filename);		
		_mutex.unlock();
	}
	void RestoreSolver(const std::string& solver_bin_filename) {
		LOG(INFO) << "Restoring Solver ...";
		_mutex.lock();
		_solver->Restore(solver_bin_filename.c_str());
		_mutex.unlock();
	}
	void SaveTrainedModel(const std::string& model_bin_filename) {
		LOG(INFO) << "Saving Trained Model ...";		
		_mutex.lock();		
		caffe::NetParameter net_param;
		_net->ToProto(&net_param);
		caffe::WriteProtoToBinaryFile(net_param, model_bin_filename);
		_mutex.unlock();
	}
	Action SelectAction(const std::vector<float> &input) {
		_mutex.lock();
		//LOG_IF(FATAL, _epsilon < 0.0 || _epsilon > 1.0) << "Epsilon " << _epsilon << " is out of range.";		
		Action action;
		if (std::uniform_real_distribution<>(0.0, 1.0)(_random_engine) < _epsilon) {
			//LOG(INFO) << "SelectAction - Random";
			action = (Action)std::uniform_int_distribution<int>(0, _output_layer_size-1)(_random_engine);
		}
		else {			
			//LOG(INFO) << "SelectAction - Greedy";
			std::vector<std::vector<float>> samples;
			samples.push_back(input);
			std::vector<float> outputs = SelectActionGreedily(samples).front();			
			action = (Action)std::distance(outputs.begin(), std::max_element(outputs.begin(), outputs.end()));
		}
		//LOG_IF(FATAL, static_cast<int> (action) >= _output_layer_size) << "Action " << action << " is greater than number of outputs (" << _output_layer_size << ").";
		_mutex.unlock();
		return action;
	}	
	void PrintVector(std::string prefix, const std::vector<float> &vec) {
		std::ostringstream buf;		
		for (auto i = 0; i < vec.size(); ++i) {
			buf << std::to_string(vec[i]) << " ";			
		}
		LOG(INFO) << prefix << " " << buf.str();		
	}
	void AddTransition(const Transition &t) {
		if (_replay_memory.size() == _replay_memory_capacity)
			_replay_memory.pop_front();
		_replay_memory.push_back(t);	
		//PrintVector("CurrentState", t.currentState);
		//PrintVector("NextState", t.nextState);
		//LOG(INFO) << "REWARD " << t.reward << " ACTION " << t.action;
	}
	void Update() {
		_mutex.lock();		

		std::vector<int> indicies(_minibatch_size);		
		for (auto i = 0; i < _minibatch_size; ++i)
			indicies[i] = std::uniform_int_distribution<int>(0, (int)_replay_memory.size() - 1) (_random_engine);

		std::vector<float> input_batch(_input_layer_size * _minibatch_size);
		for (auto i = 0; i < indicies.size(); ++i)
		{			
			std::vector<float> &state = _replay_memory[indicies[i]].nextState;
			for (int j = 0; j < state.size(); ++j)
				input_batch[i*_input_layer_size + j] = state[j];
		}		
		//PrintVector("InputBatchNextState", input_batch);
		std::vector<std::vector<float>> batch_results = SelectActionGreedily(input_batch, _minibatch_size);					
		std::vector<float> target_batch(_output_layer_size * _minibatch_size);
		for (int i = 0; i < _minibatch_size; ++i)
		{
			const auto& transition = _replay_memory[indicies[i]];
			const auto action = transition.action;
			//LOG_IF(FATAL, static_cast<int> (action) >= _output_layer_size) << "Action " << action << " is greater than number of outputs (" << _output_layer_size << ").";
			const auto reward = transition.reward;			
			const std::vector<float> &outputs = batch_results[i];
			const auto max_idx = std::distance(outputs.begin(), std::max_element(outputs.begin(), outputs.end()));
			const auto target = reward + _gamma * outputs[max_idx];
			//LOG(INFO) << "REWARD " << reward << " GAMMA " << _gamma << " OUTPUT " << outputs[max_idx] << " MAXID " << max_idx << " TARGET " << target << " I " << i;
			//LOG_IF(FATAL, std::isnan(target));
			for (int j = 0; j < _output_layer_size; ++j)
				target_batch[i * _output_layer_size + j] = outputs[j];
			target_batch[i * _output_layer_size + static_cast<int>(action)] = (float) target;
			const std::vector<float> &state = transition.currentState;
			for (int j = 0; j < state.size(); ++j)
				input_batch[i * _input_layer_size + j] = state[j];
		}
		//PrintVector("InputBatchCurrentState", input_batch);
		//PrintVector("TargetBatch", target_batch);
		LOG_IF(FATAL, input_batch.size() != _minibatch_size * _input_layer_size) << "InputLayerData " << input_batch.size() << " != BatchSize " << _minibatch_size << " * InputLayerSize" << _input_layer_size;
		LOG_IF(FATAL, target_batch.size() != _minibatch_size * _output_layer_size) << "TargetLayerData " << target_batch.size() << " != BatchSize " << _minibatch_size << " * OutputLayerSize" << _output_layer_size;
		_input_layer->Reset(const_cast<float*> (input_batch.data()), const_cast<float*> (input_batch.data()), _input_layer_size * _minibatch_size);
		_target_layer->Reset(const_cast<float*> (target_batch.data()), const_cast<float*> (target_batch.data()), _output_layer_size * _minibatch_size);
		_solver->Step(1);
		_mutex.unlock();
	}
	size_t ReplayMemorySize() const {
		return _replay_memory.size();
	}
	void SetEpsilon(double epsilon) {
		LOG(INFO) << "SetEpsilon " << epsilon;
		_epsilon = epsilon;
	}
private:
	std::vector<std::vector<float>> SelectActionGreedily(const std::vector<std::vector<float>> &samples) {
		std::vector<float> input_layer_data;
		for (int i = 0; i < samples.size(); ++i)
		{
			LOG_IF(FATAL, samples[i].size() != _input_layer_size);
			for (int j = 0; j < _input_layer_size; ++j)
				input_layer_data.push_back(samples[i][j]);
		}
		return SelectActionGreedily(input_layer_data, (int) samples.size());
	}
	std::vector<std::vector<float>> SelectActionGreedily(const std::vector<float>& input_layer_data, int batch_size) {
		LOG_IF(FATAL, input_layer_data.size() != batch_size * _input_layer_size) << "InputLayerData " << input_layer_data.size() << " != BatchSize " << batch_size << " * InputLayerSize" << _input_layer_size ;
		_input_layer->set_batch_size(batch_size);		
		_target_layer->set_batch_size(batch_size);
		_net->Reshape();				
		_input_layer->Reset(const_cast<float*>(input_layer_data.data()), const_cast<float*>(input_layer_data.data()), (int) input_layer_data.size());		
		_target_layer->Reset(const_cast<float*>(input_layer_data.data()), const_cast<float*>(input_layer_data.data()), batch_size * _output_layer_size);						
		_net->Forward(nullptr);		
		std::vector<std::vector<float>> results;
		results.reserve(batch_size);
		for (int j = 0; j < batch_size; j++)
		{
			std::vector<float> outputs(_output_layer_size);
			for (int i = 0; i < _output_layer_size; i++)
			{
				const auto q = _output_blob->data_at(j, i, 0, 0);				
				LOG_IF(FATAL, std::isnan(q));				
				outputs[i] = q;
			}
			results.push_back(outputs);
		}
		return results;
	}
private:
	std::mutex _mutex;	
	bool _exit = false;
	const int _replay_memory_capacity;
	std::deque<Transition> _replay_memory;
	std::string _solver_proto_filename;
	std::shared_ptr<caffe::Solver<float>> _solver;	
	boost::shared_ptr<caffe::Net<float>> _net;
	boost::shared_ptr<caffe::Blob<float>> _input_blob, _output_blob,_target_blob;	
	boost::shared_ptr<caffe::MemoryDataLayer<float>> _input_layer, _target_layer;	
	std::mt19937 _random_engine;
	int _input_layer_size;
	int _output_layer_size;
	int _minibatch_size;
	double _gamma;
	double _epsilon;	
};
