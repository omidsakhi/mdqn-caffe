var express = require('express')
var app = express()
var http = require('http').Server(app);
var io = require('socket.io')(http);

app.use(express.static('public'))

app.get('/', function (req, res) {
	res.sendfile('index.html');
});

var caffe = null;
var agents = [];

function findAgent(agent_id) {
	for (var i = 0; i < agents.length; i++)
		if (agents[i].agent_id == agent_id)
			return agents[i];
	return null;
}

io.on('connection', function (socket) {
	console.log('a new connection established.');
	socket.on('agent', function (data) {
		console.log('a new agent ' + data.agent_id + ' joined.');
		var a = findAgent(data.agent_id);
		if (a === null) {
			agents.push({ "agent_id": data.agent_id, "socket": socket });
		}
	})
	socket.on('caffe', function () {
		console.log('caffe server joined.');
		caffe = socket;
	});
	socket.on('disconnect', function () {
		if (socket == caffe) {
			console.log("caffe server left.");
			caffe = null;
		}
		else {
			for (var i = agents.length - 1; i > -1; i--)
				if (agents[i].socket == socket) {
					console.log("agent " + agents[i].agent_id + " left");
					agents.splice(i, 1);
				}
		}
	});
	socket.on('server-data', function (data) {
		if (data.agent_id == "*") {
			if (agents.length > 0) {
				agents[0].socket.emit('server-data', data);
			}
		}
		else {
			var a = findAgent(data.agent_id);
			if (a !== null) {
				a.socket.emit('server-data', data);
			}
			else {
				console.log('no client/agent ' + data.agent_id + ' to send server data.');
			}
		}
	});
	socket.on('client-data', function (data) {
		if (caffe !== null) {
			caffe.emit('client-data', data);
		}
		else {
			console.log('no caffe to send client data ' + data.type);
		}
	})
});


http.listen(3000, function () {
	console.log('listening on *:3000');
});

