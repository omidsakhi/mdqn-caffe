#pragma once

#include "application.h"
#include <mutex>
#include <string>
#include <iostream>
#include "src/sio_client.h"
#include <condition_variable>
#include "caffe_network.h"

enum ConnectionStatus {
	NoConnection,
	Connected,
	Failed,
	Closed
};

class Connection {
private:
	void OnConnected()
	{
		_mutex.lock();
		_cond.notify_all();				
		_socket = _handler.socket();
		_status = ConnectionStatus::Connected;		
		_mutex.unlock();
		bindToNetwork();
	}

	void OnClose(sio::client::close_reason const& reason)
	{
		std::cout << "Connection closed." << std::endl;
		_status = ConnectionStatus::Closed;
		Application::Exit();
	}

	void OnFail()
	{
		std::cout << "Connection faild." << std::endl;
		_status = ConnectionStatus::Failed;
		Application::Exit();
	}
	void bindToNetwork() {
		_socket->emit("caffe", std::string());
		_socket->on("client-data", sio::socket::event_listener_aux([&](std::string const& name, sio::message::ptr const& data, bool isAck, sio::message::list &ack_resp) {
			sio::message::ptr answer;
			_mutex.lock();			
			std::string type = data->get_map()["type"]->get_string();
			if (type == "input") {
				std::string id = data->get_map()["agent_id"]->get_string();
				answer = sio::object_message::create();
				answer->get_map()["agent_id"] = sio::string_message::create(id);
				answer->get_map()["type"] = sio::string_message::create("action");
				auto a = data->get_map()["input"]->get_vector();
				std::vector<float> inputs(27);
				std::transform(a.begin(), a.end(), inputs.begin(), [&](auto m) { return m->get_double(); });
				answer->get_map()["action_index"] = sio::int_message::create(_cnet.SelectAction(inputs));
			}
			else if (type == "transition") {
				Transition t;
				t.currentState.resize(27);
				t.nextState.resize(27);
				auto sio_state = data->get_map()["state"]->get_vector();				
				std::transform(sio_state.begin(), sio_state.end(), t.currentState.begin(), [&](auto m) { return m->get_double(); });
				auto sio_after_state = data->get_map()["after_state"]->get_vector();				
				std::transform(sio_after_state.begin(), sio_after_state.end(), t.nextState.begin(), [&](auto m) { return m->get_double(); });				
				t.action = (Action) data->get_map()["action_index"]->get_int();
				t.reward = data->get_map()["reward"]->get_double();				
				_cnet.AddTransition(t);
			}
			else if (type == "epsilon")	{
				double epsilon = data->get_map()["epsilon"]->get_double();
				_cnet.SetEpsilon(epsilon);
			}
			else if (type == "snapshot") {
				time_t rawtime;
				struct tm timeinfo;
				char buffer[80];
				time(&rawtime);
				localtime_s(&timeinfo, &rawtime);
				strftime(buffer, 80, "%Y%m%d%I%M%S", &timeinfo);
				std::string date(buffer);
				_cnet.SaveTrainedModel(date + "-network.bin");
			}
			_mutex.unlock();
			if (answer)
				_socket->emit("server-data", answer);
		}));
	}
public:
	Connection(CaffeNetwork &cnet) : _cnet(cnet) {
		_handler.set_open_listener(std::bind(&Connection::OnConnected,this));
		_handler.set_close_listener(std::bind(&Connection::OnClose, this, std::placeholders::_1));
		_handler.set_fail_listener(std::bind(&Connection::OnFail,this));
	}
	void Open(std::string url) {
		_handler.connect(url);
		_mutex.lock();
		if (_status != ConnectionStatus::Connected)
			_cond.wait(_mutex);
		_mutex.unlock();				
	}
	void Close() {
		_handler.sync_close();
		_handler.clear_con_listeners();
	}
	bool IsConnected() {
		return _status == ConnectionStatus::Connected;
	}

	ConnectionStatus Status() {
		return _status;
	}

	sio::socket::ptr Socket() {
		return _socket;
	}

	void Lock() {
		_mutex.lock();
	}

	void Unlock() {
		_cond.notify_all();
		_mutex.unlock();
	}
private:
	sio::client _handler;
	std::mutex _mutex;
	ConnectionStatus _status = ConnectionStatus::NoConnection;
	std::condition_variable_any _cond;
	sio::socket::ptr _socket = NULL;
	CaffeNetwork &_cnet;

};