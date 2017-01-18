import { Eye, SensedType } from './eye'
import { Vec } from './vec'
import { Food, FoodType } from './food'

export enum AgentState {
    Ready,
    Waiting
}
export class Action {
    constructor(
        public rotation1: number,
        public rotation2: number
    ) { }
}

export class Agent {
    public id: string;
    public state: AgentState = AgentState.Ready;
    public eyes: Eye[] = [];
    public actions: Action[];
    public digestionSignal: number = 0.0;
    public lastReward: number = 0.0;    
    public currentAction: Action = new Action(0.0, 0.0);    
    public oldInputs: number[] = [];
    public totalGoodFood : number = 0;
    public totalBadFood : number = 0;

    constructor(
        public name : string,
        public position: Vec,
        public radius: number = 10,
        public angle: number = 0,
        public epsilon: number = 0.5 
    ) {
        this.id = this.genId();
        this.actions = [
            new Action(1, 1),
            new Action(0.8, 1),
            new Action(1, 0.8),
            new Action(0.5, 0),
            new Action(0, 0.5)
        ]
        for (var k = 0; k < 9; k++)
            this.eyes.push(new Eye(k, (k - 3) * 0.25, 85));
    }

    genId(): string {
        return 'xxxxxxxxxxxxxxxx'.replace(/[xy]/g, function (c) { var r = Math.random() * 16 | 0, v = c == 'x' ? r : (r & 0x3 | 0x8); return v.toString(16); });
    }
    getInputs(): number[] {
        var inputArray = new Array<number>(this.eyes.length * 3);
        for (var i = 0; i < this.eyes.length; i++) {
            var e = this.eyes[i];
            inputArray[i * 3] = 1.0;
            inputArray[i * 3 + 1] = 1.0;
            inputArray[i * 3 + 2] = 1.0;
            if (e.sensedType !== SensedType.Nothing) {
                inputArray[i * 3 + (e.sensedType - 1)] = e.sensedProximity / e.maxRange;
            }
        }
        return inputArray;
    }
    eatFood(f:Food) {
        if (f.type == FoodType.Bad)
        {
            this.digestionSignal -= 7.0;
            this.totalBadFood ++;
        }
        else {
            this.digestionSignal += 5.0;    
            this.totalGoodFood ++;
        }        
    }
    reward(): number {
        var proximityReward = 0.0;
        for (let e of this.eyes)
            proximityReward += (e.sensedType == SensedType.Wall) ? (e.sensedProximity / e.maxRange) : 1.0;
        proximityReward = proximityReward / this.eyes.length;
        proximityReward = Math.min(1.0, proximityReward * 2);

        var forwardReward = 0.0;
        if ((this.currentAction.rotation1 == 1 && this.currentAction.rotation2 == 1) && proximityReward > 0.75)
            forwardReward = 0.1 * proximityReward;

        this.lastReward = proximityReward + forwardReward + this.digestionSignal;

        this.digestionSignal -= 0.1 * this.digestionSignal;        

        return this.lastReward;
    }

    eyeEndPoint(e: Eye): Vec {
        return new Vec(this.position.x + e.maxRange * Math.sin(this.angle + e.angle), this.position.y + e.maxRange * Math.cos(this.angle + e.angle));
    }

    applyAction(actionIndex: number) {
        this.currentAction = this.actions[actionIndex];
        var v = new Vec(0, this.radius / 2.0);
        v = v.rotate(this.angle + Math.PI / 2);
        var w1p = this.position.add(v);
        var w2p = this.position.sub(v);
        var vv = this.position.sub(w2p);
        vv = vv.rotate(-this.currentAction.rotation1);
        var vv2 = this.position.sub(w1p);
        vv2 = vv2.rotate(this.currentAction.rotation2);
        var np1 = w2p.add(vv).scale(0.5);
        var np2 = w1p.add(vv2).scale(0.5);
        this.position = np1.add(np2);
        this.angle -= this.currentAction.rotation1;
        if (this.angle < 0) this.angle += 2 * Math.PI;
        this.angle += this.currentAction.rotation2;
        if (this.angle > 2 * Math.PI) this.angle -= 2 * Math.PI;
    }
}
