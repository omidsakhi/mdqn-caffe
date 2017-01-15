import { Vec } from './vec'
export class Food {
    public age : number = 0;    
    constructor(
        public position: Vec,
        public radius : number,
        public bestBeforeAge : number,
        public expireAge : number
    ){}

    rancid() : Boolean {
        return this.age > this.bestBeforeAge;
    }
}
