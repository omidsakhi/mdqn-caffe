export enum SensedType {
    Nothing,
    Wall,
    GoodFood,
    BadFood
}
export class Eye {
    public sensedProximity: number;
    public sensedType: SensedType = SensedType.Nothing;
    constructor(
        public num: number,
        public angle: number,
        public maxRange: number
    ) {
        this.sensedProximity = this.maxRange;
    }
}
