export class Vec {

    constructor(
    public x: number,
    public y: number,
    ){}

    distance(v:Vec) {return Math.sqrt(Math.pow(this.x-v.x,2) + Math.pow(this.y-v.y,2));}
    length() {return Math.sqrt(Math.pow(this.x,2) + Math.pow(this.y,2));}
    add(v:Vec) { return new Vec(this.x + v.x, this.y + v.y); }
    sub(v:Vec) { return new Vec(this.x - v.x, this.y - v.y); }
    rotate(a:number) { return new Vec(this.x * Math.cos(a) + this.y * Math.sin(a),-this.x * Math.sin(a) + this.y * Math.cos(a));}    
    scale(s:number) { return new Vec(this.x * s,this.y * s); }
    normalize() { return this.scale(1.0/this.length()); }
}
