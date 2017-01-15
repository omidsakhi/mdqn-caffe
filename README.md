# Multi-Agent Deep Q Learning With Caffe

The goal of this project is to develop a multi-agent deep Q learning enviroment and front-end agents on the browser and Caffe as the back-end server.

Inspired by [ConvNetJS Deep Q Learning Demo](http://cs.stanford.edu/people/karpathy/convnetjs/demo/rldemo.html) and [muupan/dqn-in-the-caffe](https://github.com/muupan/dqn-in-the-caffe)

A part of this project is generated with [angular-cli](https://github.com/angular/angular-cli) version 1.0.0-beta.24.

![screen-shot](https://github.com/omidsakhi/mdqn-caffe/blob/master/screenshot.png)

# Status (WIP)

Both the front-end and the back-end of the project compile well (**albeit with much effort**). However, while everything works fine, there is still no sign of inteligence in the agents. With some love from the fans, we can debug it. All suggestions are welcome.

# Requirments

- [Caffe](https://github.com/BVLC/caffe/tree/windows), *commit: 8bb7cbc2162ded85802d61f83b5ac2881f21a7fd*
- [CUDA 8](https://developer.nvidia.com/cuda-toolkit)
- [cuDNN - cudnn-8.0-windows10-x64-v5.1](https://developer.nvidia.com/cudnn)
- [Node.js](https://nodejs.org/en/)
- [Angular CLI](https://cli.angular.io/)
- [socket.io-client-cpp](https://github.com/socketio/socket.io-client-cpp) *commit: 725a8e0e17ecead64574fd9879bd7029b0bf25fa*
- Visual Studio 2015

# Notes

- Windows is the only tested operating system.
- GPU is not tested.

# Developer's Guide

1. Download Caffe according to instructions and build it using \scripts\build_win.cmd
2. Install Node.js and Angular CLI
3. Build Socket.IO client for C++ (Also requires two libraries to be extracted in the specified location)
4. Open caffe-sever solution and refrence Caffe project inside the solution.
5. Correct all the library links and include folders according to your needs. (Have to really use CMAKE for this)
6. Build caffe_server.
7. In 'caffe_server\build\x64\Release' gather all the nessessary DLL files.
8. Copy Caffe model/solver files fomr 'caffe_server\models' to 'caffe-server\build\x64\Release'.
9. Get ready to run.

# To Run

0. Build caffe-server application.
1. Run 'npm install' to get node-modules folder.
2. Run 'ng build' to build the front-end.
3. From the dist folder run 'node server.js'.
4. Copy Caffe model/solver files from 'caffe_server\models' to 'caffe_server\build\x64\Release'.
5. From the 'caffe_server\build\x64\Release', run caffe_network.exe.
6. From the browser point to 'localhost:3000'.

# TODO

- To build the caffe-server solution/project using CMAKE.
- To debug the neural network logic.
- To improve caffe-server C++ program architecture. (threading and events)
- To use protobuf to send data between server and clients.
- To expand the commands for controlling Caffe network.
- To develop more debugging front-end tools like the ones that are developed using D3.
