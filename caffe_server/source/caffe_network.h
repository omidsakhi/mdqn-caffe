#pragma once

#include <random>
#include <caffe/caffe.hpp>
#include <caffe/sgd_solvers.hpp>
#include <caffe/solver.hpp>
#include <caffe/layers/memory_data_layer.hpp>
#include <caffe/layers/reshape_layer.hpp>

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

template <typename Dtype> std::string PrintBlobSize(const caffe::Blob<Dtype>& blob) {
	return "Num: " + std::to_string(blob.num()) + " Channels: " + std::to_string(blob.channels()) + " Height: " + std::to_string(blob.height()) + " Width: " + std::to_string(blob.width());	
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
	void Initialize(int input_layer_size, int output_layer_size, int minibatch_size, double gamma) {		
		std::lock_guard<std::mutex> lock(_mutex);		

		_input_layer_size = input_layer_size;
		_output_layer_size = output_layer_size;
		_minibatch_size = minibatch_size;		
		_gamma = gamma;		
		
		LOG(INFO) << "Initialize";		
		caffe::SolverParameter solver_param;
		caffe::ReadProtoFromTextFileOrDie(_solver_proto_filename, &solver_param);
		LOG_IF(FATAL, !solver_param.IsInitialized()) << "Failed to read solver prototxt.";
		_solver.reset(caffe::SolverRegistry<float>::CreateSolver(solver_param));
		_net = _solver->net();
		LOG_IF(FATAL, _net == NULL) << "Failed to create network.";

		_input_layer = boost::dynamic_pointer_cast<caffe::MemoryDataLayer<float>> (_net->layer_by_name("input_layer"));		
		LOG_IF(FATAL, _input_layer == NULL) << "Failed to get input_layer.";
		_target_layer = boost::dynamic_pointer_cast<caffe::MemoryDataLayer<float>> (_net->layer_by_name("target_layer"));
		LOG_IF(FATAL, _target_layer == NULL) << "Failed to get target_layer.";
	
		SetMinibatchSize(minibatch_size);
		
		auto _input_blob = _net->blob_by_name("input_layer_output");
		LOG_IF(FATAL, HasBlobSize(*_input_blob, _minibatch_size, 1, input_layer_size, 1) == false) << "Input layer size error " << PrintBlobSize(*_input_blob);
		
		auto _target_blob = _net->blob_by_name("target_layer_output");
		LOG_IF(FATAL, HasBlobSize(*_target_blob, _minibatch_size, output_layer_size, 1, 1) == false) << "Target layer size error " << PrintBlobSize(*_target_blob);

		_output_blob = _net->blob_by_name("network_output");		
		LOG_IF(FATAL, _output_blob == NULL) << "Failed to get network_output";
		LOG_IF(FATAL, HasBlobSize(*_output_blob, _minibatch_size, output_layer_size, 1, 1) == false) << "Output layer size error." << PrintBlobSize(*_output_blob);

		LOG(INFO) << "Initialize [Done]";		
		
	}
	void LoadTrainedModel(const std::string& model_bin_filename)
	{
		std::lock_guard<std::mutex> lock(_mutex);
		LOG(INFO) << "Loading Trained Model ...";		
		_net->CopyTrainedLayersFrom(model_bin_filename);				
	}
	void RestoreSolver(const std::string& solver_bin_filename) {
		std::lock_guard<std::mutex> lock(_mutex);
		LOG(INFO) << "Restoring Solver ...";		
		_solver->Restore(solver_bin_filename.c_str());		
	}
	void SaveTrainedModel(const std::string& model_bin_filename) {
		std::lock_guard<std::mutex> lock(_mutex);
		LOG(INFO) << "Saving Trained Model ...";				
		caffe::NetParameter net_param;
		_net->ToProto(&net_param);
		caffe::WriteProtoToBinaryFile(net_param, model_bin_filename);		
	}
	Action SelectAction(const std::vector<float> &input, double epsilon) {
		LOG_IF(FATAL, input.size() != _input_layer_size) << "Input size error.";
		std::lock_guard<std::mutex> lock(_mutex);
		Action action;
		if (std::uniform_real_distribution<>(0.0, 1.0)(_random_engine) < epsilon) {			
			action = (Action)std::uniform_int_distribution<int>(0, _output_layer_size-1)(_random_engine);
		}
		else {						
			std::vector<float> outputs = Predict(input,1).front();			
			action = (Action)std::distance(outputs.begin(), std::max_element(outputs.begin(), outputs.end()));
		}		
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
		std::lock_guard<std::mutex> lock(_mutex);
		LOG_IF(FATAL, t.currentState.size() != _input_layer_size) << "CurrentState size error.";
		LOG_IF(FATAL, t.nextState.size() != (_input_layer_size)) << "NextState size error.";
		if (_replay_memory.size() == _replay_memory_capacity)
			_replay_memory.pop_front();
		_replay_memory.push_back(t);	
		//PrintVector("CurrentState", t.currentState);
		//PrintVector("NextState", t.nextState);
		//LOG(INFO) << "REWARD " << t.reward << " ACTION " << t.action;
	}
	void Update() {
		std::lock_guard<std::mutex> lock(_mutex);

		std::vector<int> indicies(_minibatch_size);		
		for (auto i = 0; i < _minibatch_size; ++i)
			indicies[i] = std::uniform_int_distribution<int>(0, (int)_replay_memory.size() - 1) (_random_engine);

		std::vector<float> input_batch_state_1(_input_layer_size * _minibatch_size);
		std::vector<float> input_batch_state_2(_input_layer_size * _minibatch_size);		
		for (auto i = 0; i < indicies.size(); ++i)
		{
			auto idx = indicies[i];			
			for (int j = 0; j < _input_layer_size; ++j)
			{
				input_batch_state_1[i*_input_layer_size + j] = _replay_memory[idx].currentState[j];
				input_batch_state_2[i*_input_layer_size + j] = _replay_memory[idx].nextState[j];
			}
		}				
		std::vector<std::vector<float>> batch_results_1 = Predict(input_batch_state_1, _minibatch_size);
		std::vector<std::vector<float>> batch_results_2 = Predict(input_batch_state_2, _minibatch_size);					
		std::vector<float> target_batch(_output_layer_size * _minibatch_size);		
		std::fill(target_batch.begin(), target_batch.end(), 0.0f);		
		for (int i = 0; i < _minibatch_size; ++i)
		{
			const auto& transition = _replay_memory[indicies[i]];
			const auto action = transition.action;
			//LOG_IF(FATAL, static_cast<int> (action) >= _output_layer_size) << "Action " << action << " is greater than number of outputs (" << _output_layer_size << ").";
			const auto reward = transition.reward;			
			const std::vector<float> &outputs = batch_results_2[i];
			const auto max_idx = std::distance(outputs.begin(), std::max_element(outputs.begin(), outputs.end()));
			const auto target = reward + _gamma * outputs[max_idx];
			LOG_IF(FATAL, std::isnan(target));
			//LOG(INFO) << "REWARD " << reward << " OUTPUT " << outputs[max_idx] << " TARGET " << target << " I " << i;
			for (int j=0; j < _output_layer_size; ++j)
				target_batch[i * _output_layer_size + j] = batch_results_1[i][j];
			target_batch[i * _output_layer_size + static_cast<int>(action)] = (float) target;
		}
		LOG_IF(FATAL, input_batch_state_1.size() != _minibatch_size * _input_layer_size) << "InputLayerData " << input_batch_state_1.size() << " != BatchSize " << _minibatch_size << " * InputLayerSize" << _input_layer_size;
		LOG_IF(FATAL, target_batch.size() != _minibatch_size * _output_layer_size) << "TargetLayerData " << target_batch.size() << " != BatchSize " << _minibatch_size << " * OutputLayerSize" << _output_layer_size;
		_input_layer->Reset(const_cast<float*> (input_batch_state_1.data()), const_cast<float*> (input_batch_state_1.data()), _input_layer_size * _minibatch_size);
		_target_layer->Reset(const_cast<float*> (target_batch.data()), const_cast<float*> (target_batch.data()), _output_layer_size * _minibatch_size);
		_solver->Step(1);		
	}
	size_t ReplayMemorySize() {
		std::lock_guard<std::mutex> lock(_mutex);
		return _replay_memory.size();
	}
private:
	void SetMinibatchSize(int batch_size) {
		_input_layer->set_batch_size(batch_size);
		_target_layer->set_batch_size(batch_size);
		auto _input_blob = _net->blob_by_name("input_layer_output");
		auto _target_blob = _net->blob_by_name("target_layer_output");
		_input_blob->Reshape(batch_size, 1, _input_layer_size, 1);
		_target_blob->Reshape(batch_size, _output_layer_size, 1, 1);		
		_net->Reshape();
	}
	std::vector<std::vector<float>> Predict(const std::vector<float>& input_layer_data, int batch_size) {
		LOG_IF(FATAL, input_layer_data.size() != batch_size * _input_layer_size) << "InputLayerData " << input_layer_data.size() << " != BatchSize " << batch_size << " * InputLayerSize" << _input_layer_size ;
		SetMinibatchSize(batch_size);
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
	boost::shared_ptr<caffe::Blob<float>> _output_blob;	
	boost::shared_ptr<caffe::MemoryDataLayer<float>> _input_layer, _target_layer;	
	std::mt19937 _random_engine;
	int _input_layer_size;
	int _output_layer_size;
	int _minibatch_size;
	double _gamma;	
};
