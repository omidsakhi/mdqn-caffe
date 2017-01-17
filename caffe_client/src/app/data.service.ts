import { Injectable } from '@angular/core';
import { Agent } from './agent';

declare var io: any;

@Injectable()
export class DataService {
  public socket : any;
  constructor() { 
    this.socket = io();
  }
  
  registerAgent(agent : Agent) {
    this.socket.emit('agent', {
      agent_id: agent.id      
    })
  }

  sendInputs(agent : Agent, inputs: number[]) {
    this.socket.emit('client-data', {
      agent_id : agent.id,
      type : "input",
      input : inputs,
      epsilon : agent.epsilon
    })
  }
  
  backupNetwork() {
    this.socket.emit('client-data', {
      type : "snapshot"      
    })
  }

  sendTransition(agent: Agent,state:number[],actionIndex:number,reward:number,afterState:number[]) {
    this.socket.emit('client-data', {
      agent_id: agent.id,
      type: "transition",
      state: state,
      action_index: actionIndex,
      reward: reward,
      after_state: afterState
    });
  }

}
