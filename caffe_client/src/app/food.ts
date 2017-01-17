import { Vec } from './vec'

export enum FoodType {
    Good,
    Bad
}

export class Food {
    public age : number = 0;    
    constructor(
        public position: Vec,
        public radius : number,
        public type: FoodType
    ){}

}
